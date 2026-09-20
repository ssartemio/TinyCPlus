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

generated = folder / 'emitted_gui.c'
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


layout_source = folder / 'layout.tc'
layout_source.write_text(r'''
import std.gui;
int main() {
    var surface, error = Surface.create(80, 24);
    if (error != 0)
        return error;
    defer surface.destroy();

    GuiRect area = GuiRect(2, 2, 76, 20);
    GuiRect left = GuiLayout.row(area, 2, 4, 0);
    GuiRect right = GuiLayout.row(area, 2, 4, 1);
    assert(left.contains(3, 3));
    assert(!left.contains(79, 3));
    assert(right.x > left.x + left.width);

    surface.clear(Pixel.rgba(16, 24, 32));
    GuiDraw.button(surface, left, "OK");
    GuiDraw.button(surface, right, "CANCEL", pressed: true);
    GuiDraw.label(surface, GuiRect(0, 0, 80, 2), "TINYC+", Pixel.rgba(255, 255, 255),
                  centered: true);
    assert(surface.checksum() != 0);
    return 0;
}
''')
p = run('run', layout_source)
assert p.returncode == 0, (p.stdout, p.stderr)


textbox_source = folder / 'textbox.tc'
textbox_source.write_text(r'''
import std.gui;
int main() {
    var surface, error = Surface.create(120, 30);
    if (error != 0)
        return error;
    defer surface.destroy();

    GuiTextBox box = GuiTextBox.create("abc");
    defer box.destroy();

    GuiEvent text;
    text.kind = 2;
    text.codepoint = 88;
    assert(box.handleEvent(text) == 0);
    assert(box.dirty());

    OwnedString value = box.text();
    defer value.destroy();
    assert(value.view() == "Xabc");

    GuiRect area = GuiRect(2, 2, 100, 18);
    surface.clear(Pixel.rgba(16, 24, 32));
    box.draw(surface, area, focused: true);
    assert(surface.checksum() != 0);
    box.markClean();
    assert(!box.dirty());
    return 0;
}
''')
p = run('run', textbox_source)
assert p.returncode == 0, (p.stdout, p.stderr)


form_source = ROOT / 'examples' / 'gui_form.tc'
p = run('check', form_source)
assert p.returncode == 0, (p.stdout, p.stderr)


coexist_source = folder / 'coexist.tc'
coexist_source.write_text(r'''
import std.tui;
import std.gui;

int main() {
    var terminal, terminalError = Window.create(width: 20, height: 5, headless: true);
    if (terminalError != 0)
        return terminalError;
    defer terminal.destroy();

    Widget label = Ui.label("TUI");
    terminal.setContent(label);
    terminal.draw();

    var graphical, guiError = GuiWindow.create(width: 20, height: 10, title: "GUI", headless: true);
    if (guiError != 0)
        return guiError;
    defer graphical.destroy();

    Surface canvas = graphical.surface();
    canvas.clear(Pixel.rgba(1, 2, 3));
    GuiDraw.label(canvas, GuiRect(0, 0, 20, 10), "GUI", Pixel.rgba(255, 255, 255),
                  centered: true);
    assert(graphical.present() > 0);
    return 0;
}
''')
p = run('run', coexist_source)
assert p.returncode == 0, (p.stdout, p.stderr)
