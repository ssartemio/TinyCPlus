#!/usr/bin/env python3
"""Build the dependency-free C11 frontend. No packages are required."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--cc', default=os.environ.get('CC'))
parser.add_argument('--debug', action='store_true')
parser.add_argument('--sanitize', action='store_true', help='GCC/Clang ASan and UBSan (requires host runtimes)')
args = parser.parse_args()
ext = '.exe' if os.name == 'nt' else ''
bundled = ROOT / 'third_party' / 'tcc' / ('tcc.exe' if os.name == 'nt' else 'posix/tcc')
cc = args.cc or (str(bundled) if bundled.exists() else shutil.which('cc') or shutil.which('gcc') or shutil.which('clang'))
if not cc:
    raise SystemExit('A C compiler is required. Set CC or use --cc /path/to/compiler.')
env = os.environ.copy()
env['PATH'] = str(Path(cc).resolve().parent) + os.pathsep + env.get('PATH', '')
(ROOT / 'bin').mkdir(exist_ok=True)
sources = sorted((ROOT / 'compiler').glob('*.c'))
flags = ['-std=c11', '-Wall', '-Wextra']
if args.debug:
    flags += ['-g']
else:
    flags += ['-O2']
if args.sanitize:
    flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
if os.name != 'nt':
    flags += ['-D_POSIX_C_SOURCE=200809L']
cmd = [cc, *flags, *map(str, sources), '-o', str(ROOT / 'bin' / ('tiny' + ext))]
if os.name != 'nt' and sys.platform != 'darwin':
    cmd += ['-ldl']
subprocess.run(cmd, cwd=ROOT, env=env, check=True)
shutil.copy2(ROOT / 'bin' / ('tiny' + ext), ROOT / 'bin' / ('tinyc' + ext))
print('Built', ROOT / 'bin' / ('tiny' + ext))
