# Graphical GUI foundation

Status: experimental post-1.0 work on `feature/gui-foundation`.

The renderer remains deterministic and in-memory on every supported platform.
`GuiWindow` adds a platform-neutral window/event abstraction. Headless windows
work everywhere; Windows additionally has the first native backend using Win32/GDI.

## Contract

- pixel format: `0xAARRGGBB`;
- integer pixel coordinates;
- drawing clips to the target surface;
- surfaces own memory and require explicit `destroy()`;
- borrowed surfaces returned by `DoubleBuffer.front()` / `back()` must not be destroyed;
- no implicit allocation while drawing;
- no alpha compositing yet: pixels are copied/replaced;
- built-in dependency-free 5x7 bitmap text is available for ASCII-oriented UI;
- lowercase letters map to uppercase glyphs in this first font;
- unsupported Unicode codepoints currently render as `?`;
- native windows use Win32/GDI on Windows; macOS has an experimental Cocoa backend; Linux remains headless in this branch;
- Win32 presentation uses GDI and the same 0xAARRGGBB front buffer.

## Core API

`Surface` provides pixel access, clear, filled/outlined rectangles, Bresenham
lines, blit, damage bounds and a deterministic checksum.

`DoubleBuffer` owns front/back surfaces. Drawing happens on the back surface;
`present()` copies only the accumulated damage rectangle and returns the number
of pixels whose values actually changed.

Rendering is isolated from platform APIs. `GuiWindow` owns a double buffer and
exposes key/text/mouse/resize/close/custom events. CI exercises the complete
headless event path on every platform while compiling the Win32 implementation
on Windows.

## Next steps

1. validate the Win32 native path interactively in addition to CI compilation;
2. reuse Row/Column layout rules for graphical Label/Button/TextBox;
3. validate Cocoa interactively and add the Linux native backend without changing Surface/Canvas semantics;
4. add richer font backends later without changing the basic Surface contract;
5. keep GUI/TUI event payloads interoperable while preserving their existing kind values.

## Shared input keys

GUI and TUI use one runtime ABI for special keys. The public `std.input`
module exposes `InputKey.Left`, `Right`, `Up`, `Down`, `Home`, `End`,
`Delete`, `PageUp` and `PageDown`. Their established numeric values
(`1001..1009`) remain unchanged.

Event `kind` values remain subsystem-specific: GUI has distinct text and close
events while TUI has timer events. Keeping those values separate avoids a
breaking change while still sharing the useful key identity contract.


## Primitive widgets

The first graphical widgets are intentionally immediate-mode:

- `GuiRect` and `GuiLayout.row/column` provide allocation-free geometry;
- `GuiDraw.label` and `GuiDraw.button` render directly to a `Surface`;
- `GuiTextBox` is the first stateful control and reuses the existing UTF-8
  `GapBuffer` from TinyEdit.

`GuiTextBox` owns its editing buffer, returns text as an explicit
`OwnedString`, accepts normalized `GuiEvent` key/text input and draws a
single-line field with caret. Long content uses a small horizontal viewport so
the caret remains visible. This is deliberately not a second text engine.


### Stateful button and focus

`GuiButton` wraps the immediate button renderer with press/release state and
keyboard activation through Enter/Space when focused. Its label is a borrowed
`string`, so the caller must keep non-literal backing storage alive.

`GuiFocus` is intentionally small: it stores a focus index, supports
`next()/previous()/set()`, and consumes Tab to advance. Mouse hit-testing
remains explicit, which keeps layout and ownership visible instead of introducing
a hidden widget tree.


## macOS/Cocoa backend

The experimental macOS backend is implemented from C11 through the Objective-C
runtime plus CoreGraphics; the TinyC+ frontend itself does not become
Objective-C. `Surface`, widgets and `GuiEvent` remain unchanged.

Native Cocoa GUI builds temporarily use the external system C compiler
(Clang) instead of libtcc because the current libtcc integration does not model
Apple framework linking. Non-GUI programs and headless GUI tests retain the
existing TinyCC path.

CI compiles and links the Cocoa backend and executes its headless path. A real
WindowServer smoke remains an interactive qualification item.


### Interactive Cocoa smoke

CI compiles/links the Cocoa backend and executes its headless path. On a macOS
desktop with an active WindowServer, the native smoke can be run explicitly:

```bash
clang -std=c11 -D_POSIX_C_SOURCE=200809L -DTC_GUI_COCOA tests/runtime_gui.c \
  -lm -framework Cocoa -framework CoreGraphics -lobjc -o build/gui-cocoa
TC_GUI_NATIVE_SMOKE=1 ./build/gui-cocoa
```

This creates a real `NSWindow`, paints the TinyC+ software surface, pumps the
Cocoa event loop briefly, then verifies close and cleanup.
