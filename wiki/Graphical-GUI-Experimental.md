# GUI gráfica — trabajo post-1.0

> **Estado: experimental. No forma parte del contrato TinyC+ 1.0.0-rc.1 de main.**

La GUI gráfica se desarrolla después de estabilizar lenguaje, concurrencia,
networking y TUI.

## Arquitectura

```text
Aplicación TinyC+
       ↓
     std.gui
       ↓
 ┌───────────────┐
 │ Surface       │
 │ DoubleBuffer  │
 │ GuiEvent      │
 │ widgets       │
 └───────┬───────┘
         ↓
 backend de ventana
   ├─ Win32/GDI
   ├─ Linux/X11
   └─ macOS/Cocoa experimental
```

El contrato central usa software rendering y pixels `0xAARRGGBB`. Las
operaciones de dibujo incluyen clear, pixel, rect, fillRect, line, blit y texto
bitmap.

## Ejemplo headless

```c
import std.gui;

int main()
{
    var surface, error = Surface.create(width: 64, height: 40);
    if (error != 0)
        return error;

    defer surface.destroy();

    var background = Pixel.rgba(16, 24, 32);
    var accent = Pixel.rgba(90, 180, 255);

    surface.clear(background);
    surface.fillRect(8, 8, 48, 24, accent);
    surface.line(0, 0, 63, 39, Pixel.rgba(255, 90, 90));

    println(surface.checksum());
    return 0;
}
```

## Ejemplo de ventana/formulario

```c
import std.gui;

int main()
{
    var window, error =
        GuiWindow.create(width: 640, height: 360, title: "TinyC+ form");

    if (error != 0)
        return error;

    defer window.destroy();

    GuiTextBox input = GuiTextBox.create("type here");
    defer input.destroy();

    GuiButton closeButton = GuiButton("CLOSE");
    GuiFocus focus = GuiFocus(2);

    while (window.isOpen()) {
        GuiEvent event = window.nextEvent(timeout: 16);

        if (event.kind == GuiEventKind.Close)
            break;

        Surface canvas = window.surface();
        canvas.clear(Pixel.rgba(18, 24, 32));

        GuiDraw.label(
            canvas,
            GuiRect(40, 32, 360, 24),
            "TINYC+ FORM",
            Pixel.rgba(230, 237, 243)
        );

        window.present();
    }

    return 0;
}
```

## Desarrollo por PR

**PR #4 — GUI foundation**  
Surface, DoubleBuffer, bitmap text, GuiWindow, Win32/GDI, eventos, TextBox,
Button, Focus y pruebas.

**PR #9 — Linux/X11**  
Backend Xlib con `XPutImage`, eventos normalizados y smoke real bajo Xvfb en
x86-64/ARM64.

**PR #10 — widgets portables**  
Checkbox, ProgressBar y helpers adicionales de layout, implementados en TinyC+
sin runtime C extra.

**feature/gui-macos**  
Backend Cocoa/CoreGraphics experimental desde C11 mediante runtime Objective-C.
La compilación y enlace se validan con Clang; la validación WindowServer real es
un paso separado.

## Principios que no cambian

La GUI no debe introducir GC, widget tree oculto obligatorio ni ownership
implícito.

`Surface` sigue siendo una abstracción portable y los backends nativos deben
vivir debajo de ella.

El objetivo no es copiar Qt, Cocoa o WinUI completos, sino construir una capa
pequeña, verificable y coherente con el resto de TinyC+.
