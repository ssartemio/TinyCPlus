"""Exact portable snapshots for selected lowering contracts; explicit update mode."""
import argparse
import os
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--update',action='store_true')
args=parser.parse_args()
tiny=ROOT/'bin'/('tiny.exe' if os.name=='nt' else 'tiny')
count=0
for source in sorted((ROOT/'tests/golden').glob('*.tc')):
    p=subprocess.run([str(tiny),'--emit-c',str(source)],capture_output=True,text=True,timeout=15)
    assert p.returncode==0,p.stderr
    code=re.sub(r'(#line \d+ )"[^"]+"',r'\1"<source>"',p.stdout)
    expected=source.with_suffix('.expected.c')
    if args.update:
        expected.write_text(code,encoding='utf-8')
    else:
        assert expected.read_text(encoding='utf-8')==code, f'Lowering snapshot changed: {source.name}'
    count+=1
print(f'{count} C lowering snapshots '+('updated' if args.update else 'verified'))
