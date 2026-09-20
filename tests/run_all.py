#!/usr/bin/env python3
"""Reproducible local suite. Interop dependencies: pip install -r tests/requirements.txt."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc', help='Also compare every language case against this C compiler')
parser.add_argument('--fuzz', type=int, default=1000)
parser.add_argument('--skip-interop', action='store_true', help='Explicitly omit grpcio/protobuf checks')
parser.add_argument('--report', default=str(ROOT / 'build' / 'test-report.json'))
args = parser.parse_args()
ext = '.exe' if os.name == 'nt' else ''
native = ROOT / 'build' / 'native-tests'
native.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
if args.cc:
    env['PATH'] = str(Path(shutil.which(args.cc) or args.cc).resolve().parent) + os.pathsep + env.get('PATH', '')
records = []

def run(label, command, timeout=600):
    print('\n' + label, flush=True)
    start = time.monotonic()
    try:
        p = subprocess.run(list(map(str, command)), cwd=ROOT, env=env, capture_output=True, text=True, timeout=timeout)
        print(p.stdout, end='', flush=True)
        if p.stderr:
            print(p.stderr, file=sys.stderr, end='', flush=True)
        records.append(dict(test=label, seconds=round(time.monotonic()-start, 3), exit_code=p.returncode,
                            stdout=p.stdout, stderr=p.stderr))
        return p.returncode == 0
    except subprocess.TimeoutExpired:
        records.append(dict(test=label, seconds=timeout, exit_code=124, stderr='timeout'))
        return False

run('Language, diagnostics, lowering and deterministic fuzzing', [sys.executable, ROOT/'tests/test_compiler.py', '--fuzz', args.fuzz, *(['--cc',args.cc] if args.cc else [])])
for name in ('test_modules_ffi.py', 'test_tools.py', 'test_stdlib.py', 'test_selfhost.py', 'test_editor.py', 'test_golden.py', 'test_tutorial.py'):
    command = [sys.executable, ROOT/'tests'/name]
    if name in ('test_tutorial.py', 'test_stdlib.py', 'test_selfhost.py') and args.cc:
        command += ['--cc', args.cc]
    run(name, command)
bundled = ROOT/'third_party/tcc'/('tcc.exe' if os.name == 'nt' else 'posix/tcc')
compilers = [str(bundled)] if bundled.exists() else [shutil.which('cc') or 'cc']
if args.cc and args.cc not in compilers:
    compilers.append(args.cc)
for cc in compilers:
    for name in ('concurrent', 'network', 'tui'):
        output = native/('runtime_'+name+ext)
        flags = ['-std=c11', '-I'+str(ROOT/'runtime')]
        if os.name != 'nt':
            flags += ['-D_POSIX_C_SOURCE=200809L', '-pthread', '-lm']
        elif name == 'network':
            flags += ['-lws2_32']
        if run('Build runtime '+name+' with '+Path(cc).name, [cc, ROOT/'tests'/('runtime_'+name+'.c'), *flags, '-o', output]):
            run('Run runtime '+name+' with '+Path(cc).name, [output], timeout=30)
if not args.skip_interop:
    for cc in [None, *([args.cc] if args.cc else [])]:
        for name in ('test_protobuf.py','test_grpc.py'):
            run(name+(' / '+Path(cc).name if cc else ' / default backend'),
                [sys.executable, ROOT/'tests'/name, *(['--cc',cc] if cc else [])])
if args.cc:
    run('External compiler module and FFI ABI checks', [sys.executable,ROOT/'tests/test_modules_ffi.py','--cc',args.cc])
report = Path(args.report)
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(dict(platform=sys.platform,python=sys.version,interop_skipped=args.skip_interop,
                                 results=records),indent=2),encoding='utf-8')
failed = sum(item['exit_code'] != 0 for item in records)
print(f'\n{len(records)-failed}/{len(records)} stages passed. Report: {report}')
raise SystemExit(bool(failed))
