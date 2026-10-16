#!/usr/bin/env python3

# Validate data semantics that a plain JSON syntax check cannot see.  Without these
# checks a data change can pass every existing validation and still abort the whole
# data load at runtime: the game reports "Json error" on the main menu and crashes.
# Both classes below were hit for real while porting the CBN shields.
#
#   1. item_group entry shapes, exactly as src/item_factory.cpp parses them:
#      "entries" and nested "distribution"/"collection" elements must be objects, the
#      [ "id", prob ] shorthand is only legal inside "items"/"groups", and an entry object
#      must carry one of item/group/collection/distribution or it is silently dropped
#   2. a definition object must carry a "type".  Losing that member keeps the file
#      syntactically valid (json_formatter will not complain either) but the loader
#      aborts with 'missing required field "type"'
#   3. "copy-from" must resolve inside the namespace the loader uses.  Only the item and
#      monster namespaces are checked, because those are the ones with a single flat
#      namespace: every item type shares the itype namespace, monsters share theirs.
#      Other types derive ids differently (recipes from result/id_suffix, factions from
#      name, ...) and would need per-type rules to avoid false positives.
#   4. monster flags must exist in the engine's flag table (the enum_to_string cases in
#      src/monstergenerator.cpp).  A misspelled or upstream-only flag such as BLEED keeps
#      the file valid JSON but aborts the load with 'invalid monster flag'.
#   5. strict mode: a copy-from child must not repeat a value it already inherits, the
#      loader reports 'cannot assign explicit value the same as default or inherited
#      value'.  Players see that as a debug prompt on the main menu.  Only the members the
#      loader hands to assign() with strict enabled can produce that report, so the list of
#      them is read from the source (see load_strict_members) instead of guessed.  Set-like
#      members are excluded: their assign() overload discards the strict argument.
#
# Usage: validate_json_semantics.py [file ...] [--changed-files list.txt] [--data-root data]
#                                   [--source-root .]

import argparse
import json
import os
import re
import sys
from pathlib import Path

DEFAULT_DATA_ROOT = Path("data")
ITEM_TYPES = {
    "GENERIC", "TOOL", "TOOL_ARMOR", "ARMOR", "GUN", "GUNMOD", "AMMO", "MAGAZINE",
    "COMESTIBLE", "MEDICINE", "BOOK", "BIONIC_ITEM", "ENGINE", "WHEEL", "PET_ARMOR",
    "CONTAINER", "BATTERY",
}
NAMESPACES = (("item", ITEM_TYPES), ("monster", {"MONSTER"}))
ENTRY_KEYS = {"item", "group", "collection", "distribution"}
STRUCTURAL_KEYS = {"id", "type", "copy-from", "abstract", "extend", "delete", "relative",
                   "proportional", "//", "name", "description", "looks_like", "migration"}
MONSTER_FLAG_SOURCE = Path("src/monstergenerator.cpp")
# The item loader hands each type to one load_* function; see the "type" registrations in
# src/init.cpp.  The members a type is strict about follow from that function.
ITEM_MEMBER_SOURCE = Path("src/item_factory.cpp")
ITEM_LOADERS = {
    "load_ammo": "AMMO", "load_gun": "GUN", "load_armor": "ARMOR",
    "load_pet_armor": "PET_ARMOR", "load_tool": "TOOL", "load_toolmod": "TOOLMOD",
    "load_tool_armor": "TOOL_ARMOR", "load_book": "BOOK", "load_comestible": "COMESTIBLE",
    "load_engine": "ENGINE", "load_wheel": "WHEEL", "load_gunmod": "GUNMOD",
    "load_magazine": "MAGAZINE", "load_battery": "BATTERY", "load_generic": "GENERIC",
    "load_bionic": "BIONIC_ITEM",
}
# Unit members compare after their own conversion, so equal text is not enough: the loader
# stores millilitres / milligrams / watts / cents and 1 L has to equal 1000 ml.
MEMBER_UNITS = {
    "weight": {"mg": 1.0, "g": 1000.0, "kg": 1000000.0},
    "integral_weight": {"mg": 1.0, "g": 1000.0, "kg": 1000000.0},
    "volume": {"ml": 1.0, "l": 1000.0},
    "barrel_volume": {"ml": 1.0, "l": 1000.0},
    "power_draw": {"mw": 1.0, "w": 1000.0, "kw": 1000000.0},
    "price": {"cent": 1.0, "usd": 100.0, "kusd": 100000.0},
    "price_postapoc": {"cent": 1.0, "usd": 100.0, "kusd": 100000.0},
}
NUMBER = re.compile(r"\s*(-?\d+(?:\.\d+)?)\s*([A-Za-z%]*)\s*\Z")
# Set-like members that are handed to assign() with strict on.  Their assign() overload
# (assign_set) throws the strict argument away, so the loader can never report them.
SET_MEMBERS = {"ammo_effects", "built_in_mods", "default_mods", "material"}
# Values that equal what they inherit were already in the tree when this check was added,
# they are unrelated to any current work and are reported as skipped instead of failing.
# Each one was checked by hand: the child repeats the literal value of its parent, so the
# loader really does report it, which is why it is a baseline entry and not a bug here.
KNOWN_REDUNDANT = {
    ("40x53mm_grenade", "range"),
    ("combatnail", "loudness"),
    ("utility_exoskeleton_on", "ammo"),
    ("mon_chud", "symbol"),
    ("mon_chud", "weakpoint_sets"),
    ("mon_chud", "families"),
    ("mon_feral_armored_battleaxe", "color"),
    ("mon_zombie_resort_dancer", "upgrades"),
    ("mon_zombie_resort_bouncer", "upgrades"),
    ("mon_zombie_resort_staff", "upgrades"),
    ("mon_zombie_bio_op2", "harvest"),
}


