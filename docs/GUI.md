# Graphical GUI foundation

Status: experimental post-1.0 work on `feature/gui-foundation`.

The renderer remains deterministic and in-memory on every supported platform.
`GuiWindow` adds a platform-neutral window/event abstraction. Headless windows
work everywhere; Windows uses Win32/GDI and Linux can opt into the native X11 backend.

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
- Windows native windows use Win32/GDI;
- Linux native windows are available with `--gui-backend x11`;
- Linux headless remains the default, so X11 is not a build dependency for ordinary TinyC+;
- macOS still uses headless windows;
- Win32 and X11 present the same `0xAARRGGBB` front buffer.

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
3. add a macOS native backend and evaluate a Wayland backend without changing Surface/Canvas semantics;
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


## Linux X11 backend

The X11 backend is deliberately opt-in:

```sh
tiny run examples/gui_window.tc --gui-backend x11
tiny run examples/gui_window.tc --cc gcc --gui-backend x11
```

Compilation enables `TC_GUI_X11_BACKEND` and links `libX11` only for that
invocation. Headless GUI programs continue to compile without X11 headers or
libraries.

The backend currently provides:

- native window creation and close protocol handling;
- the same double-buffered `Surface` used by Win32/headless;
- partial damaged-region presentation through `XPutImage`;
- resize events with buffer recreation;
- special-key normalization through `std.input`;
- key/text, pointer motion and three-button mouse events;
- timeout-based event waiting through the X connection file descriptor.

CI installs `libx11-dev` only in the dedicated X11 job and runs the native
tests under Xvfb. Both the external GCC backend and libtcc execute a TinyC+
native X11 program there.

Current limits:

- X11 uses the default TrueColor visual with standard RGB masks;
- text input uses XIM/XIC with `Xutf8LookupString` when available and falls back to `XLookupString`; advanced IME preedit UI is still future work;
- Wayland is not implemented yet;
- X11 is not auto-selected from `DISPLAY`; explicit selection keeps builds
  deterministic and avoids making desktop development packages mandatory.
