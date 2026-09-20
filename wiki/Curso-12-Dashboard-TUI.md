# Capítulo 12 — Proyecto final estable: dashboard TUI concurrente

**Objetivo:** unir lenguaje, ownership, async, TCP y TUI en una aplicación que
**no bloquee la interfaz mientras espera la red**.

Este es el capstone de TinyC+ 1.0.

## 12.1 Arquitectura

Ejecute el servidor del capítulo 11 en una terminal y el dashboard en otra.

```text
┌─────────────────────────────┐
│ TUI event loop              │
│                             │
│ draw / refresh              │
│ keyboard input              │
│ status label                │
│        │                    │
│        └── polls Task.ready │
└─────────────────────────────┘
               │
               │ Task<int>
               ▼
┌─────────────────────────────┐
│ async probe()               │
│                             │
│ connectAsync                │
│ writeAsync                  │
│ readAsync                   │
└─────────────┬───────────────┘
              │ TCP
              ▼
       TinyStatus server
```

La UI jamás llama a un read bloqueante.

## 12.2 Función de red

```c
import std.net;

async int probe(string host, int port)
{
    var socket =
        await TcpSocket.connectAsync(
            host,
            port,
            timeout: 2000
        );

    defer socket.close();

    byte[1] request = {1};

    var sent =
        await socket.writeAsync(
            request[:],
            timeout: 2000
        );

    if (sent != 1)
        return -1;

    byte[3] response = {};

    var received =
        await socket.readAsync(
            response[:],
            timeout: 2000
        );

    if (received != 3)
        return -1;

    if (response[0] != 1)
        return -1;

    return response[1];
}
```

Un fallo real de la operación async queda en el Error del Task. Un error de
nuestro protocolo simplificado devuelve `-1`.

## 12.3 Dashboard completo

```c
import std.net;
import std.string;
import std.tui;

async int probe(string host, int port)
{
    var socket =
        await TcpSocket.connectAsync(
            host,
            port,
            timeout: 2000
        );

    defer socket.close();

    byte[1] request = {1};
    byte[3] response = {};

    var sent =
        await socket.writeAsync(
            request[:],
            timeout: 2000
        );

    if (sent != 1)
        return -1;

    var received =
        await socket.readAsync(
            response[:],
            timeout: 2000
        );

    if (received != 3 || response[0] != 1)
        return -1;

    return response[1];
}

int main()
{
    var window, error = Window.create();

    if (error != 0) {
        println("ANSI terminal required.");
        return error;
    }

    defer window.destroy();

    var root = Ui.column();

    root.add(
        Ui.label(
            "TinyStatus | R: refresh | Ctrl-Q: quit"
        )
    );

    var panel = Ui.panel("Server");
    var status = Ui.label("Connecting...");
    panel.add(status);

    root.add(panel);

    window.setContent(root);

    // Arrancamos la primera consulta sin bloquear.
    var request =
        probe("127.0.0.1", 7777);

    // defer observa el valor actual al salir.
    defer request.destroy();

    bool consumed = false;

    while (true) {
        // Consumimos el resultado cuando esté disponible.
        if (!consumed && request.ready()) {
            var score, networkError =
                request.result();

            if (networkError != 0 || score < 0) {
                status.setText("Network error");
            }
            else {
                var number =
                    String.fromInt(score);
                defer number.destroy();

                var message =
                    String.concat(
                        "Server score: ",
                        number.view()
                    );
                defer message.destroy();

                // Widget copia el texto; el OwnedString
                // temporal puede destruirse al salir.
                status.setText(message.view());
            }

            consumed = true;
        }

        window.draw();
        window.refresh();

        // Timeout corto: la UI continúa viva aunque
        // no haya entrada de teclado.
        var event =
            window.nextEvent(timeout: 50);

        // Ctrl-Q
        if (event.kind == 1 &&
            event.key == 17)
            break;

        // R / r vuelve a consultar.
        if (event.kind == 1 &&
            (event.key == 82 ||
             event.key == 114) &&
            consumed) {

            request.destroy();

            request =
                probe(
                    "127.0.0.1",
                    7777
                );

            consumed = false;
            status.setText("Connecting...");
            continue;
        }

        window.dispatch(event);
    }

    return 0;
}
```

## 12.4 Lo importante no es el aspecto visual

Este programa prueba varias capas simultáneamente:

```text
std.tui
std.string
std.net
Task<int>
async/await
socket lifecycle
OwnedString
event loop
manual cleanup
```

Ese es el valor del proyecto final.

## 12.5 Por qué la interfaz no se congela

`probe()` retorna inmediatamente un Task.

Cuando llega a:

```c
await socket.readAsync(...)
```

su frame queda suspendido.

El loop de TUI sigue haciendo:

```text
ready?
draw
refresh
nextEvent(timeout)
dispatch
```

No hay un thread nuevo por cada await.

## 12.6 Ownership final

```text
Window             owned → destroy
widget tree        transferido a Window
Task<int> request  owned → destroy
socket             owned dentro probe → close
byte arrays        stack
slices             borrowed
OwnedString temp   owned → destroy
status Widget      handle gestionado por Window
```

Poder describir esta tabla es una parte de “haber terminado” el curso.

## 12.7 Manejo de error de Task

`Task<T>.get()` exige éxito. Para una UI preferimos `.result()`, porque el
error de red debe convertirse en estado visual, no en panic.

## 12.8 Mejoras propuestas

Convierta el dashboard en una aplicación real:

1. muestre severity;
2. pinte distintos estados con colores TUI;
3. añada timestamp;
4. refresque automáticamente con Timer;
5. añada CancellationToken;
6. lea host/port desde Arguments;
7. persista el último score;
8. muestre histórico usando Array;
9. use Channel para telemetría continua;
10. cambie el protocolo manual por Protobuf/gRPC.

## 12.9 Versión gRPC

Después de dominar el protocolo pequeño, el paso lógico es generar un servicio
Protobuf:

```protobuf
service StatusService {
    rpc GetStatus(StatusRequest)
        returns (StatusResponse);
}
```

El curso empieza con bytes manuales para enseñar framing y ownership; gRPC
permite luego sustituir esa capa conservando el modelo async/UI.

## Fin de la ruta estable

Los capítulos 1–12 usan capacidades de TinyC+ 1.0 RC.

El siguiente capítulo enseña cómo portar esta idea a la GUI gráfica que se está
desarrollando post-1.0.

---

[← Capítulo 11](Curso-11-Servidor-TinyStatus) ·
[Siguiente → Capítulo 13: GUI experimental](Curso-13-GUI-Experimental)
