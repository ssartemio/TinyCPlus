# API reference

Source: `std/tui.tc`

Module: `std.tui`

## tc_gap_create

```c
void* tc_gap_create(string input);
```

## tc_gap_destroy

```c
void tc_gap_destroy(void* buffer);
```

## tc_gap_length

```c
u64 tc_gap_length(void* buffer);
```

## tc_gap_position

```c
u64 tc_gap_position(void* buffer);
```

## tc_gap_seek

```c
void tc_gap_seek(void* buffer, u64 position);
```

## tc_gap_insert

```c
i32 tc_gap_insert(void* buffer, string input);
```

## tc_gap_left

```c
void tc_gap_left(void* buffer);
```

## tc_gap_right

```c
void tc_gap_right(void* buffer);
```

## tc_gap_backspace

```c
void tc_gap_backspace(void* buffer);
```

## tc_gap_delete

```c
void tc_gap_delete(void* buffer);
```

## tc_gap_home

```c
void tc_gap_home(void* buffer);
```

## tc_gap_end

```c
void tc_gap_end(void* buffer);
```

## tc_gap_line

```c
i32 tc_gap_line(void* buffer);
```

## tc_gap_column

```c
i32 tc_gap_column(void* buffer);
```

## tc_gap_vertical

```c
void tc_gap_vertical(void* buffer, i32 direction);
```

## tc_gap_goto_line

```c
void tc_gap_goto_line(void* buffer, i32 line);
```

## tc_gap_dirty

```c
i32 tc_gap_dirty(void* buffer);
```

## tc_gap_clean

```c
void tc_gap_clean(void* buffer);
```

## tc_gap_text

```c
string tc_gap_text(void* buffer);
```

## tc_gap_find

```c
i64 tc_gap_find(void* buffer, string needle, u64 start);
```

## tc_gap_cut_line

```c
string tc_gap_cut_line(void* buffer);
```

## tc_ui_create

```c
void* tc_ui_create(i32 width, i32 height, i32 headless, i32* error);
```

## tc_ui_width_value

```c
i32 tc_ui_width_value(void* window);
```

## tc_ui_height_value

```c
i32 tc_ui_height_value(void* window);
```

## tc_ui_resize

```c
i32 tc_ui_resize(void* window, i32 width, i32 height);
```

## tc_ui_clear

```c
void tc_ui_clear(void* window, u32 foreground, u32 background);
```

## tc_ui_put

```c
void tc_ui_put(void* window, i32 x, i32 y, u32 codepoint, u32 foreground, u32 background);
```

## tc_ui_text

```c
void tc_ui_text(void* window, i32 x, i32 y, string text, u32 foreground, u32 background);
```

## tc_ui_render

```c
string tc_ui_render(void* window);
```

## tc_ui_changed

```c
i32 tc_ui_changed(void* window);
```

## tc_ui_refresh

```c
void tc_ui_refresh(void* window);
```

## tc_ui_snapshot

```c
string tc_ui_snapshot(void* window);
```

## tc_ui_post

```c
i32 tc_ui_post(void* window, i32 kind, i32 key, i32 x, i32 y, u32 codepoint);
```

## tc_ui_next

```c
void tc_ui_next(void* window, i32 timeout, i32* kind, i32* key, i32* x, i32* y, i32* width, i32* height, u32* codepoint);
```

## tc_ui_replay

```c
i32 tc_ui_replay(void* window, string input, u64* offset);
```

## tc_widget_create

```c
void* tc_widget_create(i32 kind, string text, i32 width, i32 height);
```

## tc_widget_destroy

```c
void tc_widget_destroy(void* widget);
```

## tc_widget_add

```c
i32 tc_widget_add(void* parent, void* child);
```

## tc_widget_text

```c
void tc_widget_text(void* widget, string text);
```

## tc_widget_get_text

```c
string tc_widget_get_text(void* widget);
```

## tc_widget_buffer

```c
void* tc_widget_buffer(void* widget);
```

## tc_widget_on_click

```c
void tc_widget_on_click(void* widget, void* closure);
```

## tc_widget_size

```c
void tc_widget_size(void* widget, i32 width, i32 height);
```

## tc_ui_focus

```c
void tc_ui_focus(void* window, void* widget);
```

## tc_ui_content

```c
void tc_ui_content(void* window, void* widget);
```

## tc_ui_draw

```c
void tc_ui_draw(void* window);
```

## tc_ui_dispatch

```c
i32 tc_ui_dispatch(void* window, i32 kind, i32 key, i32 x, i32 y, i32 width, i32 height, u32 codepoint);
```

## tc_ui_destroy

```c
void tc_ui_destroy(void* window);
```

## GapBuffer

