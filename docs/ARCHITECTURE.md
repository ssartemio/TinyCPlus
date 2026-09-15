# Arquitectura y costes

```text
.tc -> lexer -> parser/AST -> módulos -> monomorfización -> análisis de tipos
    -> lowering C11 -> libtcc (memoria / ejecutable)
                   -> GCC/Clang (ejecutable / assembler)
```

`compiler/main.c` implementa la CLI y el ciclo de vida de cada unidad;
`lexer.c` produce tokens con ubicación; `parser.c` construye AST;
`modules.c` carga y califica símbolos; `generic.c` instancia tipos/funciones;
`semantic.c` valida y resuelve; `cgen.c` convierte operaciones a C;
`backend.c` carga dinámicamente libtcc o ejecuta un compilador externo;
`tooling.c` implementa fmt/doc/REPL. `types.c` interna tipos;
`util.c` proporciona arena, buffers y diagnósticos.

La arena libera objetos de AST/tipos por unidad. No hay IR optimizador propio.
`--emit-c` permite inspeccionar estructuras, frames, adaptadores de interfaces,
funciones de closure y bucles. Los `#line` remiten al archivo `.tc` original.
Los diagnósticos abortan la unidad actual mediante setjmp/longjmp, sin impedir
compilar las siguientes unidades del modo batch. La comprobación sintáctica no
sustituye a compilar y enlazar: ciertos límites del backend aparecen entonces.

| Construcción | Representación | Reserva de memoria / coste |
|---|---|---|
| Primitiva/puntero | Tipo C del host | Stack/registro |
| Clase por valor | struct C | Tamaño de sus campos |
| Array fijo | struct con `data[N]` | Contiguo, por valor |
| Slice/string | puntero + longitud | Sin reserva; almacenamiento prestado |
| Tuple | struct con v0, v1… | Por valor |
| Interfaz | puntero + vtable | Sin reserva; llamada indirecta |
| Función/lambda sin capturas | puntero C | Sin entorno |
| Closure prestada | entorno local + invoke + marca | Copia de capturas; sin heap |
| `owned` closure | entorno reservado + invoke | Una reserva; destroy explícito |
| `Array<T>` | data, length, capacity | Realloc al crecer |
| Stream filter/map/reduce | Bucle fusionado | Sin colecciones intermedias |
| `spawn` | frame + Future + trabajo | Reserva por tarea; pool reutilizado |
| Función async | frame de estados + Future | Reserva por invocación; sin hilo propio |
| Timer/TCP async | petición del reactor | Reserva por operación; buffer prestado |
| DNS/archivo async | trabajo en pool de I/O | Dos hilos compartidos; copias documentadas |
| gRPC cliente | conexión HTTP/2 + buffers | Una conexión por llamada |
| Ventana TUI | buffers de celdas anterior/actual | Proporcional a ancho × alto |

El runtime se incluye en el C generado según módulos utilizados. Una aplicación
simple no incorpora el transporte HTTP/2. nghttp2 se compila desde sus 26
unidades C únicamente cuando se importa gRPC. No se distribuye un servidor
Python como parte del runtime.

El REPL compila celdas con libtcc, conserva código y almacena variables globales
en slots persistentes. No vuelve a ejecutar inicializadores ya completados.
Los slots no son un GC: destruya recursos antes de `:reset`/`:quit`. La retención
de código es necesaria para punteros guardados; está limitada a 128 celdas y
64 MiB de slots. Una celda fallida puede haber modificado estado; no hay rollback.

Límites defensivos: 32 MiB por fuente, dos millones de tokens, profundidad
sintáctica/semántica 256, 256 módulos, 512 instancias genéricas y límites
específicos de protocolos. El fuzzing actual combina entradas aleatorias
deterministas con casos válidos, negativos y de regresión. No demuestra ausencia
de errores para toda entrada posible.

`tools/benchmark.py` mide tiempo real de compilación y ejecución, tamaño de
ejecutable y memoria máxima de procesos donde el host ofrece esa métrica. Son
mediciones locales, sin promesas generales de rendimiento. Ejecute benchmarks
en una máquina dedicada antes de comparar optimizaciones o equipos diferentes.
