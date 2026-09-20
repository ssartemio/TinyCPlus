# TinyC+ desde cero — ejemplos ejecutables

Este directorio acompaña el curso de la Wiki
[TinyC+ desde cero](../../wiki/TinyCPlus-Desde-Cero.md).

Los ejemplos 01–12 corresponden a capacidades estables de TinyC+ 1.0 RC y se
verifican automáticamente con `tests/test_tutorial.py`.

El capítulo 13 vive en `experimental/` porque requiere la GUI post-1.0 y no se
incluye en la suite estable.

Ejecutar todo:

Primero compile el frontend con `python build.py`. La suite crea `build/` si falta.

```bash
python tests/test_tutorial.py
```

Ejecutar un ejemplo:

```bash
tiny run examples/tutorial/06_collections_streams.tc
```

El capítulo 7 es multiarchivo y añade una unidad C:

```bash
tiny run examples/tutorial/07_modules/main.tc \
  --c-source examples/tutorial/07_modules/checksum.c
```


## Soluciones por ejercicio

Después de intentar los ejercicios de cada capítulo:

```text
solutions/             soluciones explícitas por capítulo
exercises/             fallos esperados y casos de borde
07_modules/            solución multiarchivo/FFI
experimental/          capítulo 13, requiere API GUI ausente en main
EXERCISE_SOLUTIONS.md  mapa de cobertura
```

La suite ejecuta tanto el programa acumulativo como la solución de ejercicios:

```bash
python tests/test_tutorial.py
python tests/test_tutorial.py --cc gcc
```
