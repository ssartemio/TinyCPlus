# Primeros pasos

Esta página lleva desde un checkout limpio hasta ejecutar y compilar programas
TinyC+.

## Windows x64

El repositorio distribuye `tiny.exe` y TinyCC junto con el árbol del proyecto.

```powershell
.\bin\tiny.exe --version
.\bin\tiny.exe run examples\hello.tc
.\bin\tiny.exe run examples\core.tc
.\bin\tiny.exe run examples\collections.tc
.\bin\tiny.exe run examples\streams.tc
.\bin\tiny.exe run examples\async.tc
```

Para compilar un ejecutable:

```powershell
.\bin\tiny.exe build examples\core.tc -o bin\core.exe
.\bin\core.exe
```

No copie únicamente `tiny.exe`. El compilador espera encontrar `runtime`,
`std` y `third_party`. Para instalaciones personalizadas use `TINY_HOME` o
`--home`.

## Linux y macOS

Reconstruya primero las dependencias empaquetadas y el frontend:

```bash
python3 tools/bootstrap.py --cc cc
python3 build.py --cc cc
./bin/tiny run examples/hello.tc
```

El bootstrap es offline: usa las fuentes incluidas en `third_party`.

## Compilar el compilador

```bash
python3 build.py
python3 build.py --cc clang
python3 tools/bootstrap.py --verify-only
```

El frontend está escrito en C11. Python sólo se usa en scripts de build,
generación Protobuf y pruebas.

## Flujo recomendado para aprender

Cree `hello.tc`:

```c
int main()
{
    println("Hola desde TinyC+");
    return 0;
}
```

Valide sin enlazar:

```bash
tiny check hello.tc
```

Inspeccione el lowering:

```bash
tiny --emit-c hello.tc -o hello.c
```

Compile:

```bash
tiny build hello.tc -o hello
```

O compile y ejecute:

```bash
tiny run hello.tc
```

## Inspeccionar el compilador

TinyC+ está diseñado para que el programador pueda mirar detrás de la
abstracción:

```bash
tiny --emit-tokens examples/core.tc
tiny --emit-ast examples/core.tc
tiny --emit-c examples/core.tc -o build/core.c
tiny --emit-asm examples/core.tc --cc clang -o build/core.s
```

`--emit-asm` usa GCC/Clang porque libtcc genera código máquina directamente.

## Primer programa con recursos

```c
import std.collections;

int main()
{
    var values = Array<int>.create();
    defer values.destroy();

    values.push(10);
    values.push(20);
    values.push(30);

    for (i, value in values)
        println(value);

    return 0;
}
```

Aquí ya aparecen tres ideas esenciales:

- el contenedor es propietario de memoria;
- el programador programa explícitamente su destrucción;
- `defer` garantiza la limpieza en las salidas normales del bloque.

Antes de escribir aplicaciones grandes, lea **[Memoria y recursos](Memory-and-Resources)**.
