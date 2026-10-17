"""Regressions for the functional review; both compiler backends are exercised."""
import json
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import shutil

ROOT = Path(__file__).resolve().parents[1]
TINY = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
parser = argparse.ArgumentParser()
parser.add_argument("--sanitize", action="store_true")
parser.add_argument("--cc")
options = parser.parse_args()
EXTERNAL = options.cc or os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc') or shutil.which('clang')
folder = Path(tempfile.mkdtemp(prefix='review-', dir=ROOT / 'build'))

def command(*args, expected=0):
    result = subprocess.run([str(TINY), *map(str, args)], cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result

def source(name, text):
    path = folder / name
    path.write_text(text, encoding='utf-8')
    return path

program = source('safe.tc', r'''
import std.string;
import std.collections;
import std.errors;
import std.net;
import std.gui;
int main() {
    StringBuilder b;
    b.append("abc"); b.append(b.view()); assert(b.view() == "abcabc");
    b.clear(); b.append("012345678901234567890123456789012345678901234567890123456789012");
    b.append(b.view()); assert(b.length == 126); assert(b.view()[63] == '0');
    b.append(b.view()[1:4]); assert(b.view()[126:129] == "123"); b.destroy();
    Array<int> a; var v, found = a.tryPop(); assert(!found && v == 0);
    a.push(7); var x, ok = a.tryPeek(); assert(ok && x == 7);
    var y, present = a.tryGet(1); assert(!present);
    var z, popped = a.tryPop(); assert(popped && z == 7); a.destroy();
    Array<int> transfer; transfer.push(9); var moved = transfer.take(); assert(transfer.length == 0 && moved.get(0) == 9); transfer.destroy(); moved.destroy();
    TcpSocket emptySocket; assert(emptySocket.close() == 0); assert(emptySocket.close() == 0);
    Deque<int> d; var q, got = d.tryPopBack(); assert(!got); d.pushFront(8);
    var r, taken = d.tryPopFront(); assert(taken && r == 8); d.destroy();
    Map<int,int> m; var mv, mf = m.tryGet(1); assert(!mf); m.destroy();
    GuiProgressBar progress(2147483647); progress.set(2147483647); assert(progress.percent() == 100);
    GuiFocus focus(3); GuiEvent event; event.kind = GuiEventKind.Key; event.key = 9;
    event.modifiers = GuiModifier.Shift; assert(focus.handleEvent(event)); assert(focus.index == 2);
    var task = AsyncFile.readAll("build/review-no-such-file-92817");
    var data, error = task.result(); var status = Errors.task(error, task.nativeError(), "readAll");
    assert(status.category == ErrorCategory.NotFound); assert(status.nativeError != 0); task.destroy();
    return 0;
}
''')
for backend in [[], ['--cc', EXTERNAL]]:
    command('run', program, *backend)
if options.sanitize:
    emitted = folder / 'safe.c'
    executable = folder / 'safe-asan'
    command('--emit-c', program, '-o', emitted)
    subprocess.run([EXTERNAL, '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-g', str(emitted), '-I' + str(ROOT / 'runtime'),
                    '-pthread', '-lm', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
# A module owns type identity, including enum constants and generic instances.
for name in ['left', 'right']:
    source(name + '.tc', f'''module {name};
class Item {{ int value; static Item create() {{ Item x; return x; }} }}
T identity<T>(T value) {{ return value; }}
extern C {{ struct Native {{ int value; Native* next; }} }}
enum Choice {{ Yes = 1 }}
class Box<T> {{ T value; static Box<T> create(T value) {{ Box<T> b; b.value = value; return b; }} }}
''')
modules = source('modules.tc', '''import left; import right;
int main() { left.Item a; right.Item b; a.value=3; b.value=4;
left.Box<int> x = left.Box<int>.create(5); right.Box<int> y = right.Box<int>.create(6);
assert(a.value+b.value+x.value+y.value==18);
var own = left.Item.create(); assert(own.value == 0);
assert(left.identity<int>(3) + right.identity<int>(4) == 7);
left.Native foreignLeft; right.Native foreignRight; foreignLeft = foreignRight;
left.Choice c = left.Choice.Yes; right.Choice d = right.Choice.Yes;
assert(c == left.Choice.Yes && d == right.Choice.Yes); return 0; }
''')
for backend in [[], ['--cc', EXTERNAL]]:
    command('run', modules, *backend)
ambiguous = source('ambiguous.tc', 'import left; import right; int main(){ Item x; return 0; }')
assert 'ambiguous imported type' in command('check', ambiguous, expected=1).stderr
wrong = source('wrong.tc', 'import left; import right; int main(){ left.Item a; right.Item b; a=b; return 0; }')
command('check', wrong, expected=1)
source('foreignbad.tc', 'module foreignbad; extern C { struct Native { double value; Native* next; } }')
foreignbad = source('foreign-layout.tc', 'import left; import foreignbad; int main(){return 0;}')
assert 'conflicting extern C layout' in command('check', foreignbad, expected=1).stderr
# Formatting checks preserve bytes, valid JSON checks and typed AST can be consumed.
original = program.read_bytes()
command('fmt', program, '--check', expected=1)
assert program.read_bytes() == original
command('fmt', program)
command('fmt', program, '--check')
assert json.loads(command('check', program, '--json').stdout)['ok']
assert not json.loads(command('check', wrong, '--json', expected=1).stdout)['ok']
ast = json.loads(command('--emit-typed-ast', program).stdout)
assert ast['schemaVersion'] == 1 and ast['nodes']
assert not json.loads(command('check', folder / 'missing.tc', '--json', expected=1).stdout)['ok']
assert not json.loads(command('check', '--project', folder / 'missing.project', '--json', expected=2).stdout)['ok']
assert json.loads(command('capabilities').stdout)['libtccAvailable']
command('doctor')
# Direct owner copies, use after close, and explicit transfer.
for name, body in [('copy', 'var b=a;'), ('double', 'a.destroy(); a.destroy();'), ('use', 'a.destroy(); println(a.view());')]:
    owner = source(name + '.tc', 'import std.string; int main(){var a=String.copy("x");' + body + 'return 0;}')
    assert 'local ownership' in command('check', owner, '--ownership-check', expected=1).stderr
owner = source('take.tc', 'import std.string; int main(){var a=String.copy("x");var b=a.take(); b.destroy();return 0;}')
command('run', owner, '--ownership-check')
# Isolation discovers remaining results after a panic, on both backends.
tests = source('tests.tc', '@test void first(){assert(false);}\n@test void second(){assert(true);}')
assert 'second' in command('test', tests, '--list').stdout
command('test', tests, '--filter', 'no-matches', expected=1)
command('test', tests, '--filter', 'second')
for backend in [[], ['--cc', EXTERNAL]]:
    result = command('test', tests, '--isolate', *backend, expected=1)
    assert 'PASS second' in result.stdout
# Isolated async tests are compiled once and selected by the child dispatcher.
async_tests = source('async-tests.tc', r'''import std.time;
async int value(){var timer=Timer.after(1); defer timer.destroy(); await timer; return 7;}
@test void one(){ var task=value(); assert(task.get()==7); task.destroy(); }
@test void two(){ var task=value(); assert(task.get()==7); task.destroy(); }''')
for backend in [[], ['--cc', EXTERNAL]]:
    command('test', async_tests, '--isolate', *backend)
group = source('group.tc', r'''import std.concurrent;
int slow(CancellationToken* token) { while (!token.cancelled()) tc_sleep_ms(1); return 1; }
int main(){var group=TaskGroup<int>.create(); defer group.destroy();
var first=spawn slow(group.cancellation()); defer first.destroy();
Task<int> failed; failed.handle=tc_future_create(sizeof(int)); defer failed.destroy();
int zero=0; tc_future_complete(failed.handle, &zero, 13); group.add(first); group.add(failed);
assert(group.cancelOnError()==13); assert(group.cancellation().cancelled());
var timer=Future<int>.create(); defer timer.destroy(); Array<void*> handles; defer handles.destroy(); handles.push(timer.handle);
var missing,timeout=TaskWait.any(&handles, 2); assert(missing == -1 && timeout == 4);
timer.complete(9); var found,error=TaskWait.any(&handles, 2); assert(found == 0 && error == 0); return 0;}''')
command('run', group)
if os.name != 'nt':
    process = source('process.tc', r'''import std.process;
int main(){ Array<string> args; args.push("/bin/sh"); args.push("-c"); args.push("exec sleep 5");
var child,error=ProcessHandle.start(&args); assert(error==0); args.destroy();
var code,timed=child.wait(10); assert(code == -1 && timed==4); assert(child.cancel()==0);
var final,waitError=child.wait(1000); assert(waitError==0 && final>=128); child.destroy(); return 0;}''')
    command('run', process)
print('Functional review regressions passed: aliasing, recoverable collections, namespaces, GUI, errors, CLI, ownership, processes')
