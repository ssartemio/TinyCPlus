"""Verify hashes and execute a package after extracting it at another path."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
ROOT = Path(__file__).resolve().parents[1]
folder = Path(tempfile.mkdtemp(prefix='distribution-', dir=ROOT / 'build'))
archive = folder / 'package.zip'
subprocess.run([sys.executable, str(ROOT / 'tools/package.py'), '--output', str(archive)], check=True)
with zipfile.ZipFile(archive) as package:
    package.extractall(folder / 'relocated')
    for entry in package.infolist():
        if entry.external_attr >> 16:
            (folder / 'relocated' / entry.filename).chmod(entry.external_attr >> 16)
root = folder / 'relocated/tinycplus'
for line in (root / 'SHA256SUMS.txt').read_text().splitlines():
    digest, name = line.split('  ', 1)
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
capabilities = json.loads((root / 'CAPABILITIES.json').read_text())
assert capabilities['packagePlatform'] == ('windows' if os.name == 'nt' else 'macos' if sys.platform == 'darwin' else 'linux')
env = os.environ.copy()
for key in ('TINY_HOME', 'LIBTCC_PATH'):
    env.pop(key, None)
tiny = root / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
result = subprocess.run([str(tiny), 'doctor'], env=env, cwd=root, capture_output=True, text=True, timeout=30)
assert result.returncode == 0, (result.stdout, result.stderr)
assert str(root).replace('\\', '/') in result.stderr.replace('\\', '/')
result = subprocess.run([str(tiny), 'repl'], input='println(17);\n:quit\n', env=env, cwd=root, capture_output=True, text=True, timeout=30)
if capabilities['libtccAvailable']:
    assert result.returncode == 0 and '17' in result.stdout, (result.stdout, result.stderr)
else:
    assert result.returncode != 0, 'A package without libtcc must not advertise a working REPL'
print('Distribution verified: hashes, platform manifest, relocated compiler and REPL')
