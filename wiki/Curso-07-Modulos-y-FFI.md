# Capítulo 7 — Módulos y FFI C

**Objetivo:** dividir TinyStatus en archivos y entender cómo usar una biblioteca C
sin aumentar el núcleo del lenguaje.

## Separar el modelo

`model.tc`:

```c
module tinystatus.model;

class Measurement {
    int temperature;
    int load;

    Measurement(int temperature, int load)
    {
        this.temperature = temperature;
        this.load = load;
    }

    int score()
    {
        return temperature + load;
    }
}
```

`main.tc`:

```c
module tinystatus.main;
import model;

int main()
{
    Measurement sample(24, 37);
    println(sample.score());
    return 0;
}
```

Los imports son transitivos y la resolución de funciones conoce módulos. Los
nombres de clases/enums todavía tienen restricciones globales documentadas.

## FFI C

Imagine una biblioteca C que ofrece una función de checksum:

```c
/* checksum.c */
int status_checksum(int temperature, int load)
{
    return temperature * 31 + load;
}
```

Declare la frontera en TinyC+:

```c
extern C {
    int status_checksum(int temperature, int load);
}

int main()
{
    println(status_checksum(24, 37));
    return 0;
}
```

Compile:

```bash
tiny run main.tc --c-source checksum.c
```

## Structs y callbacks

TinyC+ también puede describir structs/enums C y callbacks. El ejemplo oficial
`examples/ffi.tc` verifica layout con `sizeof` y pasa una lambda como function
pointer.

## Regla de ABI

El FFI no corrige una declaración equivocada.

Si C espera:

```c
struct X { int a; double b; };
```

la declaración TinyC+ debe representar exactamente ese layout compatible.

## Decisión arquitectónica

Antes de reescribir una biblioteca madura pregunte:

```text
¿puedo envolver una API C pequeña?
```

Ese patrón es preferible para SQLite, TLS, codecs y librerías del sistema.

## Ejercicios

1. Separe Measurement y main en módulos.
2. Cree una función C que multiplique un score.
3. Llámela con `extern C`.
4. Añada un struct C y compruebe `sizeof`.
5. Ejecute `examples/ffi.tc --c-source examples/ffi_math.c`.

[← Capítulo 6](Curso-06-Colecciones-Closures-y-Streams) ·
[Siguiente → Capítulo 8](Curso-08-Persistencia-y-Archivos)
