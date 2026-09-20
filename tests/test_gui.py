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
    event.kind = GuiEventKind.Custom;
    event.key = 77;
    assert(window.post(event) == 0);
    GuiEvent received = window.nextEvent(timeout: 0);
    assert(received.kind == GuiEventKind.Custom);
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
    text.kind = GuiEventKind.Text;
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


input_source = folder / 'input_compat.tc'
input_source.write_text(r'''
import std.input;
import std.gui;
import std.tui;

int main() {
    assert(GuiEventKind.Key == 1);
    assert(GuiEventKind.Close == 5);
    assert(GuiMouseButton.Left == 1);
    assert(InputKey.Left == 1001);
    assert(InputKey.Right == 1002);
    assert(InputKey.Delete == 1007);
    assert(InputKey.PageDown == 1009);

    GuiEvent graphical;
    graphical.kind = GuiEventKind.Key;
    graphical.key = InputKey.Left;
    assert(GuiInput.key(graphical, InputKey.Left));

    var terminal, error = Window.create(width: 10, height: 3, headless: true);
    if (error != 0)
        return error;
    defer terminal.destroy();

    Widget input = Ui.textBox("x");
    terminal.setContent(input);
    terminal.focus(input);

    UiEvent event;
    event.kind = GuiEventKind.Key;
    event.key = InputKey.End;
    assert(terminal.dispatch(event) == 0);
    event.key = InputKey.Left;
    assert(terminal.dispatch(event) == 0);
    return 0;
}
''')
p = run('run', input_source)
assert p.returncode == 0, (p.stdout, p.stderr)


button_focus_source = folder / 'button_focus.tc'
button_focus_source.write_text(r'''
import std.gui;

int main() {
    GuiFocus focus = GuiFocus(2);
    assert(focus.isFocused(0));

    GuiEvent tab;
    tab.kind = GuiEventKind.Key;
    tab.key = 9;
    assert(focus.handleEvent(tab));
    assert(focus.isFocused(1));
    focus.previous();
    assert(focus.isFocused(0));

    GuiButton button = GuiButton("OK");
    GuiRect area = GuiRect(10, 10, 80, 24);

    GuiEvent press;
    press.kind = GuiEventKind.Mouse;
    press.button = GuiMouseButton.Left;
    press.pressed = 1;
    press.x = 20;
    press.y = 15;
    assert(!button.handleEvent(press, area));
    assert(button.down);

    GuiEvent release = press;
    release.pressed = 0;
    assert(button.handleEvent(release, area));
    assert(!button.down);

    GuiEvent enter;
    enter.kind = GuiEventKind.Key;
    enter.key = 13;
    assert(button.handleEvent(enter, area, focused: true));
    assert(!button.handleEvent(enter, area, focused: false));
    return 0;
}
''')
p = run('run', button_focus_source)
assert p.returncode == 0, (p.stdout, p.stderr)
