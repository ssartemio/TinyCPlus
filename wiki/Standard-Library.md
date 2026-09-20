# Biblioteca estándar

La stdlib sigue la filosofía de “núcleo pequeño + módulos explícitos”.

## Módulos principales

| Módulo | Propósito |
|---|---|
| std.core | funciones base |
| std.memory | memoria manual |
| std.string | strings, OwnedString y StringBuilder |
| std.collections | Array<T> y Map<K,V> |
| std.io | I/O básico, incluido stderr (`Console.writeError`) |
| std.fs | archivos |
| std.time | tiempo/timers |
| std.math | matemáticas |
| std.process | argumentos, `Process.run` (con shell) y `Process.spawn` (sin shell) |
| std.concurrent | threads/tasks/channels |
| std.net | TCP/UDP/DNS |
| std.protobuf | wire format Protobuf |
| std.grpc | transporte gRPC |
| std.tui | interfaz terminal |

`std.gui` pertenece por ahora al trabajo post-1.0 experimental.

## Criterio de diseño

Una API estándar debería responder claramente:

```text
¿devuelve vista o propietario?
¿puede bloquear?
¿puede suspender?
¿qué error devuelve?
¿qué recurso debe destruir el usuario?
¿qué storage debe permanecer vivo?
```

## Ejemplo: archivo

```c
import std.fs;

int main()
{
    var contents, error = File.readAll("config.txt");

    if (error != 0)
        return error;

    defer contents.destroy();

    println(contents.view());
    return 0;
}
```

## Ejemplo: colección

```c
import std.collections;

var values = Array<int>.create();
defer values.destroy();

values.push(1);
values.push(2);
values.push(3);
```

## Ejemplo: concurrencia

```c
import std.concurrent;

var task = spawn work(21);
defer task.destroy();

println(task.get());
```

El hecho de que distintos módulos compartan convenciones de ownership y Error es
más importante que hacer que cada API sea “mágicamente conveniente”.
