# Capítulo 11 — Servidor TinyStatus

**Objetivo:** convertir el protocolo del capítulo anterior en un servidor
independiente, pequeño y comprensible.

El servidor escuchará en localhost y responderá a una petición
`GET_STATUS = 1`.

## 11.1 Protocolo

```text
request (1 byte)
┌───────────────┐
│ command = 1   │
└───────────────┘

response (3 bytes)
┌─────────┬─────────┬──────────┐
│ version │ score   │ severity │
│    1    │  0..255 │   0..2   │
└─────────┴─────────┴──────────┘
```

Mantenerlo pequeño hace visible el problema de framing.

## 11.2 Servidor completo

```c
import std.net;

int main()
{
    var listener, error =
        TcpSocket.listen("127.0.0.1", 7777);

    if (error != 0) {
        println("Unable to listen.");
        return error;
    }

    defer listener.close();

    println("TinyStatus server on 127.0.0.1:7777");

    // El tutorial atiende 100 conexiones y termina.
    for (int requestNumber = 0;
         requestNumber < 100;
         requestNumber = requestNumber + 1) {

        var client, acceptError = listener.accept();

        if (acceptError != 0)
            continue;

        byte[1] request = {};
        var received = client.read(request[:]);

        if (received == 1 && request[0] == 1) {
            // En una aplicación real estos valores vendrían
            // del modelo/persistencia construido antes.
            byte[3] response = {1, 72, 1};

            var written = client.write(response[:]);

            if (written != 3)
                println("Partial response.");
        }

        client.close();
    }

    return 0;
}
```

Ejecute:

```bash
tiny run tinystatus_server.tc
```

## 11.3 Primer cliente de prueba

En otra terminal:

```c
import std.net;

int main()
{
    var socket, error =
        TcpSocket.connect("127.0.0.1", 7777);

    if (error != 0)
        return error;

    defer socket.close();

    byte[1] request = {1};
    byte[3] response = {};

    if (socket.write(request[:]) != 1)
        return 13;

    var received = socket.read(response[:]);

    if (received != 3)
        return 13;

    println(response[0]);
    println(response[1]);
    println(response[2]);

    return 0;
}
```

Esperamos aproximadamente:

```text
1
72
1
```

## 11.4 TCP no conserva “mensajes”

Un error pedagógico frecuente es pensar:

> hice write(3), por tanto read devolverá 3.

TCP entrega un stream. Una lectura puede ser parcial.

Para un protocolo robusto escribiríamos una función del estilo:

```text
readExactly(socket, buffer, expected)
```

que acumule bytes hasta completar el frame, EOF, timeout o error.

El ejemplo simplificado funciona como laboratorio, no como definición de un
protocolo de producción.

## 11.5 ¿Dónde está la concurrencia?

Por ahora el servidor atiende conexiones secuencialmente. Es intencional:
primero aislamos transporte y framing.

La aplicación final sí será concurrente: la interfaz continuará procesando
eventos mientras una tarea async espera la red.

Una extensión del servidor podría entregar cada cliente a `spawn` o a un
TaskGroup, pero entonces debe definir con precisión quién es propietario de cada
socket.

## 11.6 Errores y cleanup

Observe la política:

```text
listener       propietario → close al terminar
client socket  propietario → close por conexión
request[]      stack
response[]     stack
slices         vistas sobre esos arrays
```

No hay reserva escondida para los frames.

## 11.7 Ejercicios

1. Cambie el score según un contador.
2. Rechace commands distintos de 1.
3. Implemente `readExactly`.
4. Añada un byte de error al protocolo.
5. Haga que el servidor lea el score desde `status.txt`.
6. Como ejercicio avanzado, atienda clientes con `spawn` y documente ownership.

---

[← Capítulo 10](Curso-10-Async-y-Networking) ·
[Siguiente → Capítulo 12](Curso-12-Dashboard-TUI)


## Solución ejecutable

[`examples/tutorial/11_tinystatus_server.tc`](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/11_tinystatus_server.tc)

La [solución de los ejercicios](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/solutions/11_exercises.tc)
incluye `readExactly`, el byte de error y la carga del score desde un archivo.
El [ejemplo avanzado](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/exercises/11_multi_client.tc)
atiende a dos clientes con `spawn`. Cada worker cierra el socket que recibe y
`main` espera las tareas antes de cerrar el listener.

La CI ejecuta esta solución como parte de `tests/test_tutorial.py`.
