#!/usr/bin/env python3
# Copyright (c) 2026 TinyC+ contributors
# SPDX-License-Identifier: Apache-2.0
"""Assemble a stable release exclusively from the current commit and CI packages."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", required=True, type=Path)
    args = parser.parse_args()
    directory = args.artifacts.resolve()
    heading = (ROOT / "README.md").read_text(encoding="utf-8").splitlines()[0]
    match = re.fullmatch(r"# TinyC\+ (\d+\.\d+\.\d+)", heading)
    if not match:
        raise SystemExit("A stable semantic version is required in README.md")
    version = match[1]
    notes = ROOT / "docs" / "releases" / (version + ".md")
    if not notes.is_file() or not notes.read_text(encoding="utf-8").strip():
        raise SystemExit("Release notes are missing: " + str(notes))
    expected = {
        f"TinyCPlus-{version}-windows-x64.exe": b"MZ",
        f"tinycplus_{version}_amd64.deb": b"!<arch>\n",
        f"tinycplus-{version}-1.fc42.x86_64.rpm": b"\xed\xab\xee\xdb",
    }
    if set(p.name for p in directory.iterdir()) != set(expected):
        raise SystemExit("Expected exactly the verified Windows, Ubuntu and Fedora packages: " + ", ".join(expected))
    for name, magic in expected.items():
        path = directory / name
        with path.open("rb") as stream:
            if stream.read(len(magic)) != magic:
                raise SystemExit("Invalid package header: " + name)
    source = directory / f"tinycplus-{version}-source.tar.gz"
    subprocess.run(["git", "archive", "--format=tar.gz", f"--prefix=tinycplus-{version}/",
                    "--output=" + str(source), "HEAD"], cwd=ROOT, check=True)
    manifest = []
    for path in sorted(directory.iterdir()):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        manifest.append(f"{digest}  {path.name}\n")
    (directory / "SHA256SUMS.txt").write_text("".join(manifest), encoding="utf-8", newline="\n")
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
            output.write(f"version={version}\n")
    print(f"Prepared TinyC+ {version}: {len(manifest)} assets and SHA256SUMS.txt")


if __name__ == "__main__":
    main()
