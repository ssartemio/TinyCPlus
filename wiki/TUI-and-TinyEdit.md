# TUI y TinyEdit

TinyC+ incluye una interfaz de terminal moderna antes de abordar una GUI gráfica
completa.

## Modelo TUI

`std.tui` trabaja con una ventana, buffers de celdas, widgets, layout y eventos.

Widgets principales:

```text
Label
Button
TextBox
Panel
Row
Column
Stack
```

## Ejemplo

```c
import std.tui;

int main()
{
    var window, error = Window.create();

    if (error != 0) {
        println("A terminal with ANSI support is required.");
        return error;
    }

    defer window.destroy();

    var column = Ui.column();
    column.add(Ui.label("TinyC+ TUI"));

    var panel = Ui.panel("Message");
    var input = Ui.textBox("Hello, TinyC+!");
    panel.add(input);
    column.add(panel);

    var status = Ui.label("Ready");
    var button = Ui.button("Show message");

    button.onClick(() => {
        var text = input.text();
        defer text.destroy();
        status.setText(text.view());
    });

    column.add(button);
    column.add(status);

    window.setContent(column);
    window.focus(input);

    while (true) {
        window.draw();
        window.refresh();

        var event = window.nextEvent();
        if (event.kind == 1 && event.key == 17)
            break;

        window.dispatch(event);
    }

    return 0;
}
```

## Render diferencial

La ventana mantiene frame anterior y actual. `refresh()` emite sólo celdas
cambiadas.

Esto hace que la TUI sea útil tanto en terminal real como en pruebas headless.

## Eventos

La TUI maneja teclas, resize, timer, mouse y eventos custom. Tab mueve foco;
Enter puede activar botones.

La cola admite 64 eventos.

## TinyEdit

`apps/tinyedit.tc` es una aplicación real construida con la TUI.

Ejecutar:

```bash
tiny build apps/tinyedit.tc -o bin/tinyedit
bin/tinyedit notas.txt
```

Atajos:

| Tecla | Acción |
|---|---|
| Ctrl-S | Guardar |
| Ctrl-Q | Salir |
| Ctrl-F | Buscar |
| Ctrl-G | Ir a línea |
| Ctrl-X | Cortar línea |
| Ctrl-U | Pegar |
| Flechas/Home/End | Navegar |
| Backspace/Delete | Borrar |

TinyEdit usa un gap buffer UTF-8, edición multilinea, scroll, guardado atómico,
replay de eventos y snapshots textuales para pruebas.

## Qué no intenta ser

TinyEdit 1.0 no incluye resaltado sintáctico, undo/redo, múltiples documentos ni
clipboard del sistema.

Su valor para el proyecto es mayor que “tener un editor”: prueba en una
aplicación real el sistema de strings, fs, argumentos, TUI, widgets, input,
ownership y testing reproducible.
