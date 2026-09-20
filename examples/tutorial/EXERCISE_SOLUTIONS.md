# Soluciones de ejercicios

Los archivos de nivel superior `01_...` a `12_...` son las soluciones
acumulativas principales del curso. La mayoría de los ejercicios de cada capítulo
están integrados directamente en ellos mediante `assert` y comportamiento
observable.

Este subdirectorio contiene casos que conviene mantener separados porque su
objetivo es provocar un error o verificar una ruta de borde:

| Capítulo | Archivo | Qué demuestra |
|---|---|---|
| 3 | `exercises/03_bounds_fail.tc` | bounds failure esperado, código 101 |
| 4 | `exercises/04_defer_return.tc` | defer se ejecuta antes de retornar |
| 8 | `exercises/08_corrupt_file.tc` | parse error manejado explícitamente |
| 9 | `exercises/09_channel_closed.tc` | receive después de close retorna Error |

`tests/test_tutorial.py` verifica tanto las rutas exitosas como el fallo de
bounds esperado.

## Cobertura por capítulo

- 01: tipos explícitos/inferidos, condición y lowering.
- 02: defaults, named args, clamp, clasificación, continue.
- 03: promedio, máximo, warnings, slice vacía y bounds esperado.
- 04: new/delete, OwnedString, concat, cleanup y retorno temprano.
- 05: clase, propiedad e interface estructural.
- 06: Array, filter/count, first, reduce, collect y closure owned.
- 07: módulo sibling, clase importada y C FFI.
- 08: writeAtomic, readAll, parseInt y archivo corrupto.
- 09: Thread, Atomic, Future, Channel, close y spawn.
- 10: accept/connect/read async sobre loopback.
- 11: protocolo TinyStatus y servidor de una conexión.
- 12: Task.ready/result + TCP + TUI headless/snapshot.
- 13: dashboard gráfico en `experimental/`, fuera de la suite estable.
