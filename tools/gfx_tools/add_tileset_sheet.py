#!/usr/bin/env python3
"""Add a sprite sheet to a tileset with indices derived from the real image geometry.

Why this tool exists: the engine walks "tiles-new" in order and advances the sprite index
by the number of cells an image *really* has - width / sprite_width * height /
sprite_height, see src/cata_tiles.cpp (load_tileset() and the following "offset += size").
It does not use the length of the JSON "tiles" array, and upstream sheets routinely declare
fewer entries than they have cells.  Computing the new block's start index from the declared
counts therefore produces indices that point into another image's unused cells, and the
sprites silently draw nothing.

The sheet is inserted before the fallback block, which is where the upstream tilesets keep
their extra sheets, and the new indices are recomputed from the images themselves.

Usage:
  add_tileset_sheet.py <tileset dir> <sheet png> <ids json> [--replace] [--dry-run]

<ids json> is a JSON array; each element is either "some_id" or ["id_a", "id_b"] to give
several ids the same sprite (for example an item and its worn/wielded overlays).
"""
import argparse
import json
import shutil
import struct
import sys
from pathlib import Path


def png_size(path):
    with path.open("rb") as handle:
        header = handle.read(33)
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path}: not a PNG file")
    return struct.unpack(">II", header[16:24])


def real_cells(tileset_root, block, defaults):
    sheet = tileset_root / block["file"]
    width = block.get("sprite_width", defaults["width"])
    height = block.get("sprite_height", defaults["height"])
    if not sheet.is_file():
        return len(block["tiles"])
    image_width, image_height = png_size(sheet)
    return (image_width // width) * (image_height // height)


def find_array_end(text, open_index):
    depth = 0
    in_string = False
    escaped = False
    for index in range(open_index, len(text)):
        char = text[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "[":
            depth += 1
        elif char == "]":
            depth -= 1
            if depth == 0:
                return index
    raise SystemExit("could not find the end of the tiles-new array")


def block_span(text, filename):
    needle = f'"file": "{filename}"'
    index = text.find(needle)
    if index < 0:
        return None
    start = text.rfind("{", 0, index)
    depth = 0
    in_string = False
    escaped = False
    for position in range(start, len(text)):
        char = text[position]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return start, position + 1
    return None


def render_block(filename, ids, start, newline, indent):
    lines = [f"{indent}{{", f'{indent}  "file": "{filename}",',
             f'{indent}  "//": "range {start} to {start + len(ids) - 1}",',
             f'{indent}  "tiles": [']
    rendered = []
    for offset, identifier in enumerate(ids):
        if isinstance(identifier, list):
            id_text = "[" + ", ".join(json.dumps(value) for value in identifier) + "]"
        else:
            id_text = json.dumps(identifier)
        rendered.append(f'{indent}    {{ "id": {id_text}, "fg": {start + offset}, '
                        f'"rotates": false }}')
    lines.append(",".join(rendered))
    lines.append(f"{indent}  ]")
    lines.append(f"{indent}}}")
    return newline.join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("tileset")
    parser.add_argument("sheet")
    parser.add_argument("ids")
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    tileset = Path(args.tileset)
    config_path = tileset / "tile_config.json"
    raw = config_path.read_text(encoding="utf-8")
    newline = "\r\n" if "\r\n" in raw else "\n"
    config = json.loads(raw)
    sheet_name = Path(args.sheet).name
    ids = json.loads(Path(args.ids).read_text(encoding="utf-8"))

    def used_ids(document):
        found = set()
        for block in document["tiles-new"]:
            for tile in block["tiles"]:
                value = tile.get("id")
                found.update(value if isinstance(value, list) else [value])
        return found

    existing = [identifier for identifier in ids
                if not isinstance(identifier, list) and identifier in used_ids(config)]
    if existing or block_span(raw, sheet_name):
        if not args.replace:
            print(f"{config_path}: {sheet_name} or its ids are already present, "
                  f"use --replace to re-insert")
            return 0
        span = block_span(raw, sheet_name)
        if span:
            before = raw[:span[0]].rstrip()
            after = raw[span[1]:]
            if before.endswith(","):
                before = before[:-1]
            elif after.lstrip().startswith(","):
                after = after.lstrip()[1:]
            raw = before + after
            config = json.loads(raw)
        else:
            print(f"{config_path}: ids exist without a {sheet_name} block, nothing to replace")
            return 1

    defaults = config["tile_info"][0]
    fallback_index = next((index for index, block in enumerate(config["tiles-new"])
                           if block["file"].startswith("fallback")), None)
    start = sum(real_cells(tileset, block, defaults)
                for block in config["tiles-new"][:fallback_index])
    indent = "  "
    block = render_block(sheet_name, ids, start, newline, indent)

    if fallback_index is None:
        marker = '"tiles-new"'
        open_index = raw.index("[", raw.find(marker))
        close_index = find_array_end(raw, open_index)
        updated = raw[:close_index] + "," + newline + block + newline + raw[close_index:]
    else:
        fallback = config["tiles-new"][fallback_index]
        span = block_span(raw, fallback["file"])
        if span is None:
            raise SystemExit("could not locate the fallback block")
        updated = raw[:span[0]] + block + "," + newline + raw[span[0]:]

    parsed = json.loads(updated)
    inserted = next(block for block in parsed["tiles-new"] if block["file"] == sheet_name)
    first = inserted["tiles"][0]["fg"]
    if first != start:
        raise SystemExit(f"internal error: inserted at {first}, expected {start}")
    if len(inserted["tiles"]) != len(ids):
        raise SystemExit("internal error: entry count changed")

    if args.dry_run:
        print(f"{config_path}: would add {len(ids)} tiles for {sheet_name} at "
              f"{start}..{start + len(ids) - 1}")
        return 0

    source_sheet = Path(args.sheet)
    if source_sheet.resolve() != (tileset / sheet_name).resolve():
        shutil.copy2(source_sheet, tileset / sheet_name)
    config_path.write_text(updated, encoding="utf-8", newline="")
    print(f"{config_path}: added {len(ids)} tiles for {sheet_name} at "
          f"{start}..{start + len(ids) - 1}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
