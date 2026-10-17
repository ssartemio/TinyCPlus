"""Iterator protocol, borrowed cursor lifetime and mutation contracts."""
import argparse
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc')
args = parser.parse_args()
tiny = Path(os.environ.get('TINY_TEST_COMPILER', str(ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')))).resolve()
folder = ROOT / 'build' / 'tests' / ('iterators-' + uuid.uuid4().hex)
folder.mkdir(parents=True)


def run(source, expected, diagnostic=None, output=None):
    command = [str(tiny), 'run', str(source), '--home', str(ROOT)]
    if args.cc:
        command += ['--cc', args.cc]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
    assert result.returncode == expected, (source, result.returncode, result.stdout, result.stderr)
    if diagnostic:
        assert diagnostic in result.stderr, result.stderr
    if output:
        assert result.stdout == output, result.stdout


run(ROOT / 'tests/iterators.tc', 0, output='iterators verified\n')
owners = {
    'array': ('var c=Array<int>.create();c.push(1);', ['c.push(2);', 'c.pop();', 'c.set(0,2);', 'c.reserve(c.capacity);', 'c.clear();', 'c.destroy();']),
    'deque': ('var c=Deque<int>.create();c.pushBack(1);', ['c.pushFront(2);', 'c.pushBack(2);', 'c.popFront();', 'c.popBack();', 'c.set(0,2);', 'c.reserve(c.capacity);', 'c.clear();', 'c.destroy();']),
    'map': ('var c=Map<int,int>.create();c.put(1,1);', ['c.put(1,2);', 'c.put(2,2);', 'c.remove(1);', 'c.remove(99);', 'c.clear();', 'c.destroy();']),
    'set': ('var c=Set<int>.create();c.add(1);', ['c.add(1);', 'c.add(2);', 'c.remove(1);', 'c.remove(99);', 'c.clear();', 'c.destroy();']),
    'heap': ('var c=PriorityQueue<int>.create(less);c.push(1);', ['c.push(2);', 'c.pop();', 'c.clear();', 'c.destroy();']),
    'bits': ('var c=BitSet.create(65);c.set(64);', ['c.set(64);', 'c.reset(64);', 'c.unionWith(&c);', 'c.intersectWith(&c);', 'c.differenceWith(&c);', 'c.clear();', 'c.destroy();']),
    'ordered': ('var c=OrderedMap<int,int>.create();c.put(1,1);', ['c.put(1,2);', 'c.put(2,2);', 'c.remove(1);', 'c.remove(99);', 'c.clear();', 'c.destroy();']),
}
checks = 0
for name, (setup, mutations) in owners.items():
    for index, mutation in enumerate(mutations):
        source = folder / f'{name}-{index}.tc'
        source.write_text('import std.collections;bool less(int a,int b){return a<b;}int main(){' + setup + 'var it=c.iterator();' + mutation + 'it.next();return 0;}')
        run(source, 101, 'assertion failed')
        checks += 1
    # Verify that for borrows the actual owner, including a loop that would
    # otherwise appear exhausted immediately after its first iteration.
    source = folder / f'{name}-for.tc'
    source.write_text('import std.collections;bool less(int a,int b){return a<b;}int main(){' + setup + 'for(v in c){c.clear();}return 0;}')
    run(source, 101, 'assertion failed')
    checks += 1

invalid = {
    'missing_next': ('class I{}class C{I iterator(){I i;return i;}}int main(){C c;for(v in c){}return 0;}', 'next() method'),
    'bad_next': ('class I{int next(){return 1;}}class C{I iterator(){I i;return i;}}int main(){C c;for(v in c){}return 0;}', 'next() must return'),
    'bad_bool': ('class I{(int,int,int) next(){return (0,0,0);}}class C{I iterator(){I i;return i;}}int main(){C c;for(v in c){}return 0;}', 'next() must return'),
    'next_argument': ('class I{(int,int,bool) next(int x){return (0,0,false);}}class C{I iterator(){I i;return i;}}int main(){C c;for(v in c){}return 0;}', 'next() method'),
    'static_iterator': ('class I{}class C{static I iterator(){I i;return i;}}int main(){C c;for(v in c){}return 0;}', 'iterator() must be'),
    'iterator_argument': ('class I{}class C{I iterator(int x){I i;return i;}}int main(){C c;for(v in c){}return 0;}', 'iterator() must be'),
    'temporary_owner': ('import std.collections;int main(){for(v in Array<int>.create()){}return 0;}', 'collection variable or pointer'),
}
for name, (source, diagnostic) in invalid.items():
    path = folder / (name + '.tc')
    path.write_text(source)
    run(path, 1, diagnostic)
print(f'Iterators verified: seven collections, explicit cursors, structural protocol, async/control flow, {checks} mutation failures, {len(invalid)} protocol diagnostics')
