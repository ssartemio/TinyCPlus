from pathlib import Path
import os
import re
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
folder = ROOT / 'build' / 'tests' / ('tools-' + uuid.uuid4().hex)
folder.mkdir(parents=True)

def run(*args, input=None):
    return subprocess.run([str(tiny), *map(str, args)], input=input, cwd=folder, capture_output=True, text=True, timeout=20)

tests = folder / 'tests.tc'
tests.write_text('''
@test void addition() { assert(2 + 3 == 5); }
@test int named() { return 0; }
int main() { assert(false); return 1; }
''')
p = run('test', tests)
assert p.returncode == 0 and 'PASS addition' in p.stdout and '2 tests executed; passed' in p.stdout, (p.stdout, p.stderr)
tests.write_text('@test int failure() { return 1; }')
p = run('test', tests)
assert p.returncode == 1 and 'FAIL failure' in p.stdout
tests.write_text('@test void failure() { assert(false); }')
p = run('test', tests)
assert p.returncode == 101 and 'assertion failed' in p.stderr

source = folder / 'format.tc'
source.write_text('''/// The sample entry point.
int main(){int[3] values={1,2,3}; // preserve this comment
for(int i=0;i<3;i++){if(i==1){println("a\\x00b");}else println(values[i]);}return 0;} /* tail */
''')
before = run('run', source)
p = run('fmt', source)
assert p.returncode == 0, p.stderr
formatted = source.read_text()
assert '/// The sample' in formatted and '// preserve this comment' in formatted and '/* tail */' in formatted
assert '\n    int[3] values' in formatted
p = run('fmt', source)
assert p.returncode == 0 and source.read_text() == formatted
after = run('run', source)
assert before.returncode == after.returncode == 0 and before.stdout == after.stdout, after.stderr

switch_source = folder / 'switch_format.tc'
switch_source.write_text('''enum Color{Red,Green,Blue}
int f(int x,Color c){int default=0;int r=0;
switch(x){case -1:r=1;case 0,1:{r=2;}default:default=3;r=default+r;}
switch(c){case Color.Red:return 1;case Color.Green,Color.Blue:return -2;}}
int main(){println(f(0,Color.Red));println(f(5,Color.Blue));}
''')
before = run('run', switch_source)
assert before.returncode == 0 and before.stdout.split() == ['1', '-2'], before.stderr
p = run('fmt', switch_source)
assert p.returncode == 0, p.stderr
formatted = switch_source.read_text()
assert ('    switch (x) {\n        case -1:\n            r = 1;\n        case 0, 1: {\n            r = 2;\n        }\n'
        '        default:\n            default = 3;\n            r = default + r;\n    }\n') in formatted, formatted
assert '        case Color.Green, Color.Blue:\n            return -2;\n' in formatted, formatted
p = run('fmt', switch_source)
assert p.returncode == 0 and switch_source.read_text() == formatted
after = run('run', switch_source)
assert after.returncode == 0 and after.stdout == before.stdout, after.stderr
docs = folder / 'api.md'
p = run('doc', source, '-o', docs)
assert p.returncode == 0 and 'The sample entry point.' in docs.read_text() and 'i32 main();' in docs.read_text(), (p.stderr, docs.read_text())
source.write_text('enum Color { Red = -1, Green = 2 }')
p = run('doc', source, '-o', docs)
assert p.returncode == 0 and 'Red = -1' in docs.read_text() and 'Green = 2' in docs.read_text(), docs.read_text()
source.write_text('class Buffer<T>{static (Array<T>,Error) read(func<T(int)> callback);} ')
p = run('doc', source, '-o', docs)
assert p.returncode == 0 and '(Array<T>, i32) read(func<T(i32)> callback)' in docs.read_text(), docs.read_text()

session = '\n'.join(['var x = 10;', 'x * 20', 'x++;', 'x',
    'int twice(int value) { return value * 2; }', 'twice(x)', 'const int fixed = 7;', 'fixed',
    'var f = twice;', 'f(3)', 'f = (int v)=>v+20; return 1;', 'f(3)', ':reset', 'var x = 4;', 'x', ':quit', ''])
p = run('repl', input=session)
assert p.returncode == 0 and not p.stderr, (p.stdout, p.stderr)
numbers = re.findall(r'(?:>>> )+(\d+)\n', p.stdout)
assert numbers == ['200', '11', '22', '7', '6', '23', '4'], (numbers, p.stdout)
assert not list(folder.glob('tcr*.tmp'))
print('Tooling verified: @test pass/fail, formatter preservation/idempotence, documentation, persistent REPL and function pointers')
