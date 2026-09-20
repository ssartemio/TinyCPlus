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


window_source = folder / 'window.tc'
window_source.write_text(r'''
import std.gui;
int main() {
    var window, error = GuiWindow.create(width: 12, height: 8, title: "CI", headless: true);
    if (error != 0)
        return error;
    defer window.destroy();
    Surface canvas = window.surface();
    canvas.clear(Pixel.rgba(1, 2, 3));
    int changed = window.present();
    assert(changed == 96);
    GuiEvent event;
    event.kind = 6;
    event.key = 77;
    assert(window.post(event) == 0);
    GuiEvent received = window.nextEvent(timeout: 0);
    assert(received.kind == 6);
    assert(received.key == 77);
    window.close();
    assert(!window.isOpen());
    return 0;
}
''')
p = run('run', window_source)
assert p.returncode == 0, (p.stdout, p.stderr)
