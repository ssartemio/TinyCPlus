# TUI y TinyEdit

`std.tui` ofrece una ventana de terminal, buffers de celdas, widgets y eventos.
Consulte [examples/tui.tc](../examples/tui.tc) para una aplicación con TextBox,
Button, Label y layout Column/Panel. Se construye y ejecuta con:

```powershell
.\bin\tiny.exe build examples\tui.tc -o bin\tui-demo.exe
.\bin\tui-demo.exe
```

Window.create configura la consola para entrada sin buffer y salida ANSI;
devuelve `(Window,Error)`. Use `defer window.destroy();` para restaurarla en la
salida normal. Los panic del runtime también restauran la ventana activa.
Una terminación forzada del proceso o un fallo del host no puede garantizarlo.

`Ui.label`, `button`, `textBox`, `panel`, `row`, `column` y `stack` construyen
widgets. `parent.add(child)` transfiere el hijo al padre. `window.setContent(root)`
transfiere el árbol completo a la ventana. Destruir la ventana libera hijos y
callbacks propios. Las variables Widget son handles, no propietarios duplicados.

Las dimensiones explícitas fijan el tamaño; dimensiones flexibles distribuyen
el espacio restante en Row/Column. Panel agrega borde/título; Stack superpone.
`window.draw()` realiza layout/dibujo y `refresh()` emite únicamente celdas
modificadas con respecto al frame anterior. `put`/`text` permiten dibujo directo.
Colores son enteros RGB de 24 bits. Hay dos buffers de celdas por ventana.

La entrada produce UiEvent: kind 1 tecla, 2 resize, 3 timer, 4 mouse, 5 custom.
`nextEvent(timeout)` espera; `dispatch(event)` maneja edición, foco y botones.
Tab mueve el foco, Enter activa un botón y el mouse puede enfocar/activar.
`key` usa códigos ASCII/control; flechas 1001..1004, Home/End 1005/1006,
Delete 1007, PageUp/PageDown 1008/1009. `codepoint` contiene texto Unicode.
Los eventos personalizados pueden publicarse con post; la cola admite 64.

Un callback prestado de `button.onClick` debe vivir mientras exista el botón.
`owned(lambda)` transfiere al widget la propiedad de su entorno; no vuelva a
destruir la misma closure manualmente. El runtime no hace seguros para hilos
todos los métodos de widgets; gestione el árbol desde su hilo de UI.

Window.create(headless:true) evita modificar la consola. render/snapshot,
post y replay permiten pruebas automatizadas sin una pantalla real. Las pruebas
verifican 10.000 operaciones aleatorias del gap buffer, UTF-8, layout, foco,
callbacks, queue bounds y diferencias de celdas.

## TinyEdit

La aplicación de [apps/tinyedit.tc](../apps/tinyedit.tc) utiliza un gap buffer con
cursor sobre límites de codepoints UTF-8, edición multilínea y scroll.

| Tecla | Acción |
|---|---|
| Ctrl-S | Guardar con reemplazo atómico |
| Ctrl-Q | Salir; pulsar de nuevo para descartar cambios sin guardar |
| Ctrl-F | Buscar texto |
| Ctrl-G | Ir a línea |
| Ctrl-X | Cortar la línea actual |
| Ctrl-U | Pegar el buffer interno |
| Flechas / Home / End | Mover el cursor |
| Backspace / Delete | Borrar un codepoint |

`tinyedit FILE --replay KEYS_FILE --snapshot OUTPUT` ejecuta eventos guardados y
escribe una vista textual. El clipboard es interno; no se integra con el clipboard
del sistema. No incluye resaltado sintáctico, undo/redo, múltiples documentos ni
GUI. La anchura Unicode es aproximada; secuencias de graphemes combinados y shaping
completo están fuera de esta versión. El tamaño máximo del gap buffer es 64 MiB.

El soporte del driver interactivo está implementado para Windows/POSIX. En esta
entrega se comprobó por pruebas headless/replay y una sesión ConPTY Windows:
texto Unicode y multilínea, guardado, salida y restauración de cursor. No se
realizó una validación visual humana en distintos emuladores ni la prueba POSIX.
