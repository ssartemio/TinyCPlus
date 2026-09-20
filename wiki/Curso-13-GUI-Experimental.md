# Capítulo 13 — Port del dashboard a GUI gráfica

> **Experimental post-1.0.** Este capítulo no describe una API disponible todavía
> en `main`. Use la rama/PR gráfica correspondiente.

**Objetivo:** demostrar que la arquitectura del capítulo 12 no depende de una
terminal. Sustituimos la capa de presentación, no el modelo de red.

## 13.1 Qué permanece igual

```text
probe()
TcpSocket
Task<int>
async/await
protocolo
ownership
String
```

Sólo cambia:

```text
Window / Widget / UiEvent
        ↓
GuiWindow / Surface / GuiEvent
```

Ésta es una prueba importante del diseño en capas.

## 13.2 Preparación

La base gráfica vive en trabajo post-1.0:

- [PR #4 — GUI foundation](https://github.com/ssartemio/TinyCPlus/pull/4)
- [PR #9 — Linux/X11](https://github.com/ssartemio/TinyCPlus/pull/9)
- [PR #10 — widgets portables](https://github.com/ssartemio/TinyCPlus/pull/10)
- [feature/gui-macos](https://github.com/ssartemio/TinyCPlus/tree/feature/gui-macos)

No copie este capítulo y espere que compile contra `main` hasta que la API
gráfica se integre.

## 13.3 Esqueleto de ventana

```c
import std.gui;

int main()
{
    var window, error =
        GuiWindow.create(
            width: 640,
            height: 360,
            title: "TinyStatus"
        );

    if (error != 0)
        return error;

    defer window.destroy();

    while (window.isOpen()) {
        GuiEvent event =
            window.nextEvent(timeout: 16);

        if (event.kind == GuiEventKind.Close)
            break;

        Surface canvas =
            window.surface();

        canvas.clear(
            Pixel.rgba(18, 24, 32)
        );

        GuiDraw.label(
            canvas,
            GuiRect(32, 24, 300, 24),
            "TINYSTATUS",
            Pixel.rgba(230, 237, 243)
        );

        window.present();
    }

    return 0;
}
```

## 13.4 Reusar la tarea de red

La misma `probe()` del capítulo 12 puede permanecer.

```c
var request =
    probe("127.0.0.1", 7777);

defer request.destroy();

bool consumed = false;
```

Dentro del event loop:

```c
if (!consumed && request.ready()) {
    var score, networkError =
        request.result();

    // actualizar el estado visual
    consumed = true;
}
```

El backend de ventana no cambia el modelo de Task.

## 13.5 Botón de refresh

En la rama de widgets:

```c
GuiButton refresh =
    GuiButton("REFRESH");

GuiRect refreshArea =
    GuiRect(32, 120, 120, 30);

if (refresh.handleEvent(
        event,
        refreshArea,
        true)) {

    if (consumed) {
        request.destroy();

        request =
            probe(
                "127.0.0.1",
                7777
            );

        consumed = false;
    }
}
```

Y al dibujar:

```c
refresh.draw(
    canvas,
    refreshArea
);
```

## 13.6 Estado gráfico con OwnedString

Puede mantener un texto propietario:

```c
var status =
    String.copy("Connecting...");

defer status.destroy();
```

Al recibir resultado:

```c
status.destroy();

var number =
    String.fromInt(score);
defer number.destroy();

status =
    String.concat(
        "Server score: ",
        number.view()
    );
```

Después:

```c
GuiDraw.label(
    canvas,
    GuiRect(32, 70, 300, 24),
    status.view(),
    Pixel.rgba(230, 237, 243)
);
```

Aquí la vida del string es explícita: `GuiDraw.label` usa una vista durante el
dibujo; `status` mantiene los bytes entre frames.

## 13.7 Backends

La arquitectura gráfica pretende que el programa de arriba no dependa del
backend:

```text
GuiWindow
  ├─ Win32/GDI
  ├─ Linux/X11
  └─ macOS/Cocoa experimental
```

El renderer común mantiene Surface/DoubleBuffer y normaliza eventos.

## 13.8 Qué aprendimos al portar

El proyecto fue diseñado bien si:

- el protocolo no cambia;
- la función async no cambia;
- ownership de sockets no cambia;
- el modelo de error no cambia;
- sólo se sustituye la presentación/event loop.

Ésa es la ventaja de haber mantenido networking y UI separados.

## 13.9 Proyecto de graduación

Construya una versión completa con:

```text
[ TinyStatus ]
Server: ONLINE
Score: 72
Severity: WARNING
Last refresh: ...
[ REFRESH ] [ AUTO ]
[ ] Save history
Progress/health indicator
```

Use:

- GuiButton;
- GuiCheckbox;
- GuiProgressBar;
- GuiFocus;
- GuiLayout;
- Task<int>;
- Timer/CancellationToken;
- File.writeAtomic para histórico.

## 13.10 Después del curso

Ya conoce el recorrido completo:

```text
syntax
→ memory
→ objects
→ generics
→ collections
→ FFI
→ files
→ concurrency
→ async
→ network
→ TUI
→ GUI
```

El siguiente nivel no consiste en memorizar más sintaxis, sino en leer el
compilador, estudiar el C generado y contribuir una feature manteniendo coste,
ownership y pruebas explícitos.

---

[← Capítulo 12](Curso-12-Dashboard-TUI) ·
[Volver al índice](TinyCPlus-Desde-Cero)


## Solución experimental ejecutable

[`examples/tutorial/experimental/13_gui_dashboard.tc`](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/experimental/13_gui_dashboard.tc)

Se mantiene fuera de la CI estable hasta que `std.gui` llegue a `main`.
