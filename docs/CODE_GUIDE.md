# Guía del código de TinyC+

Esta guía explica cómo recorrer y mantener el código propio del proyecto.
Combina responsabilidades por archivo, contratos de memoria, puntos de entrada y
pruebas. Las firmas de la biblioteca están en [API.md](API.md); la semántica del
lenguaje está en [LANGUAGE.md](LANGUAGE.md). Para estudiar una función concreta,
lea su implementación junto con la fase anterior y la prueba que ejerce su caso.

## Construir, ejecutar y comprobar

Desde la raíz del repositorio, en Linux/macOS con Python 3.9+, `cc`, `make` y `sh`:

```sh
python3 tools/bootstrap.py --verify-only
python3 tools/bootstrap.py --only tcc --cc cc
python3 build.py --cc cc
./bin/tiny run examples/hello.tc
./bin/tiny run examples/hello.tc --cc cc
python3 -m venv /workspace/.tinycplus-venv
/workspace/.tinycplus-venv/bin/python -m pip install -r tests/requirements.txt
/workspace/.tinycplus-venv/bin/python tests/run_all.py --cc cc --fuzz 1000
```

El virtualenv queda fuera del checkout. La ruta `/workspace` corresponde al entorno
cloud; en otra máquina elija una ruta equivalente. El bootstrap de TinyCC verifica
el archivo fuente antes de reconstruir y escribe su instalación POSIX ignorada por
Git. El bootstrap completo también regenera nghttp2 versionado: no es necesario
hacerlo para trabajar con las fuentes de nghttp2 ya incluidas. Los ejecutables
Windows distribuidos no se ejecutan directamente en Linux.

El resultado de las pruebas se guarda en `build/test-report.json`. Los endpoints
son locales y efímeros: no hace falta configurar un servidor ni credenciales.
`--skip-interop` excluye Protobuf/gRPC con Python y reduce la cobertura. Para las
particularidades de Windows, consulte el [README](../README.md).

## Recorrido de una compilación

```text
CLI (main.c)
  -> load_program (modules.c): prelude, fuente e imports
       -> lex (lexer.c): tokens con ubicación
       -> parse (parser.c): AST
  -> expand_generics (generic.c): instancias concretas
  -> analyze (semantic.c): nombres, tipos y restricciones
  -> generate_c (cgen.c): C11 y runtime necesario
  -> backend (backend.c): libtcc o compilador C externo
```

`tc_compile` crea el contexto y delimita el manejo de errores. La CLI elige dónde
terminar: tokens y AST son inspecciones tempranas; `check` llega al análisis
semántico; `--emit-c` genera C; `build` compila y enlaza; `run` ejecuta. Un `check`
correcto no garantiza que el enlace funcione. `test` activa el modo de pruebas del
lenguaje. `fmt`, `doc` y `repl` tienen sus entradas en `tooling.c`.

Para observar un caso sin modificar la fuente:

```sh
mkdir -p build
./bin/tiny --emit-tokens examples/hello.tc
./bin/tiny --emit-ast examples/hello.tc
./bin/tiny check examples/hello.tc
./bin/tiny --emit-c examples/hello.tc -o build/hello.c
./bin/tiny build examples/hello.tc --cc cc -o build/hello
./build/hello
```

### Archivos del frontend

