# Estado de la entrega

Versión del código: **1.0.0**. Incluye las áreas del roadmap hasta 1.0,
instaladores nativos y licencia Apache-2.0 para el código propio. Las
[notas de publicación](releases/1.0.0.md) delimitan los paquetes y plataformas.
La especificación mezcla requisitos inmediatos, intención de diseño y
ampliaciones futuras: la siguiente tabla indica qué se puede ejecutar.

| Etapa | Implementación entregada | Evidencia principal |
|---|---|---|
| 0.0 | Frontend C11, tokens/AST/C, libtcc, ejecutables | tests/test_compiler.py; bootstrap offline |
| 0.1 | Tipos, control, punteros, new/delete, defer, strings, arrays/slices, tuples/Error | Casos nativos, negativos, bounds y comparación con GCC |
| 0.2 | Clases, constructor/destructor, propiedades, extensiones, interfaces, named/default args | Regresiones de objetos, retorno de recursos, vtables |
| 0.3 | Monomorfización, Array, funciones/lambdas, C FFI, librería estándar | tests/test_modules_ffi.py; ABI structs/enums/callbacks |
| 0.4 | Closures prestadas/owned/anidadas, range, streams fusionados, fmt/doc/REPL | Casos streams/capturas; tests/test_tools.py |
| 0.5 | Threads, mutex, condition, semaphore, event, Atomic portable | tests/runtime_concurrent.c |
| 0.6 | Pool, Task/Future, spawn, Channel, cancelación | Pruebas con un worker, 20.000 incrementos y backpressure |
| 0.7 | Async state machines, TCP/DNS/archivos async, TaskGroup | Frames con suspensión, errores, defer y grupos |
| 0.8 | TCP/UDP/DNS, reactor, timers, HTTP/2 vía nghttp2 | tests/runtime_network.c; interop HTTP/2 |
| 0.9 | Protobuf proto3, generador, cliente/servicio unary gRPC | tests/test_protobuf.py y test_grpc.py contra implementaciones independientes |
| 0.10 | Ventanas/celdas, widgets/layout/eventos, editor gap buffer | tests/runtime_tui.c, tests/test_editor.py |
| 1.0 | Integración, bootstrap reproducible, documentación, pruebas, benchmarks, instaladores y licencia | tests/run_all.py; docs/validation; tools/build_installers.py; LICENSE |

## Verificación y alcance

Windows x64: frontend construido con TinyCC y GCC; todos los casos de ejecución
del lenguaje se comparan contra GCC. Las pruebas nativas de concurrencia, red
y TUI se construyen y ejecutan con ambos. Protobuf y gRPC se prueban contra
protobuf/grpcio; el editor se prueba por replay y snapshots textuales.

Los informes conservados en `docs/validation` identifican la fecha y el commit
que se verificó; una ejecución histórica no certifica cambios posteriores.
Los benchmarks no comparan otros lenguajes ni prometen tasas universales.

Como antecedente, la matriz GitHub Actions se ejecutó satisfactoriamente sobre el commit
`662fed1acc1235fd191543aa7ce8ac45f2e83c0d` (run #22): Ubuntu x86-64/GCC,
Ubuntu ARM64/GCC, macOS ARM64/Clang, Windows x64/GCC y un job Ubuntu/Clang con
AddressSanitizer + UndefinedBehaviorSanitizer. En los jobs nativos se ejecutó
`tests/run_all.py --fuzz 1000`, se reconstruyó el frontend con TinyCC y se
repitió `tests/test_compiler.py --fuzz 1000`. La evidencia está en
`docs/validation/ci-2026-09-19.md`.

Además de layout/eventos por replay, se ejecutó TinyEdit en una terminal Windows
ConPTY real: entrada Unicode/multilínea, Ctrl-S, contenido guardado verificado,
Ctrl-Q y restauración de cursor/salida con código cero. No se realizó una
inspección visual humana en distintos emuladores de terminal.

## Distribución 1.0

Los instaladores se construyen para Windows x64, Ubuntu 24.04 amd64 y Fedora 42
x86_64. Linux requiere cabeceras, bibliotecas y objetos de inicio de la libc
para que TinyCC pueda compilar programas del usuario; los paquetes declaran esas
dependencias. Las pruebas de instalación deben ejecutarse también en entornos
limpios que no hereden las herramientas del host de construcción.

Ubuntu ARM64 y macOS ARM64 forman parte de la matriz de pruebas desde fuentes;
esta entrega no publica instaladores nativos para ellos. El `.deb` construido
en Ubuntu 24.04 no se anuncia como compatible con versiones anteriores ni con
cualquier Debian. Las restricciones ABI de glibc se expresan en sus dependencias.
Consulte [INSTALLERS.md](INSTALLERS.md) para instalar, crear programas propios y
verificar las descargas. Los paquetes se distribuyen sin firma Authenticode/GPG.

## Límites que forman parte de esta versión

- Memoria manual; copiar propietarios aliasa recursos. Los contenedores no
  destruyen sus elementos automáticamente. El destructor exterior debe limpiar
  recursos de miembros compuestos. No hay borrow checker ni comprobación de vida
  de closures prestadas; véase LANGUAGE.md.
- Tipos declarados en módulos diferentes deben tener nombres únicos globales;
  las funciones sí tienen resolución por módulo. Las llamadas genéricas usan
  nombres de tipo importados sin prefijo de módulo.
- Generación C sin optimizador propio. Algunas restricciones aparecen al compilar
  el C generado. La CLI no ofrece cross-compilation ni ABI de 32 bits validada.
- TaskGroup ofrece cancelación cooperativa y espera explícita; no hay select.
  Bloquear todos los workers puede producir deadlock. DNS del sistema no puede
  cancelarse durante la llamada; el reactor y servidor tienen límites acotados.
- gRPC unary/h2c, sin TLS, compresión, auth, metadata, reflexión o streaming RPC.
  Protobuf cubre el subconjunto proto3 documentado y rechaza proto2/oneof.
- TUI usa codepoints y anchura aproximada; no implementa grapheme clusters ni
  shaping Unicode completo. Los paths Windows se limitan a las APIs ANSI/C.
- REPL conserva hasta 128 celdas/64 MiB de slots; no deshace efectos de celdas
  fallidas ni libera objetos propietarios automáticamente al reiniciar.

Estos límites forman parte del contrato de **1.0**. GUI/SwiftUI-like, herencia, GC/ARC,
exceptions, reflexión y metaprogramación compleja permanecen fuera del alcance
solicitado hasta 1.0.
