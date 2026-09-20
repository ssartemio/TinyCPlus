# TinyC+ desde cero — ejemplos ejecutables

Este directorio acompaña el curso de la Wiki
[TinyC+ desde cero](../../wiki/TinyCPlus-Desde-Cero.md).

Los ejemplos 01–12 corresponden a capacidades estables de TinyC+ 1.0 RC y se
verifican automáticamente con `tests/test_tutorial.py`.

El capítulo 13 vive en `experimental/` porque requiere la GUI post-1.0 y no se
incluye en la suite estable.

Ejecutar todo:

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