| Archivo | Responsabilidad y puntos de entrada |
| --- | --- |
| [tiny.h](../compiler/tiny.h) | Contrato de datos y funciones compartidas; define `Context`, `Node`, `Type`, `Symbol`, `Scope`, `Loc` y tokens. |
| [project.c](../compiler/project.c) | Manifiesto nativo, rutas relativas, opciones de enlace y ejecución de tests; [guía de proyectos](PROJECTS.md). |
| [main.c](../compiler/main.c) | `main` interpreta opciones y rutas; `tc_compile` coordina fases, códigos de salida y liberación de la unidad. |
| [lexer.c](../compiler/lexer.c) | `lex` reconoce identificadores, literales, operadores y comentarios; `dump_tokens` expone el resultado. |
| [parser.c](../compiler/parser.c) | `parse` produce declaraciones y sentencias; `expr` aplica precedencia; `parse_type` reconoce tipos; `dump_ast` imprime el árbol. |
| [modules.c](../compiler/modules.c) | `load_program` reúne prelude e imports; `load_module` carga cada módulo; normaliza nombres y tipos calificados. |
| [types.c](../compiler/types.c) | Familias `type_primitive`, `type_named`, `type_derive`, `type_tuple`, `type_generic`, `type_const`; `type_equal` compara tipos y `type_name` produce nombres. |
| [generic.c](../compiler/generic.c) | `expand_generics` crea instancias; `sub_type` y `clone` sustituyen tipos y copian los nodos de las plantillas. |
| [semantic.c](../compiler/semantic.c) | `analyze` organiza el análisis; `check_expr`, `check_stmt` y `check_function` validan el árbol; `bind`/`lookup` gestionan símbolos; `compatible` decide conversiones. |
| [cgen.c](../compiler/cgen.c) | `generate_c` emite la unidad; `expression`/`statement` bajan operaciones; `CG` guarda estado de emisión, temporales, bucles y limpiezas. |
| [backend.c](../compiler/backend.c) | `backend` compila/ejecuta C; `load_tcc` carga la biblioteca; `backend_option` añade opciones; `tc_repl_clear` libera estado retenido. |
| [ownership.c](../compiler/ownership.c) | Diagnósticos locales optativos de copias directas y uso tras liberar; no es un borrow checker. |
| [tooling.c](../compiler/tooling.c) | `tc_format_file` conserva comentarios al formatear; `tc_document_file` extrae declaraciones y comentarios; `tc_repl` procesa celdas. |
| [util.c](../compiler/util.c) | Arena (`tc_alloc`, `tc_free`), cadenas, `Buffer`, archivos, creación de nodos y diagnósticos (`tc_error`). |

### Datos y contratos entre fases

- `Loc` identifica archivo, línea y columna: debe conservarse al transformar nodos
  para que el diagnóstico y los `#line` del C remitan a la fuente correcta.
- `NodeKind` determina qué campos de `Node` son válidos. `a`, `b`, `c`, `body`,
  `params` y `args` representan enlaces distintos según el nodo; `next` enlaza
  listas. `resolved`, `sym` y `type` reciben información del análisis.
- `Type` representa primitivas y composiciones; `base`, `items`, `count`, `decl`
  y `alias` se interpretan según `TypeKind`. No compare únicamente nombres para
  implementar una regla de tipos.
- `Scope.parent` forma la cadena de ámbitos y `Symbol` relaciona nombres,
  declaraciones y tipos. La emisión usa los nombres C resueltos.
- `Context` reúne arena, tokens, tipos, programa, ámbitos y opciones por unidad.
  Los objetos reservados con `tc_alloc` duran hasta `tc_free`; no se liberan
  individualmente. Los buffers que usan `malloc`/`realloc` tienen gestión propia.
- `tc_error` usa el `jmp_buf failure` configurado por el llamador. Un recurso
  reservado fuera de la arena necesita una ruta de liberación que contemple
  este salto; no presuponga que se ejecutará el resto de la función.

No hay IR optimizador independiente. Genéricos, closures, interfaces, streams y
async necesitan acuerdos entre análisis y generación. Para entender costes y
representaciones concretas, consulte [ARCHITECTURE.md](ARCHITECTURE.md).

## Runtime C

El runtime se incorpora según las necesidades del C emitido. Sus funciones `tc_*`
son el puente de los wrappers `.tc`; no son métodos de una VM. Al cambiar una
firma, compruebe su declaración `extern C`, la llamada emitida y la implementación.

