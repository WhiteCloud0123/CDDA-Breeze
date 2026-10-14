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
#
# Usage: validate_json_semantics.py [file ...] [--changed-files list.txt] [--data-root data]

import argparse
import json
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
    """Collect item and monster ids so copy-from targets can be resolved."""
    index = {"item": set(), "monster": set()}
    for path in data_root.rglob("*.json"):
        try:
            document = load_json(path)
        except (OSError, json.JSONDecodeError):
            continue
        for definition in iter_definitions(document):
            namespace = namespace_of(definition.get("type"))
            if namespace:
                index[namespace].update(definition_ids(definition))
    return index


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
    arguments = parser.parse_args()

    errors = []
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

    if errors:
        print(f"数据语义检查失败，共 {len(errors)} 处问题：", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print(f"数据语义检查通过，共检查 {checked} 个 JSON 文件。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
