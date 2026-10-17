#!/usr/bin/env python3
"""Create a relocatable archive, omitting scratch builds and test binaries."""
import argparse
import hashlib
import os
import json
import platform
import subprocess
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
host = {'Darwin': 'macos', 'Windows': 'windows'}.get(platform.system(), 'linux')
architecture = platform.machine().lower()
parser.add_argument('--output', default=str(ROOT.parent / f'tinycplus-1.0.0-rc.1-{host}-{architecture}.zip'))
args=parser.parse_args()
output=Path(args.output).resolve()
output.parent.mkdir(parents=True,exist_ok=True)
files=[]
for path in ROOT.rglob('*'):
    if not path.is_file() or path.resolve()==output:
        continue
    rel=path.relative_to(ROOT)
    if any(part in ('build','__pycache__','.git') for part in rel.parts) or path.suffix=='.pyc':
        continue
    if rel.parts[0]=='bin' and path.name not in ('tiny.exe','tinyc.exe','tinyedit.exe','tiny','tinyc','tinyedit'):
        continue
    if rel.parts[:3] == ('third_party', 'tcc', 'posix'):
        if host == 'windows':
            continue
    elif rel.parts[:2] == ('third_party', 'tcc') and host != 'windows':
        continue
    if rel.parts[0] == 'bin' and (path.suffix == '.exe') != (host == 'windows'):
        continue
    if rel.parts[:2]==('third_party','nghttp2') and path.name in ('nghttp2.dll','libnghttp2.dll.a'):
        continue
    files.append((rel.as_posix(),path))
executable = ROOT / 'bin' / ('tiny.exe' if host == 'windows' else 'tiny')
if not executable.is_file():
    raise SystemExit('Build the native compiler before packaging.')
capabilities = json.loads(subprocess.check_output([str(executable), 'capabilities'], text=True))
capabilities.update(packagePlatform=host, architecture=architecture,
                    dependencies=['host C runtime', 'system C headers/libraries for C compilation'],
                    optionalDependencies={'x11': 'libX11 development and runtime files',
                                          'cocoa': 'macOS Cocoa framework'},
                    validation='Run bin/tiny doctor after extracting on the target host.')
manifest=[]
with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
    for rel,path in sorted(files):
        data=path.read_bytes()
        manifest.append(hashlib.sha256(data).hexdigest()+'  '+rel)
        entry=zipfile.ZipInfo('tinycplus/'+rel,date_time=(2026,9,15,0,0,0))
        entry.compress_type=zipfile.ZIP_DEFLATED
        entry.external_attr=(0o755 if os.access(path,os.X_OK) else 0o644)<<16
        archive.writestr(entry,data)
    capability_data = (json.dumps(capabilities, indent=2, sort_keys=True) + '\n').encode()
    capability_entry = zipfile.ZipInfo('tinycplus/CAPABILITIES.json', date_time=(2026, 9, 15, 0, 0, 0))
    capability_entry.compress_type = zipfile.ZIP_DEFLATED
    capability_entry.external_attr = 0o644 << 16
    archive.writestr(capability_entry, capability_data)
    manifest.append(hashlib.sha256(capability_data).hexdigest() + '  CAPABILITIES.json')
    entry=zipfile.ZipInfo('tinycplus/SHA256SUMS.txt',date_time=(2026,9,15,0,0,0))
    entry.compress_type=zipfile.ZIP_DEFLATED
    entry.external_attr=0o644<<16
    archive.writestr(entry,'\n'.join(manifest)+'\n')
digest=hashlib.sha256(output.read_bytes()).hexdigest()
output.with_suffix(output.suffix+'.sha256').write_text(digest+'  '+output.name+'\n')
print(f'{len(files)} files; {output.stat().st_size:,} bytes; {output}\nSHA256 {digest}')
