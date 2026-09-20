# Capítulo 8 — Persistencia y archivos

**Objetivo:** guardar el estado de TinyStatus y practicar ownership con I/O real.

## Leer todo un archivo

```c
import std.fs;

int main()
{
    var contents, error = File.readAll("status.txt");

    if (error != 0)
        return error;

    defer contents.destroy();

    println(contents.view());
    return 0;
}
```

`File.readAll` devuelve `OwnedString`. El archivo ya puede cerrarse; los bytes
pertenecen al resultado hasta `destroy()`.

## Escribir

```c
var error = File.writeAll("status.txt", "42");
if (error != 0)
    return error;
```

Para reemplazo más seguro:

```c
File.writeAtomic("status.txt", "42");
```

`writeAtomic` usa un temporal hermano, flush y reemplazo.

## Guardar un entero

```c
import std.fs;
import std.string;

Error saveScore(int score)
{
    var text = String.fromInt(score);
    defer text.destroy();

    return File.writeAtomic("status.txt", text.view());
}
```

## Cargar

```c
(int, Error) loadScore()
{
    var contents, error = File.readAll("status.txt");

    if (error != 0)
        return (0, error);

    defer contents.destroy();

    var value, parseError = String.parseInt(contents.view());
    if (parseError != 0)
        return (0, parseError);

    return (cast<int>(value), 0);
}
```

## Archivos con handle

Para streaming:

```c
var file, error = File.open("data.bin", "rb");
if (error != 0)
    return error;

defer file.close();
```

Después use slices de bytes para `read`/`write`.

## Qué aprendemos de ownership

```text
File handle       propietario → close()
OwnedString       propietario → destroy()
string view       prestado
Slice<byte>       prestada sobre un buffer
```

## Ejercicios

1. Guarde el último score de TinyStatus.
2. Si el archivo no existe, use cero.
3. Corrompa el archivo y maneje parseError.
4. Cambie writeAll por writeAtomic.
5. Añada un archivo de historial con una línea por muestra.

[← Capítulo 7](Curso-07-Modulos-y-FFI) ·
[Siguiente → Capítulo 9](Curso-09-Concurrencia)
