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
# Use cmd's quoting rules rather than the C argv quoting used for ordinary EXEs.
command = (f'"{system32 / "cmd.exe"}" /D /Q /C '
           f'""{launcher}" && tiny run examples\\hello.tc '
           '&& tinyc check examples\\hello.tc && echo CONSOLE_SMOKE_OK"')
other_directory = Path(__file__).resolve().parents[1] / "build" / "tests" / ("console-" + uuid.uuid4().hex)
other_directory.mkdir(parents=True)
result = subprocess.run(command, cwd=other_directory, env=env,
                        capture_output=True, text=True, timeout=30)
assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
assert "30" in result.stdout.splitlines(), result.stdout
assert "CONSOLE_SMOKE_OK" in result.stdout.splitlines(), result.stdout
assert not result.stderr, result.stderr
print("Windows console verified: relative example paths, tiny/tinyc and no user PATH dependency")
