# Graphical GUI foundation

Status: experimental post-1.0 work on `feature/gui-foundation`.

This layer intentionally starts below native windows. It provides deterministic
in-memory rendering that is identical on every supported platform. Native window
backends will consume the same buffers later.

## Contract

- pixel format: `0xAARRGGBB`;
- integer pixel coordinates;
- drawing clips to the target surface;
- surfaces own memory and require explicit `destroy()`;
- borrowed surfaces returned by `DoubleBuffer.front()` / `back()` must not be destroyed;
- no implicit allocation while drawing;
- no alpha compositing yet: pixels are copied/replaced;
- no font/text rasterization yet.

## Core API

`Surface` provides pixel access, clear, filled/outlined rectangles, Bresenham
lines, blit, damage bounds and a deterministic checksum.

`DoubleBuffer` owns front/back surfaces. Drawing happens on the back surface;
`present()` copies only the accumulated damage rectangle and returns the number
of pixels whose values actually changed.

This isolates rendering from platform window APIs and gives CI a headless test
surface before Win32/Cocoa/X11/Wayland backends are introduced.

## Next steps

1. introduce a platform-neutral window/event backend interface;
2. implement a Win32 reference backend;
3. bridge keyboard/mouse/resize events to the existing TinyUI event concepts;
4. add bitmap-font text;
5. reuse Row/Column layout rules for graphical Label/Button/TextBox;
6. add platform backends incrementally without changing Surface/Canvas semantics.