| Archivo | Responsabilidad | Contrato que conviene revisar |
| --- | --- | --- |
| [tiny_runtime.h](../runtime/tiny_runtime.h) | `TinyString`, memoria, impresión, hash, asserts, índices y slices. | Las comprobaciones fallidas llaman a `tc_panic` y terminan con 101. Una vista de string no implica propiedad. |
| [library.c](../runtime/library.c) | Strings, consola, argumentos, archivos, procesos y utilidades de la biblioteca. | Las API del host que requieren NUL necesitan conversión; distinguir reserva propia y vista prestada. |
| [process.c](../runtime/process.c) | Procesos argv, wait con timeout, cancelación y recolección del hijo. | Un handle posee al hijo; destruirlo cancela/espera si sigue vivo. |
| [gui.c](../runtime/gui.c) | Renderer, superficies, widgets y backend de ventana común. | Operaciones en el hilo de UI; no destruir superficies prestadas. X11/Cocoa son optativos. |
| [concurrent.h](../runtime/concurrent.h) | ABI opaca de hilos, sincronización, pools, futures y canales. | Emparejar creación/destrucción y referencias de futures. |
| [concurrent.c](../runtime/concurrent.c) | Implementaciones Windows/POSIX y scheduler de tareas. | Los argumentos de callbacks deben seguir vivos al ejecutarse; cierre de canales y fin del pool son operaciones distintas. |
| [net.c](../runtime/net.c) | TCP, UDP, DNS, reactor y trabajo asíncrono de I/O. | Conservar buffers prestados hasta completar la operación; comprobar errores y cerrar handles. |
| [protobuf.c](../runtime/protobuf.c) | Varints, ancho fijo, bytes, UTF-8 y salto seguro de campos desconocidos. | El lector mantiene una vista de entrada; revisar transferencia de almacenamiento al extraer la salida del escritor. |
| [grpc.c](../runtime/grpc.c) | Clientes, servidor, rutas y framing gRPC sobre nghttp2. | Unary h2c sin TLS, compresión ni streaming; comprobar estados y destruir respuestas. |
| [gapbuffer.c](../runtime/gapbuffer.c) | Texto editable, cursor, líneas, búsqueda y cambios. | El almacenamiento se indexa en bytes; el movimiento por caracteres respeta UTF-8. |
| [tui.c](../runtime/tui.c) | Terminal, celdas, eventos, widgets, layout, replay y snapshots. | Destruir la ventana restaura recursos del terminal; headless permite pruebas sin consola interactiva. |

`defer` es una construcción del compilador para emitir limpiezas. No convierte
objetos con handles en recursos gestionados por GC. Las reglas sobre closures,
futures y buffers se detallan en [CONCURRENCY.md](CONCURRENCY.md), las de protocolos
en [PROTOCOLS.md](PROTOCOLS.md) y las de terminal en [TUI.md](TUI.md).

## Biblioteca estándar

Cada módulo define la interfaz del lenguaje y, cuando corresponde, el wrapper de
una función C. La referencia enlazada contiene firmas y comentarios extraídos.

| Fuente | Abstracciones | Referencia |
| --- | --- | --- |
| [prelude.tc](../std/prelude.tc) | Declaraciones C básicas de reserva, liberación y salida; carga implícita. | [prelude](api/prelude.md) |
| [core.tc](../std/core.tc) | `minInt`, `maxInt`, `identity<T>`. | [core](api/core.md) |
| [memory.tc](../std/memory.tc) | `Memory`: operaciones explícitas sobre memoria. | [memory](api/memory.md) |
| [string.tc](../std/string.tc) | `OwnedString`, `String`, `StringBuilder`: vistas, propiedad y construcción. | [string](api/string.md) |
| [collections.tc](../std/collections.tc) | `Array<T>`, `Map<K,V>`, `Set<T>`, `Deque<T>`, `PriorityQueue<T>`, `BitSet` y `OrderedMap<K,V>`; [contratos y costes](COLLECTIONS.md). | [collections](api/collections.md) |
| [io.tc](../std/io.tc) | `Console`: entrada y salida. | [io](api/io.md) |
| [fs.tc](../std/fs.tc) | `File`: lectura, escritura y recursos de archivo. | [fs](api/fs.md) |
| [process.tc](../std/process.tc) | `Arguments` y `Process`: argumentos y procesos. | [process](api/process.md) |
| [math.tc](../std/math.tc) | `Math` y funciones C matemáticas. | [math](api/math.md) |
| [time.tc](../std/time.tc) | `Timer`: reloj, espera y temporización asíncrona. | [time](api/time.md) |
| [concurrent.tc](../std/concurrent.tc) | Hilos, mutex, condiciones, semáforos, eventos, atómicos, cancelación, pools, canales, futures y grupos. | [concurrent](api/concurrent.md) |
| [net.tc](../std/net.tc) | `Address`, `Dns`, `TcpSocket`, `UdpSocket`, `AsyncFile`. | [net](api/net.md) |
| [protobuf.tc](../std/protobuf.tc) | `ProtoWriter`, `ProtoReader` y ABI de codificación. | [protobuf](api/protobuf.md) |
| [grpc.tc](../std/grpc.tc) | `GrpcResponse`, `GrpcClient`, `GrpcServer`. | [grpc](api/grpc.md) |
| [tui.tc](../std/tui.tc) | `GapBuffer`, `UiEvent`, `Widget`, `Ui`, `Window`. | [tui](api/tui.md) |