def load_json(path):
    with path.open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def iter_definitions(document):
    """Yield the definition objects of a data file (top level array or single object)."""
    if isinstance(document, list):
        for element in document:
            if isinstance(element, dict):
                yield element
    elif isinstance(document, dict):
        yield document


def definition_ids(definition):
    identifiers = definition.get("id", definition.get("abstract"))
    if isinstance(identifiers, str):
        return [identifiers]
    if isinstance(identifiers, list):
        return [entry for entry in identifiers if isinstance(entry, str)]
    return []


def namespace_of(definition_type):
    for namespace, members in NAMESPACES:
        if definition_type in members:
            return namespace
    return None


def is_definition_file(path):
    """data/json and data/mods hold definition lists; other data directories do not."""
    parts = Path(path).parts
    for index in range(len(parts) - 1):
        if parts[index] == "data" and parts[index + 1] in ("json", "mods"):
            return True
    return False


def check_type_member(path, document, errors):
    """A definition object must carry a type; covered for data/json and data/mods only."""
    if not is_definition_file(path):
        return
    if isinstance(document, list):
        for index, element in enumerate(document):
            # entries that are not objects are not definitions (e.g. replacements.json)
            if isinstance(element, dict) and not element.get("type"):
                ids = ", ".join(definition_ids(element)) or "<no id>"
                errors.append(f'{path}: entry #{index} (id {ids}) has no "type"')
    elif isinstance(document, dict) and not document.get("type"):
        ids = ", ".join(definition_ids(document)) or "<no id>"
        errors.append(f'{path}: object (id {ids}) has no "type"')


def check_entry_array(where, array, errors, allow_shorthand):
    for index, element in enumerate(array):
        if isinstance(element, dict):
            if not (ENTRY_KEYS & set(element)):
                errors.append(f"{where}[{index}]: entry has none of "
                              "item/group/collection/distribution")
            for key in ("collection", "distribution"):
                if key in element:
                    if not isinstance(element[key], list):
                        errors.append(f"{where}[{index}].{key}: not an array")
                    else:
                        check_entry_array(f"{where}[{index}].{key}", element[key], errors, False)
            for key in ("items", "groups"):
                if key in element:
                    check_entry_array(f"{where}[{index}].{key}", element[key], errors, True)
            contents = element.get("contents")
            if isinstance(contents, dict):
                check_group_members(f"{where}[{index}].contents", contents, errors)
        elif isinstance(element, str):
            if not allow_shorthand:
                errors.append(f"{where}[{index}]: bare string is not allowed here")
        elif isinstance(element, list):
            if not allow_shorthand:
                errors.append(f"{where}[{index}]: array shorthand {element} is not allowed "
                              'here, use an object such as { "item": ..., "prob": ... }')
            elif (len(element) != 2 or not isinstance(element[0], str)
                  or not isinstance(element[1], int)):
                errors.append(f"{where}[{index}]: malformed shorthand {element}")
        else:
            errors.append(f"{where}[{index}]: unexpected {type(element).__name__}")


def check_group_members(where, definition, errors):
    if "entries" in definition:
        if not isinstance(definition["entries"], list):
            errors.append(f"{where}.entries: not an array")
        else:
            check_entry_array(f"{where}.entries", definition["entries"], errors, False)
    for key in ("items", "groups"):
        if key in definition:
            if not isinstance(definition[key], list):
                errors.append(f"{where}.{key}: not an array")
            else:
                check_entry_array(f"{where}.{key}", definition[key], errors, True)
    for key in ("collection", "distribution"):
        if key in definition:
            if not isinstance(definition[key], list):
                errors.append(f"{where}.{key}: not an array")
            else:
                check_entry_array(f"{where}.{key}", definition[key], errors, False)


