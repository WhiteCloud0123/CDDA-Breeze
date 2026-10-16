#!/usr/bin/env python3
"""Control test for build-scripts/validate_json_semantics.py.

Builds a throw-away data tree holding the five faults that really aborted the game data
load (item_group entry shorthand, missing "type", an unknown monster flag, a copy-from
child repeating an inherited value, a copy-from target that does not exist) plus one
correct file, then checks that the validator fails and reports exactly the five faults
against the five planted files while staying silent about the correct file.

Self-contained: the tree lives in a fresh temporary directory and is removed again, so
the test can be re-run any number of times.  Exit code 0 means the control test passed.

Usage: python build-scripts/test_validate_json_semantics.py
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VALIDATOR = REPO_ROOT / "build-scripts" / "validate_json_semantics.py"


def write(path, document):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding="utf-8")


def main():
    temp = Path(tempfile.mkdtemp(prefix="validator_control_"))
    failures = []
    try:
        # files are collected as real paths; a dict keyed by name was a bug: the validator
        # then received no file at all, checked nothing and exited 0
        planted = []

        # a correct pair: abstract base plus a child that overrides the value it states.
        # "weight" is one of the members the loader reads with strict on.
        base_file = temp / "data" / "json" / "base.json"
        write(base_file, [
            {"type": "ARMOR", "abstract": "control_base", "price": 500, "weight": "1 kg"},
            {"type": "ARMOR", "id": "control_child", "copy-from": "control_base",
             "weight": "2 kg"},
        ])

        # 1. item_group entry shorthand, only legal inside items/groups
        group_file = temp / "data" / "json" / "group.json"
        write(group_file, [{"type": "item_group", "id": "control_group",
                            "entries": [["rag", 5]]}])
        planted.append((group_file, "array shorthand"))

        # 2. a definition without "type"
        type_file = temp / "data" / "json" / "notype.json"
        write(type_file, [{"id": "control_item", "name": "x"}])
        planted.append((type_file, 'has no "type"'))

        # 3. an unknown monster flag (BLEED is an upstream-only flag) inside delete
        flag_file = temp / "data" / "json" / "flag.json"
        write(flag_file, [{"type": "MONSTER", "id": "control_monster",
                           "delete": {"flags": ["BLEED", "REVIVES"]}}])
        planted.append((flag_file, "unknown flag"))

        # 4. a copy-from child repeating an inherited value (weight is strict-checked)
        redundant_file = temp / "data" / "json" / "redundant.json"
        write(redundant_file, [{"type": "ARMOR", "id": "control_redundant",
                                "copy-from": "control_base", "weight": "1 kg"}])
        planted.append((redundant_file, "already inherits"))

        # 5. a copy-from target that does not exist
        missing_file = temp / "data" / "json" / "missing.json"
        write(missing_file, [{"type": "ARMOR", "id": "control_copy",
                              "copy-from": "no_such_item"}])
        planted.append((missing_file, "copies from unknown"))

        checked_paths = [base_file] + [path for path, _ in planted]
        result = subprocess.run(
            [sys.executable, str(VALIDATOR), *[str(path) for path in checked_paths],
             "--data-root", str(temp / "data"), "--source-root", str(REPO_ROOT)],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            cwd=str(REPO_ROOT), env={**os.environ, "PYTHONUTF8": "1"})
        output = (result.stdout or "") + (result.stderr or "")
        print(f"validator exit code: {result.returncode}")
        print(output.strip()[:4000])

        findings = [line[2:] for line in output.splitlines() if line.startswith("- ")]
        # the validator prints the paths it was handed, so compare them case/separator-wise.
        # an item_group finding reads "<path> group <id>.entries[0]: ...", the others "<path>: ..."
        reported = {os.path.normcase(os.path.normpath(re.split(r":\s|\s+group\s", finding,
                                                              maxsplit=1)[0]))
                    for finding in findings}
        if result.returncode == 0:
            failures.append("validator reported success although faults were injected")
        if len(findings) != len(planted):
            failures.append(f"expected {len(planted)} findings, got {len(findings)}: {findings}")
        for path, needle in planted:
            if os.path.normcase(os.path.normpath(str(path))) not in reported:
                failures.append(f"{path.name}: no finding reported for this file")
            if needle not in output:
                failures.append(f"{path.name}: expected to see {needle!r} in the report")
        # the correct file must contribute no finding of its own; control_base may still be
        # named inside another file's message ("copies from unknown ..."), which is fine
        if os.path.normcase(os.path.normpath(str(base_file))) in reported:
            failures.append("the correct file was reported as a problem")
        if any("control_child" in finding for finding in findings):
            failures.append("the correct child definition was reported as a problem")
    finally:
        shutil.rmtree(temp, ignore_errors=True)

    if failures:
        print("\nCONTROL TEST FAILED:")
        for failure in failures:
            print(" -", failure)
        return 1
    print("\nCONTROL TEST PASSED: five faults reported, correct file clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
