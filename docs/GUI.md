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
- no font/text rasterization yet;
- native windows currently exist only on Win32; Linux/macOS use headless windows;
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
2. add bitmap-font text;
3. reuse Row/Column layout rules for graphical Label/Button/TextBox;
4. add macOS and Linux native backends without changing Surface/Canvas semantics;
5. converge GUI/TUI event constants where that improves reuse.