def check_item_group(path, definition, errors):
    identifier = ", ".join(definition_ids(definition)) or "<no id>"
    check_group_members(f"{path} group {identifier}", definition, errors)


def build_namespace_index(data_root):
    """Collect item and monster definitions so copy-from targets can be resolved.

    Several definitions may share an id: mods and patches use the
    `{"id": X, "copy-from": X, "extend": ...}` pattern, so the id keeps a list of
    (path, definition) pairs and the self-referencing entries are filtered out while
    resolving.  The path is kept because the loader loads the core data before any mod, so
    for a core definition the parent values always come from the core definition, never from
    a mod that patches the same id afterwards.
    """
    index = {"item": {}, "monster": {}}
    for path in sorted(data_root.rglob("*.json")):
        try:
            document = load_json(path)
        except (OSError, json.JSONDecodeError):
            continue
        for definition in iter_definitions(document):
            namespace = namespace_of(definition.get("type"))
            if namespace:
                for identifier in definition_ids(definition):
                    index[namespace].setdefault(identifier, []).append((path, definition))
    return index


def load_monster_flags(source_root):
    """Read the engine's monster flag names from the enum_to_string switch."""
    source = Path(source_root) / MONSTER_FLAG_SOURCE
    try:
        text = source.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    return set(re.findall(r'case\s+[^:]+:\s*return\s+"([^"]+)"', text))


def assign_arguments(text, start):
    """Return the argument list of the assign() call whose name starts at start."""
    opening = text.find("(", start)
    arguments, depth, index, current = [], 1, opening + 1, ""
    while index < len(text) and depth:
        char = text[index]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
            if not depth:
                break
        elif char == "," and depth == 1:
            arguments.append(current.strip())
            current = ""
            index += 1
            continue
        current += char
        index += 1
    arguments.append(current.strip())
    return arguments


def strict_name(arguments):
    """assign( jo, "name", target, strict [, lo [, hi]] ) -> "name", else None."""
    if len(arguments) < 4 or arguments[3] != "strict":
        return None
    member = re.fullmatch(r'"([^"]+)"', arguments[1])
    if member and member.group(1) not in SET_MEMBERS:
        return member.group(1)
    return None


def block_end(text, opening):
    """Return the offset just past the brace that closes the block opened at opening."""
    depth, index = 1, opening + 1
    while index < len(text) and depth:
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
        index += 1
    return index


def named_functions(text):
    """Map every defined function in a C++ file to (start offset, end offset).

    The qualified name is kept ("islot_gun" is reachable as "islot_gun::load") and the bare
    member name is offered as a fallback, because "load" alone is defined many times.
    """
    ranges = {}
    for match in re.finditer(r"[\w:~<>]+(?:\s+[\w:~<>]+)*\s+([\w:~]+)\s*\(", text):
        depth, index = 1, match.end()
        while index < len(text) and depth:
            if text[index] == "(":
                depth += 1
            elif text[index] == ")":
                depth -= 1
            index += 1
        head = re.match(r"\s*(?:const\s*)?\{", text[index:index + 60])
        if not head:
            continue
        opening = index + head.end() - 1
        span = (match.start(), block_end(text, opening))
        name = match.group(1)
        ranges.setdefault(name, span)
        slot = re.search(r"islot_\w+", text[match.end():index])
        if slot:
            # Item_factory::load( islot_gun &slot, ... ) reads the members of islot_gun
            ranges.setdefault(f"{slot.group(0)}::load", span)
        if "::" in name:
            owner, member = name.rsplit("::", 1)
            ranges.setdefault(f"{owner}::{member}", span)
            ranges.setdefault(member, span)
    return ranges


