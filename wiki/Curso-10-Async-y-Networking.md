# Capítulo 10 — Async/await y networking

**Objetivo:** construir el primer intercambio TCP asíncrono sin crear un hilo por
cada operación.

## Concepto

```text
async function
   ↓
state frame
   ↓
await operación
   ↓
worker queda libre
   ↓
continuación se reanuda
```

## Ejemplo probado por la suite

```c
import std.net;

async int server(TcpSocket listener)
{
    var socket = await listener.acceptAsync();
    defer socket.close();

    byte[8] buffer = {};

    var n = await socket.readAsync(buffer[:]);

    assert(n == 3);
    assert(buffer[0] == 1);

    return 42;
}

int main()
{
    var listener, error = TcpSocket.listen("127.0.0.1", 0);
    assert(error == 0);
    defer listener.close();

    var serving = server(listener);
    defer serving.destroy();

    var connecting =
        TcpSocket.connectAsync("127.0.0.1", listener.port());
    defer connecting.destroy();

    var client = connecting.get();
    defer client.close();

    byte[3] data = {1, 2, 3};
    assert(client.write(data[:]) == 3);

    println(serving.get());
    return 0;
}
```

Salida:

```text
42
```

## Qué está ocurriendo

El listener usa un puerto efímero (`0`).

`server(listener)` devuelve un Task porque la función es async.

`acceptAsync` suspende el frame mientras espera una conexión.

El cliente se conecta y escribe tres bytes.

El servidor se reanuda y lee.

## read/write

Las lecturas TCP pueden ser parciales. En protocolos reales debe acumular bytes
hasta completar el frame esperado.

`write` intenta enviar todo el buffer; las variantes async aceptan timeout y
CancellationToken.

## DNS

```c
var address, error = Dns.resolve("localhost");
if (error == 0) {
    defer address.destroy();
    println(address.view());
}
```

La versión async devuelve bytes propietarios a través del Task: documente y
libere correctamente el resultado.

## Timer

```c
import std.time;

async int later()
{
    await Timer.after(10);
    return 42;
}
```

## Preparación del protocolo TinyStatus

Usaremos frames mínimos:

```text
request:
  byte 0 = 1        GET_STATUS

response:
  byte 0 = 1        protocolo/version
  byte 1 = score    0..255
  byte 2 = severity 0..2
```

Es deliberadamente simple para que el curso se concentre en ownership,
concurrencia y UI. En software real definiría framing, versionado, endianess,
límites y errores con mayor formalidad.

## Ejercicios

1. Cambie el payload a cuatro bytes.
2. Añada timeout a readAsync.
3. Pruebe cancelación con CancellationToken.
4. Implemente una función que acumule una lectura parcial.
5. Explique por qué `await` no equivale a `Thread.start`.

[← Capítulo 9](Curso-09-Concurrencia) ·
[Siguiente → Capítulo 11](Curso-11-Servidor-TinyStatus)


## Solución ejecutable

[`examples/tutorial/10_async_network.tc`](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/10_async_network.tc)

La CI ejecuta esta solución como parte de `tests/test_tutorial.py`.
