"""Exercise the installed Windows console from outside its installation directory."""
# Copyright (c) 2026 TinyC+ contributors
# SPDX-License-Identifier: Apache-2.0
import os
from pathlib import Path
import subprocess
import sys
import uuid

if os.name != "nt":
    raise SystemExit("This test requires Windows")

root = Path(sys.argv[1]).resolve()
launcher = root / "bin" / "tiny-console.cmd"
assert launcher.is_file(), launcher
env = os.environ.copy()
for key in ("TINY_HOME", "LIBTCC_PATH", "CC"):
    env.pop(key, None)
system32 = Path(os.environ["SYSTEMROOT"]) / "System32"
env["PATH"] = str(system32)
other_directory = Path(__file__).resolve().parents[1] / "build" / "tests" / ("user program " + uuid.uuid4().hex)
other_directory.mkdir(parents=True)
source = other_directory / "hola.tc"
executable = other_directory / "hola.exe"
# A new user file, outside the installation, as saved in UTF-8 by Windows editors.
source.write_text('int main() { println("Hola mundo"); return 0; }\n', encoding="utf-8-sig")
# Use cmd's quoting rules rather than the C argv quoting used for ordinary EXEs.
command = (f'"{system32 / "cmd.exe"}" /D /Q /C '
           f'""{launcher}" && tiny run examples\\hello.tc '
           '&& tinyc check examples\\hello.tc '
           f'&& cd /d "{other_directory}" && tinyc check hola.tc '
           f'&& tiny run hola.tc && tiny build hola.tc -o hola.exe '
           f'&& "{executable}" && echo CONSOLE_SMOKE_OK"')
result = subprocess.run(command, cwd=other_directory, env=env,
                        capture_output=True, text=True, timeout=30)
assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
assert "30" in result.stdout.splitlines(), result.stdout
assert result.stdout.splitlines().count("Hola mundo") == 2, result.stdout
assert "CONSOLE_SMOKE_OK" in result.stdout.splitlines(), result.stdout
assert not result.stderr, result.stderr
print("Windows console verified: examples, new UTF-8 user program and executable, paths with spaces, no user PATH dependency")