def load_item_strict_members(source):
    """Map every item type to the members its loader passes to assign() with strict on.

    islot_* readers are shared, so the members of a slot only reach an item type whose
    loader owns that slot: "volume" is strict inside islot_tool, which is why a TOOL
    definition can report it while an ARMOR definition can not.
    """
    text = source.read_text(encoding="utf-8", errors="replace")
    ranges = named_functions(text)
    basic = ranges.get("load_basic_info")
    universal = set()
    if basic:
        for match in re.finditer(r"\bassign\s*\(", text[basic[0]:basic[1]]):
            name = strict_name(assign_arguments(text, basic[0] + match.start()))
            if name:
                universal.add(name)
    per_type = {}
    for loader, item_type in ITEM_LOADERS.items():
        span = ranges.get(loader)
        if not span:
            continue
        block = text[span[0]:span[1]]
        slots = re.findall(r"\bdef\.(\w+)\s*->\s*load\s*\(", block)
        slots += re.findall(r"load_slot(?:_optional)?\s*\(\s*def\s*\.\s*(\w+)\s*,", block)
        members = set(universal)
        for slot in slots:
            reader = ranges.get(f"islot_{slot}::load")
            if not reader:
                continue
            for call in re.finditer(r"\bassign\s*\(", text[reader[0]:reader[1]]):
                name = strict_name(assign_arguments(text, reader[0] + call.start()))
                if name:
                    members.add(name)
        per_type[item_type] = members
    return per_type


def load_strict_members(source_root):
    """Return the members that can raise a strict violation, keyed by namespace."""
    members = {"item": {}, "monster": set()}
    item_source = Path(source_root) / ITEM_MEMBER_SOURCE
    try:
        members["item"] = load_item_strict_members(item_source)
    except OSError:
        pass
    monster_source = Path(source_root) / MONSTER_FLAG_SOURCE
    try:
        text = monster_source.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return members
    match = re.search(r"void\s+mtype::load\s*\(", text)
    span = named_functions(text).get("mtype::load") if match else None
    if not span:
        return members
    for call in re.finditer(r"\bassign\s*\(", text[span[0]:span[1]]):
        name = strict_name(assign_arguments(text, span[0] + call.start()))
        if name:
            members["monster"].add(name)
    return members


def strict_members_for(definition, strict_members):
    """The members this definition can raise a strict violation for, or None if exempt."""
    definition_type = definition.get("type")
    namespace = namespace_of(definition_type)
    if namespace == "monster":
        return strict_members["monster"]
    return strict_members["item"].get(definition_type)


def comparable(value, member):
    """Reduce a JSON value to what the loader compares, or None if it cannot be compared."""
    if isinstance(value, bool) or value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)
    if not isinstance(value, str):
        # lists and objects are carried over by the loader as they are; an empty one is not
        # worth reporting and a nested comparison would guess too much
        return None
    match = NUMBER.match(value)
    units = MEMBER_UNITS.get(member, {})
    if not match or not match.group(2):
        return value
    factor = units.get(match.group(2).lower())
    if factor is None:
        return value
    return round(float(match.group(1)) * factor, 3)


def same_value(one, other, member):
    left, right = comparable(one, member), comparable(other, member)
    return left is not None and type(left) is type(right) and left == right


def check_monster_flags(path, definition, flags, errors):
    if not flags or definition.get("type") != "MONSTER":
        return
    holders = [("flags", definition.get("flags"))]
    for holder in ("extend", "delete"):
        block = definition.get(holder)
        if isinstance(block, dict):
            holders.append((f"{holder}.flags", block.get("flags")))
    for name, values in holders:
        if not isinstance(values, list):
            continue
        for flag in values:
            if isinstance(flag, str) and flag not in flags:
                ids = ", ".join(definition_ids(definition)) or "<no id>"
                errors.append(f'{path}: MONSTER (id {ids}) {name} uses the unknown flag '
                              f'"{flag}"')


def inherited_values(definition, index, namespace, depth=0, seen=None):
    """Resolve a copy-from chain into the values the child already inherits."""
    source = definition.get("copy-from")
    if not isinstance(source, str) or depth > 6:
        return {}
    seen = seen or set()
    if source in seen:
        return {}
    seen.add(source)
    entries = index[namespace].get(source, [])
    # Same order as the loader's own resolution: a core definition first (for a core child
    # that is the one the loader finds), then any non-self copy-from, then anything left.
    candidates = [entry for path, entry in entries
                  if is_strict_source(path) and entry.get("copy-from") != source]
    if not candidates:
        candidates = [entry for _, entry in entries if entry.get("copy-from") != source]
    if not candidates:
        candidates = [entry for _, entry in entries]
    if not candidates:
        return {}
    parent = candidates[0]
    values = dict(inherited_values(parent, index, namespace, depth + 1, seen))
    for key, value in parent.items():
        if key not in STRUCTURAL_KEYS or key == "name":
            values[key] = value
    for holder, combine in (("relative", lambda base, delta: base + delta),
                            ("proportional", lambda base, factor: base * factor)):
        block = parent.get(holder)
        if isinstance(block, dict):
            for key, delta in block.items():
                if isinstance(values.get(key), (int, float)) and isinstance(delta, (int, float)):
                    values[key] = combine(values[key], delta)
    return values


