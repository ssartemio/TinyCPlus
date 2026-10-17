"""Model-based collection checks and assertion contracts, on either C backend."""
import argparse
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc', help='External compiler instead of libtcc')
parser.add_argument('--only', choices=('all', 'collections', 'clones'), default='all')
args = parser.parse_args()
tiny = Path(os.environ.get('TINY_TEST_COMPILER', str(ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')))).resolve()
folder = ROOT / 'build' / 'tests' / ('collections-' + uuid.uuid4().hex)
folder.mkdir(parents=True)


def check(source, expected, output=None):
    command = [str(tiny), 'run', str(source), '--home', str(ROOT)]
    if args.cc:
        command += ['--cc', args.cc]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
    assert result.returncode == expected, (source, result.returncode, result.stdout, result.stderr)
    if output is not None:
        assert result.stdout == output, (source, result.stdout, result.stderr)
    if expected == 101:
        assert 'assertion failed' in result.stderr, result.stderr


if args.only in ('all', 'collections'):
    check(ROOT / 'tests' / 'collections.tc', 0, 'collections verified\n')
if args.only in ('all', 'clones'):
    check(ROOT / 'tests' / 'clones.tc', 0, 'clones verified\n')
failures = {
    'deque_empty_front': 'var q=Deque<int>.create();defer q.destroy();q.popFront();',
    'deque_empty_back': 'var q=Deque<int>.create();defer q.destroy();q.popBack();',
    'deque_index': 'var q=Deque<int>.create();defer q.destroy();q.pushBack(1);q.get(1);',
    'deque_capacity_overflow': 'var q=Deque<int>.create();defer q.destroy();q.reserve(18446744073709551615);',
    'heap_empty_pop': 'var q=PriorityQueue<int>.create(less);defer q.destroy();q.pop();',
    'heap_empty_peek': 'var q=PriorityQueue<int>.create(less);defer q.destroy();q.peek();',
    'heap_destroyed': 'var q=PriorityQueue<int>.create(less);q.destroy();q.push(1);',
    'bitset_index': 'var b=BitSet.create(64);defer b.destroy();b.set(64);',
    'bitset_empty_index': 'var b=BitSet.create(0);defer b.destroy();b.contains(0);',
    'bitset_size_mismatch': 'var a=BitSet.create(64);defer a.destroy();var b=BitSet.create(65);defer b.destroy();a.unionWith(&b);',
    'ordered_index': 'var m=OrderedMap<int,int>.create();defer m.destroy();m.keyAt(0);',
}
if args.only in ('all', 'collections'):
    for label, body in failures.items():
        source = folder / (label + '.tc')
        source.write_text('import std.collections;bool less(int a,int b){return a<b;}int main(){' + body + 'return 0;}')
        check(source, 101)
if args.only == 'clones':
    print('Clones verified: seven containers, independent storage, empty copies, order, comparator and shallow element ownership')
else:
    print(f'Collections verified: deterministic reference models, custom priorities, ownership cleanup, {len(failures)} assertion contracts')
