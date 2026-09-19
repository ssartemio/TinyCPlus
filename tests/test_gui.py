from pathlib import Path
import os
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
folder = ROOT / 'build' / 'tests' / ('gui-' + uuid.uuid4().hex)
folder.mkdir(parents=True)

def run(*args):
    return subprocess.run([str(tiny), *map(str, args)], cwd=folder, capture_output=True, text=True, timeout=30)

source = ROOT / 'examples' / 'gui.tc'
p = run('check', source)
assert p.returncode == 0, (p.stdout, p.stderr)

generated = folder / 'gui.c'
p = run('--emit-c', source, '-o', generated)
assert p.returncode == 0, (p.stdout, p.stderr)
text = generated.read_text(encoding='utf-8')
assert '#include "gui.c"' in text and 'tc_gui_fill_rect' in text, text[:2000]

p = run('run', source)
assert p.returncode == 0, (p.stdout, p.stderr)
value = p.stdout.strip()
assert value.isdigit() and int(value) != 0, p.stdout

print('GUI TinyC+ integration verified: module import, lowering, native execution')