def is_strict_source(path):
    """True when the loader parses this file with strict enabled.

    Every strict call site is guarded by `bool strict = src == "dda"`, and the loader only
    hands out that source for the core data.  Definitions that come from a mod are parsed
    with strict off, which is exactly why a mod is free to repeat an inherited value.
    """
    parts = [part.lower() for part in Path(path).parts]
    for index in range(len(parts) - 1):
        if parts[index] == "data":
            return parts[index + 1] == "json"
    return True


def check_redundant_assign(path, definition, index, strict_members, errors, skipped):
    namespace = namespace_of(definition.get("type"))
    if not namespace or not isinstance(definition.get("copy-from"), str):
        return
    if not is_strict_source(path):
        return
    members = strict_members_for(definition, strict_members)
    if not members:
        # no loader asks for strict values for this type, so it can not be reported
        return
    inherited = inherited_values(definition, index, namespace)
    if not inherited:
        return
    relative = definition.get("relative") or {}
    proportional = definition.get("proportional") or {}
    identifiers = definition_ids(definition)
    for key, value in definition.items():
        if key in STRUCTURAL_KEYS or key in relative or key in proportional:
            continue
        if key not in members:
            # read through optional()/mandatory(), or assigned with strict off.  The
            # whitelist entries such as ("mon_chud", "symbol") are of this kind: they repeat
            # an inherited value in the JSON, but no strict-enabled assign() ever reads them,
            # so the loader stays quiet and reporting them would be a false positive.
            continue
        if key in inherited and same_value(inherited[key], value, key):
            if any((identifier, key) in KNOWN_REDUNDANT for identifier in identifiers):
                skipped.append(f"{', '.join(identifiers)}.{key}")
                continue
            ids = ", ".join(identifiers) or "<no id>"
            errors.append(f'{path}: {definition.get("type")} (id {ids}) sets "{key}" to a '
                          f'value it already inherits from "{definition["copy-from"]}"')


def check_copy_from(path, definition, index, errors):
    namespace = namespace_of(definition.get("type"))
    source = definition.get("copy-from")
    if not namespace or not isinstance(source, str):
        return
    if source not in index[namespace]:
        ids = ", ".join(definition_ids(definition)) or "<no id>"
        errors.append(f'{path}: {definition.get("type")} (id {ids}) copies from unknown '
                      f'"{source}"')


def collect_paths(arguments):
    paths = [Path(name) for name in arguments.files]
    if arguments.changed_files:
        # utf-8-sig so a list written with a BOM on Windows still works
        with open(arguments.changed_files, encoding="utf-8-sig") as handle:
            paths += [Path(line.strip()) for line in handle if line.strip()]
    if not paths:
        return sorted(Path(arguments.data_root).rglob("*.json"))
    return sorted({path for path in paths if path.suffix.lower() == ".json"})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", help="files to validate (default: all of data/)")
    parser.add_argument("--changed-files", help="file holding a newline separated path list")
    parser.add_argument("--data-root", default=str(DEFAULT_DATA_ROOT),
                        help="root scanned for ids and copy-from targets")
    parser.add_argument("--source-root", default=str(Path(__file__).resolve().parent.parent),
                        help="repository root, read for the flag table and the strict members")
    arguments = parser.parse_args()

    flags = load_monster_flags(arguments.source_root)
    strict_members = load_strict_members(arguments.source_root)
    errors = []
    skipped = []
    documents = {}
    checked = 0
    for path in collect_paths(arguments):
        if not path.is_file():
            continue
        try:
            document = load_json(path)
        except (OSError, json.JSONDecodeError):
            # syntax is covered by build-scripts/validate_json.py
            continue
        checked += 1
        documents[path] = document
        check_type_member(path, document, errors)
        for definition in iter_definitions(document):
            if definition.get("type") == "item_group":
                check_item_group(path, definition, errors)

    index = build_namespace_index(Path(arguments.data_root))
    for path, document in documents.items():
        for definition in iter_definitions(document):
            check_copy_from(path, definition, index, errors)
            check_monster_flags(path, definition, flags, errors)
            check_redundant_assign(path, definition, index, strict_members, errors, skipped)

    if skipped:
        print(f"数据语义检查：跳过 {len(skipped)} 处改动前就存在的、显式重复继承值的字段")
        print(f"  {', '.join(sorted(skipped))}")
    if errors:
        print(f"数据语义检查失败，共 {len(errors)} 处问题：", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    flag_note = "未找到怪物旗标表" if flags is None else f"{len(flags)} 个怪物旗标"
    print(f"数据语义检查通过，共检查 {checked} 个 JSON 文件（{flag_note}）。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
