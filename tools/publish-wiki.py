#!/usr/bin/env python3
"""Publish ./wiki to the repository's native GitHub Wiki using git.

Requires:
- git in PATH
- credentials able to push ssartemio/TinyCPlus.wiki.git

README.md is intentionally excluded because it documents the source layout
inside the main repository rather than being a Wiki page.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "wiki"
DEFAULT_REMOTE = "git@github.com:ssartemio/TinyCPlus.wiki.git"

def run(args: list[str], cwd: Path | None = None) -> None:
    subprocess.run(args, cwd=cwd, check=True)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--remote", default=DEFAULT_REMOTE)
    parser.add_argument("--message", default="docs: update project wiki")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not SOURCE.is_dir():
        raise SystemExit(f"missing wiki source: {SOURCE}")
    if shutil.which("git") is None:
        raise SystemExit("git was not found in PATH")

    with tempfile.TemporaryDirectory(prefix="tinycplus-wiki-") as temp:
        target = Path(temp) / "wiki"
        run(["git", "clone", args.remote, str(target)])

        for item in target.iterdir():
            if item.name == ".git":
                continue
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()

        for source in SOURCE.iterdir():
            if source.name == "README.md":
                continue
            destination = target / source.name
            if source.is_dir():
                shutil.copytree(source, destination)
            else:
                shutil.copy2(source, destination)

        run(["git", "add", "-A"], cwd=target)
        status = subprocess.run(
            ["git", "status", "--porcelain"],
            cwd=target,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()

        if not status:
            print("Wiki already up to date.")
            return 0

        print(status)
        if args.dry_run:
            print("Dry run: no commit or push performed.")
            return 0

        run(["git", "commit", "-m", args.message], cwd=target)
        run(["git", "push"], cwd=target)

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
