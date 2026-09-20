# Soluciones de ejercicios

El curso tiene dos capas de código:

1. los ejemplos acumulativos `01_...` a `12_...`, que construyen TinyStatus;
2. `solutions/`, que resuelve explícitamente los ejercicios de programación de
   cada capítulo.

`tests/test_tutorial.py` ejecuta ambas capas en CI.

## Soluciones por capítulo

| Capítulo | Solución ejecutable | Cobertura |
|---|---|---|
| 1 | `solutions/01_exercises.tc` | i32, bool, WARN y lowering observable |
| 2 | `solutions/02_exercises.tc` | cuarto nivel, clamp, named args, continue |
| 3 | `solutions/03_exercises.tc` | maximum, warnings, slice vacía |
| 4 | `solutions/04_exercises.tc` | tres OwnedString + defer con return |
| 5 | `solutions/05_exercises.tc` | propiedad healthy + Scorable |
| 6 | `solutions/06_exercises.tc` | reduce, first, collect, closure owned |
| 7 | `07_modules/` | módulos, clase, struct C, sizeof y FFI |
| 8 | `solutions/08_exercises.tc` | missing→0, writeAtomic, load e historial de dos muestras |
| 9 | `solutions/09_exercises.tc` | cuatro Tasks, Atomic, producer/channel/close |
| 10 | `solutions/10_exercises.tc` | cuatro bytes, lectura parcial, timeout y cancelación |
| 11 | `solutions/11_exercises.tc` y `exercises/11_multi_client.tc` | protocolo, readExactly, archivo y dos clientes concurrentes |
| 12 | `solutions/12_exercises.tc` | Timer, severidad, TUI headless e histórico |
| 13 | `experimental/13_gui_dashboard.tc` | GUI post-1.0; requiere una rama con std.gui |

## Casos separados de borde

| Capítulo | Archivo | Qué demuestra |
|---|---|---|
| 3 | `exercises/03_bounds_fail.tc` | bounds failure esperado, código 101 |
| 4 | `exercises/04_defer_return.tc` | defer se ejecuta antes de retornar |
| 8 | `exercises/08_corrupt_file.tc` | parse error manejado explícitamente |
| 9 | `exercises/09_channel_closed.tc` | receive después de close retorna Error |
| 11 | `exercises/11_multi_client.tc` | el worker posee y cierra cada socket aceptado |

## Ejercicios de inspección

Algunos ejercicios no deben convertirse en un programa que “pasa” porque su
objetivo es mirar una representación o una herramienta. Para ellos:

```bash
tiny --emit-tokens examples/tutorial/01_first_program.tc
tiny --emit-ast examples/tutorial/02_functions_control.tc
tiny --emit-c examples/tutorial/06_collections_streams.tc -o build/streams.c
tiny --emit-asm examples/tutorial/06_collections_streams.tc --cc clang -o build/streams.s
```

Los ejercicios de sanitizers se cubren además por la matriz CI del proyecto.
