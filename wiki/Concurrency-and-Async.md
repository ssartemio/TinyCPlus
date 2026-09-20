# Concurrencia y async/await

TinyC+ diferencia explícitamente entre hilo del sistema, trabajo lógico y
resultado pendiente.

```text
Thread     = ejecución del OS
Task<T>    = trabajo lógico
Future<T>  = resultado pendiente
```

## Spawn

```c
import std.concurrent;

int work(int value)
{
    return value * 2;
}

int main()
{
    var task = spawn work(21);
    defer task.destroy();

    println(task.get());
    return 0;
}
```

`spawn` copia los argumentos al frame del trabajo. Un argumento puntero,
string o slice sigue siendo prestado: su storage debe continuar vivo.

## Future

Ejemplo productor/consumidor:

```c
import std.concurrent;

async int consume(Future<int> input)
{
    var value = await input;
    return value * 2;
}

void produce(Future<int> output)
{
    output.complete(21);
}

int main()
{
    var input = Future<int>.create();
    defer input.destroy();

    var consumer = consume(input);
    defer consumer.destroy();

    var producer = spawn produce(input);
    defer producer.destroy();

    println(consumer.get());
    producer.get();
    return 0;
}
```

## Async/await

`await` no significa “crear un hilo por llamada”.

Conceptualmente:

```text
función async
   ↓
frame de estado
   ↓
operación pendiente
   ↓
worker queda libre
   ↓
continuación se reanuda cuando el Future completa
```

Esto permite que un número pequeño de workers gestione más tareas lógicas.

## Primitivas disponibles

`std.concurrent` incluye Thread, Mutex, Condition, Semaphore, Event, Atomic,
WorkerPool, Future<T>, Channel<T>, CancellationToken y TaskGroup<T>.

Los handles deben destruirse explícitamente.

## Channels

Un `Channel<T>` es un buffer acotado:

```text
send      espera espacio
receive   espera elemento
close     despierta operaciones bloqueadas
```

El canal copia valores. No destruye automáticamente recursos propietarios dentro
de esos valores.

## Cancelación

La cancelación es cooperativa mediante `CancellationToken`. No se mata un hilo
arbitrariamente como comportamiento normal.

## TaskGroup

Un TaskGroup retiene hijos, espera su terminación y puede compartir un token de
cancelación.

No existe todavía `select`.

## Regla práctica

Dentro del worker pool prefiera `await` en lugar de bloquear con `.get()`
cuando la operación puede suspenderse. Bloquear todos los workers esperando
trabajo que debe ejecutarse en ese mismo pool puede producir deadlock.
