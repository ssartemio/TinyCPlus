#!/usr/bin/env python3
"""Compile and run the stable examples from the TinyC+ desde cero course."""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TINY = ROOT / "bin" / ("tiny.exe" if os.name == "nt" else "tiny")
# File exercises write their temporary state here. The suite is also runnable
# directly after build.py, which does not create the build directory.
(ROOT / "build").mkdir(exist_ok=True)

parser = argparse.ArgumentParser()
parser.add_argument("--cc", help="Use an external C compiler for tutorial runs")
args = parser.parse_args()

CASES = [
    ("01_first_program.tc", "OK\n61\n"),
    ("02_functions_control.tc", "61\n1\n13\n"),
    ("03_arrays_slices_errors.tc", "57\n70\n2\n"),
    ("04_memory_resources.tc", "TinyC+\nstatus=42\n"),
    ("05_objects_models.tc", "61\n1\n"),
    ("06_collections_streams.tc", "4\n95\n4\n"),
    ("08_persistence_files.tc", "72\n"),
    ("09_concurrency.tc", "1\n40\n41\n42\n"),
    ("10_async_network.tc", "42\n"),
    ("11_tinystatus_server.tc", "1\n72\n1\n0\n"),
    ("12_tinystatus_tui.tc", "dashboard ok\n"),
]

SOLUTION_CASES = [
    ("01_exercises.tc", "WARN\n90\n"),
    ("02_exercises.tc", "100\n3\n4\n"),
    ("03_exercises.tc", "70\n2\n1\n"),
    ("04_exercises.tc", "first\nsecond\nthird\nbody\ncleanup\n42\n"),
    ("05_exercises.tc", "61\n0\n"),
    ("06_exercises.tc", "95\n1\n42\n"),
    ("08_exercises.tc", "missing\n73\n73\n74\n"),
    ("09_exercises.tc", "20\n4\n42\nclosed\n"),
    ("10_exercises.tc", "10\n"),
    ("11_exercises.tc", "1\n73\n1\n"),
    ("12_exercises.tc", "dashboard exercises ok\n"),
]

def invoke(command):
    return subprocess.run(
        [str(TINY), *map(str, command), "--home", str(ROOT)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )

def check_case(path):
    checked = invoke(["check", path])
    assert checked.returncode == 0, (path, checked.stdout, checked.stderr)

    command = ["run", path]
    if args.cc:
        command += ["--cc", args.cc]
    return invoke(command)

passed = 0
tutorial = ROOT / "examples" / "tutorial"

for filename, expected in CASES:
    path = tutorial / filename
    result = check_case(path)
    assert result.returncode == 0, (filename, result.stdout, result.stderr)
    assert result.stdout == expected, (filename, result.stdout, expected)
    passed += 1
    print("PASS", filename)

module_main = tutorial / "07_modules" / "main.tc"
module_c = tutorial / "07_modules" / "checksum.c"
checked = invoke(["check", module_main])
assert checked.returncode == 0, (checked.stdout, checked.stderr)
command = ["run", module_main, "--c-source", module_c]
if args.cc:
    command += ["--cc", args.cc]
result = invoke(command)
assert result.returncode == 0, (result.stdout, result.stderr)
assert result.stdout == "61\n781\n", result.stdout
passed += 1
print("PASS 07_modules/main.tc")

solutions = tutorial / "solutions"
for filename, expected in SOLUTION_CASES:
    path = solutions / filename
    result = check_case(path)
    assert result.returncode == 0, (filename, result.stdout, result.stderr)
    assert result.stdout == expected, (filename, result.stdout, expected)
    passed += 1
    print("PASS solution", filename)

EDGE_CASES = [
    ("03_bounds_fail.tc", 101, "index out of bounds"),
]

for filename, expected_code, expected_stderr in EDGE_CASES:
    path = tutorial / "exercises" / filename
    result = invoke(["run", path, *(["--cc", args.cc] if args.cc else [])])
    assert result.returncode == expected_code, (filename, result.returncode, result.stdout, result.stderr)
    assert expected_stderr in result.stderr, (filename, result.stderr)
    passed += 1
    print("PASS expected failure", filename)

for filename, expected in [
    ("04_defer_return.tc", "body\ncleanup\n42\n"),
    ("08_corrupt_file.tc", "parse error\n"),
    ("09_channel_closed.tc", "closed\n"),
    ("11_multi_client.tc", "72\n73\n"),
]:
    path = tutorial / "exercises" / filename
    result = check_case(path)
    assert result.returncode == 0, (filename, result.stdout, result.stderr)
    assert result.stdout == expected, (filename, result.stdout, expected)
    passed += 1
    print("PASS exercise", filename)

print(f"{passed} tutorial examples passed")