Para regenerar referencias después de cambiar documentación pública:

```sh
python3 tools/generate_docs.py
```

El generador escribe `docs/API.md` y `docs/api/*.md`: revise el diff. `tiny doc`
extrae declaraciones sin ejecutar la aplicación ni validar un enlace completo.

## Aplicación y ejemplos

[apps/tinyedit.tc](../apps/tinyedit.tc) implementa el editor. `main` interpreta
`FILE`, `--replay` y `--snapshot`, carga texto, construye widgets y procesa eventos.
El estado incluye portapapeles, modo del prompt, confirmación de salida y posición
del replay. `defer` destruye strings y ventana. El texto editable reside en el
`GapBuffer`; la aplicación coordina búsqueda, navegación, guardado y atajos.
`tests/test_editor.py` verifica estas rutas con entradas reproducibles.

Los ejemplos son programas ejecutables y material de uso, no implementaciones
alternativas del compilador:

| Archivos en `examples/` | Qué estudiar |
| --- | --- |
| `hello.tc`, `core.tc` | Entrada al lenguaje y operaciones básicas. |
| `collections.tc`, `streams.tc` | Colecciones y transformaciones de secuencias. |
| `async.tc` | Tareas y espera asíncrona. |
| `ffi.tc`, `ffi_math.c` | Frontera con C; la fuente auxiliar se enlaza con `--c-source`. |
| `tui.tc` | Widgets y eventos del terminal. |
| `users.proto`, `users.tc` | Esquema y bindings generados de mensajes y servicios. |
| `grpc_echo.tc`, `grpc_users.tc` | Uso del transporte y servicio gRPC. |
| `tests.tc` | Declaraciones de pruebas del lenguaje para `tiny test`. |

[El tutorial](../examples/tutorial/README.md) recorre los capítulos 01–12: primer
programa, control, arrays/errores, memoria, objetos, módulos/FFI, colecciones,
archivos, concurrencia, red async, servidor y dashboard TUI. `07_modules/` contiene
el módulo auxiliar y la función C. `exercises/` contiene casos de práctica,
incluidos fallos deliberados; `solutions/` contiene soluciones y su índice.
`experimental/13_gui_dashboard.tc` es material experimental: no demuestra soporte
GUI estable. Consulte su README antes de intentar ejecutarlo.

## Herramientas y automatización

| Archivo | Entradas, salidas y efectos |
| --- | --- |
| [build.py](../build.py) | Selecciona compilador, reúne `compiler/*.c`, aplica flags C11 y genera `bin/tiny` y copia `bin/tinyc`; admite debug y sanitizers. |
| [bootstrap.py](../tools/bootstrap.py) | Lee `third_party/dependencies.json`, verifica SHA-256, extrae fuentes y reconstruye TinyCC o regenera nghttp2. `--verify-only` no reconstruye. |
| [tiny-protoc.py](../tools/tiny-protoc.py) | `Parser` construye `Schema`/mensajes/campos/enums/servicios; `Generator` emite TinyC+ para proto3 y gRPC unary. Diagnostica sintaxis no soportada. |
| [generate_docs.py](../tools/generate_docs.py) | Invoca `tiny doc` para cada módulo estándar y actualiza el índice de API. |
| [benchmark.py](../tools/benchmark.py) | Mide compilación, ejecución, tamaño y memoria donde el host lo permite; resultados locales, no garantías universales. |
| [package.py](../tools/package.py) | Construye ZIP con selección de archivos y manifiesto SHA-256; excluye temporales e instalación POSIX de TinyCC. |
| [publish-wiki.py](../tools/publish-wiki.py) | Publica los Markdown en el repositorio de wiki mediante Git; requiere autorización y acceso de escritura. No forma parte del build ni de las pruebas locales. |
| [.github/workflows/ci.yml](../.github/workflows/ci.yml) | Matriz nativa y sanitizers; consulte el nombre real del workflow en `.github/workflows/` si se renombra. |

