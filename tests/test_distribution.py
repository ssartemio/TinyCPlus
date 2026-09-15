"""Verify archive checksums and run the extracted toolchain in a fresh directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid
import zipfile
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('archive')
args=parser.parse_args()
folder=ROOT/'build'/('relocation space '+uuid.uuid4().hex)
folder.mkdir(parents=True)
with zipfile.ZipFile(args.archive) as z:
    for entry in z.infolist():
        if not (folder/entry.filename).resolve().is_relative_to(folder.resolve()):
            raise AssertionError('Unsafe archive path')
    z.extractall(folder)
copy=folder/'tinycplus'
manifest=(copy/'SHA256SUMS.txt').read_text().splitlines()
for line in manifest:
    digest,name=line.split('  ',1)
    assert hashlib.sha256((copy/name).read_bytes()).hexdigest()==digest,name
env=os.environ.copy()
env.pop('TINY_HOME',None)
env.pop('LIBTCC_PATH',None)
ext='.exe' if os.name=='nt' else ''
tiny=copy/'bin'/('tiny'+ext)
records=[]
def run(label,command,expected=None,input=None):
    p=subprocess.run(list(map(str,command)),cwd=copy,env=env,input=input,capture_output=True,text=True,timeout=120)
    assert p.returncode==0,(label,p.returncode,p.stdout,p.stderr)
    if expected is not None:
        assert p.stdout==expected,(label,p.stdout,expected)
    records.append(dict(test=label,exit_code=p.returncode,stdout=p.stdout))
run('version',[tiny,'--version'])
run('hello',[tiny,'run','examples/hello.tc'],'30\n')
run('async',[tiny,'run','examples/async.tc'],'42\n')
run('generated grpc service/client',[tiny,'run','examples/grpc_users.tc'],'Ana\ndeveloper\n')
run('C FFI',[tiny,'run','examples/ffi.tc','--c-source','examples/ffi_math.c'])
run('native @test',[tiny,'test','examples/tests.tc'])
run('REPL',[tiny,'repl'],input='var n=40;\nn+2\n:quit\n')
run('compiler rebuild',[sys.executable,copy/'build.py'])
run('rebuilt hello',[tiny,'run','examples/hello.tc'],'30\n')
run('editor replay',[sys.executable,copy/'tests/test_editor.py'])
report=dict(archive=Path(args.archive).name,checked_files=len(manifest),relocated=True,tests=records)
(ROOT/'build/distribution-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(f'Distribution verified: {len(manifest)} checksums; {len(records)} relocated checks; compiler rebuilt from sources')