Byte gap buffer, with movement/deletion on UTF-8 codepoint boundaries.

```c
class GapBuffer
```

### GapBuffer.handle

```c
void* handle;
```

### GapBuffer.create

```c
static GapBuffer create(string input = "");
```

### GapBuffer.length

```c
u64 length();
```

### GapBuffer.position

```c
u64 position();
```

### GapBuffer.seek

```c
void seek(u64 position);
```

### GapBuffer.insert

```c
i32 insert(string text);
```

### GapBuffer.left

```c
void left();
```

### GapBuffer.right

```c
void right();
```

### GapBuffer.backspace

```c
void backspace();
```

### GapBuffer.deleteNext

```c
void deleteNext();
```

### GapBuffer.home

```c
void home();
```

### GapBuffer.end

```c
void end();
```

### GapBuffer.line

```c
i32 line();
```

### GapBuffer.column

```c
i32 column();
```

### GapBuffer.up

```c
void up();
```

### GapBuffer.down

```c
void down();
```

### GapBuffer.gotoLine

```c
void gotoLine(i32 line);
```

### GapBuffer.dirty

```c
bool dirty();
```

### GapBuffer.markClean

```c
void markClean();
```

### GapBuffer.text

```c
OwnedString text();
```

### GapBuffer.find

```c
i64 find(string needle, u64 start = 0);
```

### GapBuffer.cutLine

```c
OwnedString cutLine();
```

### GapBuffer.destroy

```c
void destroy();
```

## UiEvent

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
class UiEvent
```

### UiEvent.kind

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
i32 kind;
```

### UiEvent.key

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
i32 key;
```

### UiEvent.x

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
i32 x;
```

### UiEvent.y

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
i32 y;
```

### UiEvent.width

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
i32 width;
```

### UiEvent.height

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
i32 height;
```

### UiEvent.codepoint

kind: 1 key, 2 resize, 3 timer, 4 mouse, 5 custom.
key: ASCII/control codes; arrows 1001..1004, home/end 1005/1006,
delete 1007, page-up/down 1008/1009. codepoint carries Unicode input.

```c
u32 codepoint;
```

## Widget

add transfers child ownership to parent. Window.setContent owns the root.

```c
class Widget
```

### Widget.handle

```c
void* handle;
```

### Widget.add

```c
i32 add(Widget child);
```

### Widget.setText

```c
void setText(string text);
```

### Widget.text

```c
OwnedString text();
```

### Widget.buffer

Borrowed buffer owned by the TextBox: do not destroy it separately.

```c
GapBuffer buffer();
```

### Widget.size

```c
void size(i32 width, i32 height);
```

### Widget.onClick

An owned closure transfers to the widget. A borrowed closure must outlive it.

```c
void onClick(closure<void()> action);
```

### Widget.destroy

```c
void destroy();
```

## Ui

```c
class Ui
```

### Ui.label

```c
static Widget label(string text);
```

### Ui.button

```c
static Widget button(string text);
```

### Ui.textBox

```c
static Widget textBox(string text = "", i32 height = 0);
```

### Ui.panel

```c
static Widget panel(string title = "");
```

### Ui.row

```c
static Widget row(i32 height = 1);
```

### Ui.column

```c
static Widget column();
```

### Ui.stack

```c
static Widget stack();
```

## Window

```c
class Window
```

### Window.handle

```c
void* handle;
```

### Window.create

```c
static (Window, i32) create(i32 width = 80, i32 height = 24, bool headless = false);
```

### Window.width

```c
i32 width();
```

### Window.height

```c
i32 height();
```

### Window.resize

```c
i32 resize(i32 width, i32 height);
```

### Window.clear

```c
void clear(u32 foreground = 15134195, u32 background = 1054752);
```

### Window.put

```c
void put(i32 x, i32 y, u32 codepoint, u32 foreground = 15134195, u32 background = 1054752);
```

### Window.text

```c
void text(i32 x, i32 y, string text, u32 foreground = 15134195, u32 background = 1054752);
```

### Window.render

```c
OwnedString render();
```

### Window.changed

```c
i32 changed();
```

### Window.refresh

```c
void refresh();
```

### Window.snapshot

```c
OwnedString snapshot();
```

### Window.setContent

```c
void setContent(Widget root);
```

### Window.focus

```c
void focus(Widget widget);
```

### Window.draw

```c
void draw();
```

### Window.nextEvent

```c
UiEvent nextEvent(i32 timeout = 100);
```

### Window.post

```c
i32 post(UiEvent event);
```

### Window.replay

```c
i32 replay(string input, u64* offset);
```

### Window.dispatch

```c
i32 dispatch(UiEvent event);
```

### Window.destroy

```c
void destroy();
```

