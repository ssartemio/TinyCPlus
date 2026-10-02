#!/usr/bin/env python3
"""Build native TinyC+ installers from a verified, platform-specific build.

Run tools/bootstrap.py and build.py on EACH target OS before packaging.
No cross-compilation, Python packages, or shell interpolation required here.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VERSION_TEXT = (ROOT / "README.md").read_text(encoding="utf-8")
VERSION_MATCH = re.search(r"^# TinyC\+\s+(\d+\.\d+\.\d+(?:-[\w.]+)?)", VERSION_TEXT, re.M)
if not VERSION_MATCH:
    raise SystemExit("Cannot parse TinyC+ release version from README.md")
VERSION = VERSION_MATCH.group(1)
BASE_VERSION = VERSION.split("-")[0]
PRERELEASE = VERSION.split("-", 1)[1] if "-" in VERSION else ""
DEB_VERSION = BASE_VERSION + ("~" + PRERELEASE if PRERELEASE else "")
RPM_RELEASE = ("0." + PRERELEASE if PRERELEASE else "1").replace("-", ".")
BINARY = ".exe" if os.name == "nt" else ""
LINUX_ROOT = Path("usr/lib/tinycplus")


def execute(args: list[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def verify_platform(which: str) -> None:
    if which == "windows" and os.name != "nt":
        raise SystemExit("Windows installers must be built on Windows x64")
    if which == "linux" and not sys.platform.startswith("linux"):
        raise SystemExit(".deb and .rpm packages must be built on Linux")


def linux_arch(which: str) -> str:
    machine = platform.machine().lower()
    mapping = {"x86_64": ("amd64", "x86_64"), "amd64": ("amd64", "x86_64"),
               "aarch64": ("arm64", "aarch64"), "arm64": ("arm64", "aarch64")}
    if machine not in mapping:
        raise SystemExit("Unsupported architecture: " + machine)
    return mapping[machine][0 if which == "deb" else 1]


def fresh(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)


def copydir(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise SystemExit("Missing directory: " + str(source))
    shutil.copytree(source, destination, symlinks=True)


def stage(target: str, destination: Path) -> None:
    verify_platform(target)
    fresh(destination)
    binpath = destination / "bin"
    binpath.mkdir()
    for name in ("tiny", "tinyc", "tinyedit"):
        source = ROOT / "bin" / (name + BINARY)
        if not source.is_file():
            raise SystemExit(f"Missing {source}; build all three binaries first")
        shutil.copy2(source, binpath / source.name)
    for folder in ("runtime", "std", "examples"):
        copydir(ROOT / folder, destination / folder)
    tp = destination / "third_party"
    tp.mkdir()
    tcc = tp / "tcc"
    tcc.mkdir()
    original = ROOT / "third_party" / "tcc"
    if target == "linux":
        copydir(original / "posix", tcc / "posix")
        for name in ("COPYING", "VERSION", "tinycc-source.zip"):
            source = original / name
            if not source.is_file():
                raise SystemExit("Missing redistributed TCC license/source: " + str(source))
            shutil.copy2(source, tcc / name)
        if not any((tcc / "posix" / name).exists()
                   for name in ("libtcc.so", "libtcc.dylib")):
            raise SystemExit("Missing compiled libtcc.so; run tools/bootstrap.py")
        if not (tcc / "posix" / "tcc").is_file():
            raise SystemExit("Missing compiled TCC executable in posix/")
        # Bootstrap uses the build checkout as its configure prefix. Those
        # absolute RUNPATHs are invalid after relocation, and Fedora correctly
        # rejects them. Resolve shared libraries beside the installed binary.
        patcher = shutil.which("patchelf")
        if not patcher:
            raise SystemExit("Install patchelf to create relocatable Linux installers")
        for lib in (tcc / "posix").rglob("*"):
            if lib.is_file() and (lib.name == "tcc" or
                                 lib.name.startswith("libtcc.so")):
                execute([patcher, "--set-rpath", "$ORIGIN", lib])
    else:
        # Windows portable TCC tree contains its runtime objects, SDK headers,
        # libtcc.dll and original source for license compliance.
        shutil.copytree(original, tcc, symlinks=True, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("posix", "__pycache__", "*.pyc"))
        for name in ("tcc.exe", "libtcc.dll", "COPYING", "tinycc-source.zip"):
            if not (tcc / name).is_file():
                raise SystemExit("Missing Windows toolchain component: " + name)
        # MinGW may dynamically import these depending on toolchain flags.
        # Bundle known redistributable GCC runtime DLLs, not arbitrary PATH DLLs.
        gcc = shutil.which("gcc")
        if gcc:
            for name in ("libgcc_s_seh-1.dll", "libwinpthread-1.dll"):
                src = Path(gcc).parent / name
                if src.is_file():
                    shutil.copy2(src, binpath / name)
    copydir(ROOT / "third_party" / "nghttp2", tp / "nghttp2")
    for name in ("THIRD_PARTY.md", "README.md"):
        shutil.copy2(ROOT / name, destination / name)
    # Installed examples are documentation AND provide a no-network smoke test.
    print(f"Staged TinyC+ {VERSION} ({target}) in {destination}")


def check_stage(destination: Path, target: str) -> None:
    required = [
        "bin/tiny" + (".exe" if target == "windows" else ""),
        "bin/tinyc" + (".exe" if target == "windows" else ""),
        "bin/tinyedit" + (".exe" if target == "windows" else ""),
        "std/core.tc", "runtime/tiny_runtime.h",
        "third_party/nghttp2/COPYING", "third_party/nghttp2/sources.txt",
        "third_party/tcc/COPYING", "examples/hello.tc",
    ]
    for rel in required:
        if not (destination / rel).is_file():
            raise SystemExit("Incomplete payload: missing " + rel)


def link_aliases(root: Path) -> None:
    executable_dir = root / "usr" / "bin"
    executable_dir.mkdir(parents=True, exist_ok=True)
    for name in ("tiny", "tinyc", "tinyedit"):
        (executable_dir / name).symlink_to("../lib/tinycplus/bin/" + name)


def deb(payload: Path, out: Path) -> Path:
    verify_platform("linux")
    if not shutil.which("dpkg-deb"):
        raise SystemExit("Install dpkg-dev / dpkg-deb to build a Debian package")
    check_stage(payload, "linux")
    out.mkdir(parents=True, exist_ok=True)
    arch = linux_arch("deb")
    artifact = out / f"tinycplus_{DEB_VERSION}_{arch}.deb"
    with tempfile.TemporaryDirectory(prefix="tinycplus-deb-") as tmp:
        root = Path(tmp) / "package"
        copydir(payload, root / LINUX_ROOT)
        link_aliases(root)
        control = root / "DEBIAN"
        control.mkdir()
        (control / "control").write_text(
            f"Package: tinycplus\nVersion: {DEB_VERSION}\n"
            f"Architecture: {arch}\nMaintainer: TinyC+ contributors\n"
            "Section: devel\nPriority: optional\nDepends: libc6\n"
            "Description: TinyC+ native compiler and lightweight runtime\n"
            " C11-lowered language compiler with manual ownership, standard\n"
            " library, bundled TinyCC backend, and terminal editor.\n",
            encoding="utf-8",
        )
        execute(["dpkg-deb", "--root-owner-group", "--build", root, artifact])
    return artifact


def rpm(payload: Path, out: Path) -> Path:
    verify_platform("linux")
    if not shutil.which("rpmbuild"):
        raise SystemExit("Install rpm-build to build RPM packages")
    check_stage(payload, "linux")
    out.mkdir(parents=True, exist_ok=True)
    arch = linux_arch("rpm")
    with tempfile.TemporaryDirectory(prefix="tinycplus-rpm-") as tmp:
        work = Path(tmp)
        top = work / "rpmbuild"
        for name in ("BUILD", "BUILDROOT", "RPMS", "SOURCES", "SPECS", "SRPMS"):
            (top / name).mkdir(parents=True)
        spec = top / "SPECS" / "tinycplus.spec"
        # The project has not declared an umbrella source license yet.
        # Do not mislabel its code as MIT/LGPL; the bundled TCC/nghttp2 notices
        # are shipped separately and documented in THIRD_PARTY.md.
        spec.write_text(
            "Name: tinycplus\n"
            f"Version: {BASE_VERSION}\nRelease: {RPM_RELEASE}%{{?dist}}\n"
            "Summary: TinyC+ native compiler and lightweight runtime\n"
            "License: LicenseRef-TinyCPlus-Undeclared\n"
            "URL: https://github.com/ssartemio/TinyCPlus\n"
            f"BuildArch: {arch}\n"
            "AutoReqProv: yes\n"
            "%description\n"
            "Native TinyC+ frontend with bundled TinyCC and standard runtime.\n\n"
            "%prep\n"
            "%build\n"
            "%install\n"
            "mkdir -p %{buildroot}/usr/lib/tinycplus %{buildroot}/usr/bin\n"
            "cp -a %{_tc_payload}/. %{buildroot}/usr/lib/tinycplus/\n"
            "ln -s ../lib/tinycplus/bin/tiny %{buildroot}/usr/bin/tiny\n"
            "ln -s ../lib/tinycplus/bin/tinyc %{buildroot}/usr/bin/tinyc\n"
            "ln -s ../lib/tinycplus/bin/tinyedit %{buildroot}/usr/bin/tinyedit\n"
            "%files\n"
            "%dir /usr/lib/tinycplus\n"
            "/usr/lib/tinycplus/*\n"
            "/usr/bin/tiny\n/usr/bin/tinyc\n/usr/bin/tinyedit\n",
            encoding="utf-8",
        )
        execute(["rpmbuild", "-bb", "--define", "_topdir " + str(top),
                 "--define", "_tc_payload " + str(payload.resolve()), spec])
        built = sorted((top / "RPMS").rglob("*.rpm"))
        if len(built) != 1:
            raise SystemExit("Expected one RPM; found " + str(built))
        artifact = out / built[0].name
        shutil.copy2(built[0], artifact)
    return artifact


def windows(payload: Path, out: Path) -> Path:
    verify_platform("windows")
    check_stage(payload, "windows")
    iscc = shutil.which("ISCC.exe") or shutil.which("ISCC")
    if not iscc:
        for folder in (os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                       os.environ.get("ProgramFiles", r"C:\Program Files")):
            candidate = Path(folder) / "Inno Setup 6" / "ISCC.exe"
            if candidate.is_file():
                iscc = str(candidate)
                break
    if not iscc:
        raise SystemExit("Install Inno Setup 6 (ISCC.exe), then retry")
    out.mkdir(parents=True, exist_ok=True)
    execute([iscc, "/Qp", "/DMyAppVersion=" + VERSION,
             "/DPayloadDir=" + str(payload.resolve()),
             "/DOutputDir=" + str(out.resolve()),
             str(ROOT / "packaging" / "windows.iss")])
    results = list(out.glob(f"TinyCPlus-{VERSION}-windows-x64*.exe"))
    if len(results) != 1:
        raise SystemExit("ISCC did not produce the expected installer")
    return results[0]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    stage_p = sub.add_parser("stage", help="Stage verified runtime payload")
    stage_p.add_argument("--target", required=True, choices=("linux", "windows"))
    stage_p.add_argument("--output", type=Path, default=ROOT / "build" / "installers" / "payload")
    for name in ("deb", "rpm", "windows"):
        p = sub.add_parser(name, help=f"Build {name} installer from staged payload")
        p.add_argument("--stage", type=Path, default=ROOT / "build" / "installers" / "payload")
        p.add_argument("--output", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    if args.command == "stage":
        stage(args.target, args.output.resolve())
    else:
        payload = args.stage.resolve()
        artifact = {"deb": deb, "rpm": rpm, "windows": windows}[args.command](
            payload, args.output.resolve()
        )
        print("Built:", artifact)


if __name__ == "__main__":
    main()
