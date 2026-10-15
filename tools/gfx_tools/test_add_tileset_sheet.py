#!/usr/bin/env python3
"""Self test for add_tileset_sheet.py: a synthetic tileset plus the repository tileset.

The synthetic case mimics the upstream habit of declaring fewer entries than a sheet has
cells, which is what makes the naive index calculation wrong.  The repository case checks
that the tool reproduces the block the repository already ships (start 8898).
"""
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOL = Path(__file__).resolve().parent / "add_tileset_sheet.py"


def make_png(path, cells_x, cells_y, size=32):
    """Write a minimal RGBA PNG of the requested cell grid."""
    width, height = cells_x * size, cells_y * size
    raw = b"".join(b"\x00" + bytes([200, 100, 50, 255]) * width for _ in range(height))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 1))
           + chunk(b"IEND", b""))
    path.write_bytes(png)


def run(arguments):
    result = subprocess.run([sys.executable, str(TOOL), *arguments],
                            capture_output=True, text=True, encoding="utf-8",
                            errors="replace")
    return result.returncode, (result.stdout or "") + (result.stderr or "")


def synthetic_case():
    temp = Path(tempfile.mkdtemp(prefix="tileset_tool_"))
    tileset = temp / "gfx" / "TestSet"
    tileset.mkdir(parents=True)
    make_png(tileset / "a.png", 2, 2)          # 4 cells
    make_png(tileset / "fallback.png", 1, 1)   # 1 cell
    make_png(tileset / "new.png", 1, 2)        # 2 cells
    (tileset / "tile_config.json").write_text(json.dumps({
        "tile_info": [{"pixelscale": 1, "width": 32, "height": 32}],
        "tiles-new": [
            {"file": "a.png", "tiles": [
                {"id": "first", "fg": 0, "rotates": False},
                {"id": "second", "fg": 1, "rotates": False},
                {"id": "third", "fg": 2, "rotates": False}]},
            {"file": "fallback.png", "tiles": []}]}, indent=2), encoding="utf-8")
    ids = temp / "ids.json"
    ids.write_text(json.dumps(["fresh_a", ["fresh_b", "fresh_b_worn"]]), encoding="utf-8")

    code, output = run([str(tileset), str(tileset / "new.png"), str(ids)])
    print(f"  tool said (exit {code}): {output.strip()}")
    config = json.loads((tileset / "tile_config.json").read_text(encoding="utf-8"))
    files = [block["file"] for block in config["tiles-new"]]
    new_block = next((block for block in config["tiles-new"] if block["file"] == "new.png"), None)
    if new_block is None:
        shutil.rmtree(temp, ignore_errors=True)
        return [f"the new sheet was not inserted, blocks are {files}"]
    fgs = [tile["fg"] for tile in new_block["tiles"]]
    problems = []
    if code != 0:
        problems.append(f"tool failed: {output.strip()}")
    if fgs != [4, 5]:
        problems.append(f"expected indices [4, 5] (after a.png's 4 real cells), got {fgs}")
    if files != ["a.png", "new.png", "fallback.png"]:
        problems.append(f"expected the block before fallback, got {files}")
    shutil.rmtree(temp, ignore_errors=True)
    return problems


def repository_case():
    source = REPO / "gfx" / "MshockXotto+"
    if not (source / "shields.png").is_file():
        # the shield sheet only exists on the branch that adds the shields
        print("  repository case skipped: gfx/MshockXotto+/shields.png is not present")
        return []
    temp = Path(tempfile.mkdtemp(prefix="tileset_repo_"))
    target = temp / "Set"
    target.mkdir(parents=True)
    config = json.loads((source / "tile_config.json").read_text(encoding="utf-8"))
    for block in config["tiles-new"]:
        sheet = source / block["file"]
        if sheet.is_file():
            shutil.copy2(sheet, target / block["file"])
    shutil.copy2(source / "tile_config.json", target / "tile_config.json")
    shutil.copy2(source / "shields.png", target / "shields.png")
    ids = temp / "ids.json"
    id_list = [tile["id"] for tile in
               next(b for b in config["tiles-new"] if b["file"] == "shields.png")["tiles"]]
    ids.write_text(json.dumps(id_list), encoding="utf-8")

    code, output = run([str(target), str(target / "shields.png"), str(ids), "--replace"])
    print(f"  repo case tool said (exit {code}): {output.strip()}")
    problems = []
    if code != 0:
        problems.append(f"tool failed: {output.strip()}")
    else:
        updated = json.loads((target / "tile_config.json").read_text(encoding="utf-8"))
        block = next(b for b in updated["tiles-new"] if b["file"] == "shields.png")
        fgs = [tile["fg"] for tile in block["tiles"]]
        if fgs != list(range(8898, 8898 + len(id_list))):
            problems.append(f"expected the repository block to be reproduced at 8898.."
                            f"{8897 + len(id_list)}, got {fgs[0]}..{fgs[-1]}")
        if [b["file"] for b in updated["tiles-new"]][-1] != "fallback.png":
            problems.append("fallback.png is no longer last")
    shutil.rmtree(temp, ignore_errors=True)
    return problems


def main():
    failures = synthetic_case() + repository_case()
    if failures:
        print("SELF TEST FAILED:")
        for failure in failures:
            print(" -", failure)
        return 1
    print("SELF TEST PASSED: synthetic indices correct"
          + (", repository block reproduced at 8898"
             if (REPO / "gfx/MshockXotto+/shields.png").is_file() else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