No publique la wiki como efecto secundario de generar documentación local.
Los binarios y fuentes de terceros tienen procedencia y licencias en
[THIRD_PARTY.md](../THIRD_PARTY.md). `third_party/tcc/` incluye el paquete Windows
y el archivo fuente; `third_party/nghttp2/` incluye fuentes, configuración, lista
de unidades y parche local. No añada comentarios propios a copias vendorizadas;
documente cambios de integración y conserve verificaciones y licencias.

## Pruebas y diagnóstico

| Archivo bajo `tests/` | Área que comprueba |
| --- | --- |
| `run_modules.py`, `test_module_bench.py` | Banco focalizado con selección, comparación de backends, sanitizers, informes JSON/JUnit y pruebas del ejecutor; [guía](../tests/MODULE_BENCH.md). |
| `run_all.py` | Orquesta etapas, captura stdout/stderr y conserva códigos de salida en el informe JSON. |
| `test_compiler.py` | Casos válidos/negativos, diagnósticos, lowering, comparación con compilador externo y fuzz determinista. |
| `test_modules_ffi.py` | Imports, resolución de módulos y ABI con C. |
| `test_tools.py` | CLI, formato, documentación y REPL. |
| `test_project.py` | Build/run/check/test de proyectos y configuración inválida. |
| `test_diagnostics.py` | Fragmentos de fuente, subrayado y contexto de genéricos. |
| `test_iterators.py`, `iterators.tc` | Protocolo iterator/next, orden, invalidación y recorridos async. |
| `test_collections.py`, `collections.tc`, `clones.tc` | Colecciones comparadas con modelos deterministas y errores de uso, con ambos backends. |
| `test_stdlib.py` | Biblioteca estándar, I/O, argumentos y procesos. |
| `test_editor.py` | Edición mediante replay, snapshots y persistencia. |
| `test_golden.py`, `golden/` | Comparación del C emitido con snapshots de `defer` y streams. |
| `test_tutorial.py` | Programas del tutorial y sus resultados esperados. |
| `runtime_concurrent.c` | Primitivas de concurrencia a nivel C. |
| `runtime_network.c` | Red y operaciones relacionadas a nivel C. |
| `runtime_tui.c` | Terminal, widgets y buffer de edición a nivel C. |
| `test_protobuf.py`, `wire.proto` | Codificación e interoperabilidad con Protobuf Python. |
| `test_grpc.py` | Cliente y servidor interoperables con grpcio en loopback. |
| `test_distribution.py` | SHA-256 y ejecución de un paquete extraído en otra ruta; se ejecuta aparte con la ruta del ZIP. |

Un fallo de sintaxis se investiga desde tokens/parser; uno de tipos, desde
semantic/types; C incorrecto, desde cgen y los golden; enlace o carga de libtcc,
desde backend y rutas; errores de ejecución, desde runtime y su wrapper estándar.
No actualice un golden antes de entender por qué cambió el C. Una prueba negativa
que espera error no equivale a un fallo de infraestructura.

## Mantener la documentación

Al añadir una construcción, documente sintaxis, semántica, representación emitida
y limpieza; al añadir una API, describa propiedad, errores y restricciones de
plataforma. Mantenga esta guía sincronizada con archivos nuevos y sus pruebas.
Los documentos de `docs/validation/` guardan evidencia histórica, no garantizan
que el checkout actual haya pasado una suite. `docs/STATUS.md`, `ROADMAP_AUDIT.md`,
`SELFHOSTING.md` y `SPEC.md` distinguen estado, auditoría, planes y requisitos;
`WORKLOG.md` conserva el historial. La wiki ofrece recorridos temáticos y curso.

Los contratos y pruebas de las ampliaciones recientes están en [FUNCTIONAL_IMPROVEMENTS.md](FUNCTIONAL_IMPROVEMENTS.md).
