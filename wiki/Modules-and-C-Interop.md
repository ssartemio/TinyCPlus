# Módulos y FFI C

Una fortaleza de TinyC+ es poder usar código C sin convertirlo en un ecosistema
separado.

## Módulos

```c
module app.main;

import std.net;
import utilities;
```

Los imports son transitivos. Las funciones pueden calificarse por módulo.

La búsqueda de funciones sin calificador prefiere el módulo actual y rechaza
ambigüedades.

Límite actual importante: clases y enums definidos en módulos distintos todavía
deben tener nombres globalmente únicos.

## Extern C

Ejemplo del repositorio:

```c
extern C {
    struct CPoint {
        int x;
        double y;
    }

    enum CStatus {
        C_OK = 0,
        C_INVALID = 7
    }

    CStatus point_scale(CPoint* point, int scale);
    double point_sum(CPoint point);
    int apply_callback(func<int(int)> callback, int value);
    size_t point_size();
}
```

Uso:

```c
int main()
{
    CPoint point(3, 2.5);

    assert(sizeof(CPoint) == point_size());

    CStatus result = point_scale(&point, 2);
    assert(result == CStatus.C_OK);

    println(point_sum(point));
    println(apply_callback((int value) => value * value, 4));

    return 0;
}
```

Ejecutar con una unidad C adicional:

```bash
tiny run examples/ffi.tc --c-source examples/ffi_math.c
```

## Opciones de link

```bash
tiny build programa.tc \
  --c-source biblioteca.o \
  -I include \
  -L lib \
  -l biblioteca
```

Las opciones se pasan como argumentos, no mediante construcción de un comando
shell.

## ABI

TinyC+ puede declarar structs, enums, punteros, callbacks y funciones C. El
programador debe asegurar que el layout y calling convention coinciden con la
biblioteca.

No existe ABI C++ automática ni importación de headers estilo bindgen.

## Por qué el FFI es central

El proyecto no necesita reescribir SQLite, OpenSSL, nghttp2 o bibliotecas de
sistema para considerarlas “nativas” de TinyC+.

El enfoque preferido es:

```text
API TinyC+ pequeña
      ↓
wrapper FFI claro
      ↓
biblioteca C madura
```

Esto mantiene pequeño al compilador y deja el coste visible.
