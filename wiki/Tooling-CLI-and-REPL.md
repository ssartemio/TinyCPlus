# Tooling, CLI y REPL

TinyC+ intenta que el tooling esencial forme parte del proyecto en vez de
depender de una colección de utilidades externas.

## Comandos

```text
tiny check
tiny build
tiny run
tiny test
tiny fmt
tiny doc
tiny repl
```

## Check

```bash
tiny check programa.tc
```

Valida frontend y tipos sin producir ejecutable.

## Build

```bash
tiny build programa.tc -o programa
```

## Run

```bash
tiny run programa.tc
```

Argumentos del programa se colocan después de `--`.

## Inspección

```bash
tiny --emit-tokens programa.tc
tiny --emit-ast programa.tc
tiny --emit-typed-ast programa.tc
tiny --emit-c programa.tc -o programa.c
tiny --emit-asm programa.tc --cc clang -o programa.s
```

La salida C es una herramienta de diseño: permite comprobar cómo se materializa
una abstracción.

## Formatter

```bash
tiny fmt programa.tc
```

Con `-o` puede escribir una copia.

## Documentación

Doc comments:

```c
/// Duplica un valor.
///
/// @param value Valor de entrada.
/// @return Valor multiplicado por dos.
int twice(int value)
{
    return value * 2;
}
```

Generar:

```bash
tiny doc archivo.tc -o build/api.md
```

## Tests

```c
@test
void sum_works()
{
    assert(2 + 3 == 5);
}
```

Ejecutar:

```bash
tiny test archivo.tc
```

## REPL

```bash
tiny repl
```

El REPL usa libtcc y conserva código/variables entre celdas.

Tiene límites deliberados: hasta 128 celdas compiladas y 64 MiB de slots
persistentes.

No es un garbage collector. Destruya recursos explícitamente antes de resetear o
salir.

## Diagnósticos

Los errores del frontend reportan archivo, línea y columna.

Los fallos de bounds/assert usan un código de salida específico. La meta es que
los errores se expresen en términos del fuente TinyC+, no en términos del C
interno cuando pueden diagnosticarse antes.
