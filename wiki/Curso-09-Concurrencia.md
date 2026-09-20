# Capítulo 9 — Concurrencia

**Objetivo:** ejecutar trabajo sin bloquear el flujo principal y aprender la
diferencia entre Thread, Future, Task y Channel.

## Thread

```c
import std.concurrent;

void background(void* context)
{
    println("worker");
}

int main()
{
    var thread = Thread.start(background);
    assert(thread.join() == 0);
    return 0;
}
```

`join()` espera y libera el handle nativo. No lo repita.

## Atomic

```c
var counter = Atomic.create();
defer counter.destroy();

counter.add(1);
println(counter.load());
```

La implementación portable es secuencialmente consistente mediante mutex; no
se promete lock-free.

## Future

```c
var result = Future<int>.create();
defer result.destroy();

result.complete(42);

if (result.ready()) {
    var value, error = result.get();
    assert(error == 0);
    println(value);
}
```

Future es útil cuando un productor y un consumidor necesitan compartir un
resultado explícito.

## Spawn y Task

```c
int calculate(int value)
{
    return value * 2;
}

int main()
{
    var task = spawn calculate(21);
    defer task.destroy();

    println(task.get());
    return 0;
}
```

`Task<T>` representa trabajo lógico. No significa necesariamente “un hilo”.

## Channel<T>

```c
var channel = Channel<int>.create(8);
defer channel.destroy();

assert(channel.send(42) == 0);

var value, error = channel.receive();
assert(error == 0);
println(value);
```

Un channel copia valores y no destruye recursos propietarios contenidos dentro
de esos valores.

## Patrón del proyecto final

El dashboard TUI no debe congelarse mientras hace una conexión TCP. Usaremos:

```text
UI thread
  │
  ├─ sigue procesando eventos
  │
  └─ consulta Future.ready()
             ↑
             │
        network worker
```

Ésta es la separación que construiremos en el capítulo 12.

## Riesgo de deadlock

No bloquee todos los workers esperando tareas que sólo pueden ejecutarse en ese
mismo pool. Dentro de async, prefiera `await` cuando sea posible.

## Ejercicios

1. Lance cuatro tareas de cálculo.
2. Use Atomic para contar finalizaciones.
3. Haga un productor/consumidor con Channel.
4. Cierre el channel y observe receive.
5. Explique qué datos pasados a Thread son prestados y cuánto deben vivir.

[← Capítulo 8](Curso-08-Persistencia-y-Archivos) ·
[Siguiente → Capítulo 10](Curso-10-Async-y-Networking)
