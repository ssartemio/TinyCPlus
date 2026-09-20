# API reference

Source: `std/gui.tc`

Module: `std.gui`

Status: **experimental post-1.0**.

The GUI layer keeps manual ownership and lowers through ordinary C/FFI calls.
Surfaces use packed `0xAARRGGBB` pixels.

## Pixel

`Pixel.rgba(red, green, blue, alpha = 255)` creates a packed color value.

## Surface

An owned in-memory pixel surface.

Core operations:

```c
static (Surface, Error) create(int width, int height);
int width();
int height();
void clear(uint color);
uint getPixel(int x, int y);
void setPixel(int x, int y, uint color);
void fillRect(int x, int y, int width, int height, uint color);
void rect(int x, int y, int width, int height, uint color);
void line(int x0, int y0, int x1, int y1, uint color);
void blit(int x, int y, Surface source, int sourceX = 0, int sourceY = 0,
          int width = -1, int height = -1);
int textWidth(string text, int scale = 1);
void text(int x, int y, string text, uint color, int scale = 1);
u64 checksum();
void destroy();
```

Bitmap text is a dependency-free 5x7 first implementation. Lowercase maps to
uppercase glyphs and unsupported Unicode codepoints render as `?`.

## DoubleBuffer

Owns front/back surfaces. `front()` and `back()` return borrowed surfaces and
must not be destroyed separately.

```c
static (DoubleBuffer, Error) create(int width, int height);
Surface front();
Surface back();
int present();
void destroy();
```

## GuiRect and GuiLayout

`GuiRect` represents integer pixel geometry and provides `contains()` and
`inset()`. `GuiLayout.row()` and `GuiLayout.column()` divide an area without
allocating widget objects.

## GuiDraw

Immediate-mode helpers:

```c
static void label(Surface surface, GuiRect area, string text, uint color,
                  int scale = 1, bool centered = false);
static void button(Surface surface, GuiRect area, string text, bool pressed = false,
                   uint foreground = 15134195, uint background = 2113632,
                   uint pressedBackground = 4210752);
```

## GuiEvent

Event kinds:

```text
1 key
2 text
3 mouse
4 resize
5 close
6 custom
```

Fields include key/codepoint, mouse coordinates/button state, and resize width/height.

## GuiWindow

Owns a double-buffered drawing target and event queue.

```c
static (GuiWindow, Error) create(int width = 800, int height = 600,
                                 string title = "TinyC+", bool headless = false);
int width();
int height();
bool isOpen();
Surface surface();
int present();
Error post(GuiEvent event);
GuiEvent nextEvent(int timeout = 16);
void close();
void destroy();
```

`surface()` returns a borrowed back buffer and must not be destroyed separately.

Headless windows remain portable. Native windows use Win32/GDI on Windows.
macOS has an experimental Cocoa/CoreGraphics backend compiled through the system
Clang toolchain; other unsupported systems still reject non-headless creation.


## GuiTextBox

Owned single-line editor backed by the same UTF-8 gap buffer used by TinyEdit.

```c
static GuiTextBox create(string initial = "");
OwnedString text();
bool dirty();
void markClean();
Error handleEvent(GuiEvent event);
void draw(Surface surface, GuiRect area, bool focused = false,
          uint foreground = 15134195, uint background = 1054752,
          uint border = 8421504);
void destroy();
```

`handleEvent()` consumes normalized key/text events. Navigation currently
covers left/right/home/end/delete/backspace. `text()` transfers ownership of
the returned copy to the caller.
