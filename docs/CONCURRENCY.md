# Concurrencia e I/O

`std.concurrent` incluye Thread, Mutex, Condition, Semaphore, Event, Atomic,
WorkerPool, Future<T>, Channel<T>, CancellationToken y TaskGroup<T>.
Los handles se liberan explícitamente. `Thread.join()` espera y libera el
handle; no se debe repetir. `Thread.destroy()` espera si es necesario.
Las operaciones Atomic usan un mutex portable, con orden secuencial; no son
lock-free. Event tiene reset manual. Condition puede despertar espuriamente:
compruebe el predicado bajo el mismo Mutex en un bucle.

```c
import std.concurrent;
int work(int value) { return value * 2; }
int main() {
    var task = spawn work(21);
    defer task.destroy();
    println(task.get());
}
```

`spawn` admite funciones globales y punteros de función. Copia argumentos a un
frame antes de enviarlo al pool. Un argumento puntero/string/slice sigue siendo
prestado: su almacenamiento debe vivir hasta que termine la tarea.
El pool predeterminado tiene cuatro workers; `TINY_WORKERS=1..256` lo configura.
La salida normal espera las tareas pendientes antes de descargar código JIT.

`Task<T>.get()` bloquea y exige éxito (panic si el Future contiene error).
`.result()` retorna `(T,Error)` o sólo Error para Task<void>. `.ready()` consulta
el estado. `.retain()` añade un propietario; cada retención requiere destroy.
`Future<T>.complete(value,error)` completa una sola vez; `.get()` retorna
`(T,Error)`. Liberar un handle no cancela el trabajo ni libera objetos contenidos
en su resultado; consuma/libere resultados propietarios incluso si ya no los necesita.

```c
import std.time;
async int later() { await Timer.after(10); return 42; }
int main() { var task=later(); defer task.destroy(); println(task.get()); }
```

Cada `await` guarda estado y registra continuación; el worker queda disponible.
Al completar la operación, el pool reanuda el frame. Una falla del Future
esperado termina la función async con ese Error, después de los defer activos.
No existe unwinding por exceptions. Await a un Task almacenado no consume su
handle: debe destruirlo el propietario. Los temporales creados directamente en
await se liberan tras obtener el resultado.

Use await dentro del pool para no bloquear sus workers con `.get()`.
Channel.send/receive, Thread.join, TaskGroup.wait y otras APIs síncronas pueden
bloquear. Un programa que bloquea todos sus workers esperando trabajos en ese
mismo pool puede producir deadlock. Las pruebas de async incluyen un solo worker.

Channel<T> es un buffer acotado. `send` espera espacio; `receive` espera un
elemento. `close` despierta operaciones bloqueadas; los elementos pendientes
pueden consumirse y después receive devuelve error. Cierre y termine los usuarios
antes de destroy. El canal copia valores y no destruye recursos de sus elementos.

TaskGroup<T> retiene hijos con `add(task)`, espera su terminación con `wait`,
retorna el primer error en orden de inserción y ofrece un token cooperativo.
`cancel()` establece el token; los hijos deben consultarlo o pasarlo a I/O.
No interrumpe hilos por fuerza. `wait`/`destroy` deben ejecutarse fuera del pool
que ejecuta los hijos. Ésta es la API inicial de concurrencia estructurada.

## Red y archivos

TcpSocket proporciona connect/listen/accept/read/write/close/port y variantes
connectAsync, acceptAsync, readAsync y writeAsync. Las lecturas pueden ser
parciales; cero indica EOF. write intenta escribir todo el buffer. El reactor
usa sockets no bloqueantes y select, con sondeo máximo aproximado de 10 ms.
Admite 32 operaciones pendientes. Un exceso termina con error; no crea hilos
adicionales. Los buffers y tokens son prestados hasta completar la operación.
Termine operaciones antes de cerrar el socket. No envíe lecturas o escrituras
simultáneas sobre el mismo buffer.

Timeouts se expresan en milisegundos; cero significa sin deadline para TCP.
Error 1 indica cancelación; 4 timeout; 13 error de I/O en las rutas normalizadas.
Errores síncronos de sockets pueden conservar códigos del sistema; las APIs
no presentan todos los errno/WSA como códigos gRPC.

UdpSocket proporciona bind, sendTo, receive, receiveAsync y close. receive no
devuelve aún la dirección remitente en el wrapper de alto nivel; la FFI de
runtime sí admite un buffer para dirección y puerto. La recepción trunca
datagramas mayores que el buffer, como recvfrom. sendTo es síncrono.

Dns.resolve y resolveAsync devuelven una dirección numérica. La variante
asíncrona entrega Task<string> con bytes propietarios: envuélvalos en OwnedString.
La resolución del sistema usa dos workers de I/O; no se puede interrumpir una
llamada DNS ya iniciada. ConnectAsync copia el host, pero actualmente no prueba
la siguiente dirección después de un fallo de conexión no bloqueante a la primera.

AsyncFile.readAll devuelve Task<string> con almacenamiento propietario.
AsyncFile.writeAll copia path y contenido antes de programar la escritura, y
devuelve Task<void>. Los workers de archivos/DNS están separados del pool de
continuaciones. File.writeAtomic usa temporal hermano, flush y reemplazo;
los paths Windows utilizan las APIs C/ANSI y no garantizan nombres Unicode
arbitrarios. El contenido del editor sí admite UTF-8.
