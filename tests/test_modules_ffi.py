from pathlib import Path
import argparse
import os
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
parser = argparse.ArgumentParser()
parser.add_argument('--cc')
args = parser.parse_args()
folder = ROOT / 'build' / 'tests' / ('modules-' + uuid.uuid4().hex)
folder.mkdir(parents=True)
os.environ['TMP'] = os.environ['TEMP'] = str(folder)

def run(source):
    path = folder / 'main.tc'
    path.write_text(source)
    command = [str(tiny), 'run', str(path)]
    if args.cc:
        command += ['--cc', args.cc]
    return subprocess.run(command, capture_output=True, text=True, timeout=30)

(folder / 'left.tc').write_text('module left; import right; int get(){return 10;} int own(){return get();} class Point {int value;}')
(folder / 'right.tc').write_text('module right; import left; int get(){return 20;} int own(){return get();}')
p = run('import left; import right; int main(){left.Point p(3);println(left.get()+right.get()+p.value);println(left.own()+right.own());var q=left.Point(4);println(q.value);}')
assert p.returncode == 0 and p.stdout == '33\n30\n4\n', (p.stdout, p.stderr)
p = run('import left;import right;int main(){println(get());}')
assert p.returncode == 1 and 'ambiguous imported name' in p.stderr, p.stderr
p = run('import left;int main(){println(left.missing());}')
assert p.returncode == 1 and 'has no declaration' in p.stderr
p = run('import left;int main(){int get=3;println(get);}')
assert p.returncode == 0 and p.stdout == '3\n', p.stderr

command = [str(tiny), 'run', str(ROOT / 'examples' / 'ffi.tc'), '--c-source', str(ROOT / 'examples' / 'ffi_math.c')]
if args.cc:
    command += ['--cc', args.cc]
p = subprocess.run(command, capture_output=True, text=True, timeout=30)
assert p.returncode == 0 and p.stdout == '11\n16\n1\n', (p.stdout, p.stderr)
print('Modules/FFI verified: cyclic imports, qualified types/functions, duplicate function names, local lookup, C structs/enums/callback ABI')
