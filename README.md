# TinyC+ 1.0.0-rc.1

Compilador nativo de TinyC+, escrito en C11, con librería estándar, concurrencia,
async/await, red, generación Protobuf, gRPC unary sobre HTTP/2, TUI y el editor
`tinyedit`. El frontend genera C inspeccionable y utiliza libtcc o un compilador
C externo. No requiere LLVM, VM, GC ni Python para ejecutar el compilador o las aplicaciones.

Esta entrega implementa las áreas del roadmap hasta 1.0. Sigue identificada como
**candidata de entrega** hasta completar la integración/release formal, pero la
matriz CI ya fue validada en Windows x64/GCC, Ubuntu x86-64/GCC, Ubuntu ARM64/GCC,
macOS ARM64/Clang y Ubuntu x86-64 con ASan/UBSan. Consulte
[cobertura y límites](docs/STATUS.md) y [evidencia CI](docs/validation/ci-2026-09-19.md).

## Empezar en Windows x64

Abra PowerShell en esta carpeta. Los ejecutables y TinyCC están incluidos:

```powershell
.\bin\tiny.exe --version
.\bin\tiny.exe run examples\hello.tc
.\bin\tiny.exe run examples\core.tc
.\bin\tiny.exe run examples\collections.tc
.\bin\tiny.exe run examples\streams.tc
.\bin\tiny.exe run examples\async.tc
.\bin\tiny.exe run examples\grpc_users.tc
.\bin\tiny.exe build apps\tinyedit.tc -o bin\tinyedit.exe
.\bin\tinyedit.exe notas.txt
```

`tinyedit`: Ctrl-S guarda; Ctrl-Q sale (dos veces si hay cambios sin guardar);
Ctrl-F busca; Ctrl-G va a una línea; Ctrl-X corta una línea; Ctrl-U pega.
También admite flechas, Home, End, Backspace y Delete. Requiere una consola
con soporte ANSI. [Ejemplo de widgets](examples/tui.tc).

No mueva únicamente `tiny.exe`: conserve `compiler` (para reconstruir), `runtime`,
`std` y `third_party` junto al directorio `bin`. Para otra disposición use
`TINY_HOME` o `--home RUTA`.

## Compilar el compilador

Python 3.9 o superior se usa solamente para los scripts de construcción,
generación Protobuf y pruebas. La ruta al compilador C puede contener espacios.

```powershell
python build.py
python build.py --cc "C:\ruta\a\gcc.exe"
python tools/bootstrap.py --verify-only
python tools/bootstrap.py --cc "C:\ruta\a\gcc.exe"
```

`bootstrap.py` verifica los SHA-256 y reconstruye TinyCC y la configuración de
nghttp2 **sin descargar nada**. Las fuentes originales, licencias y el parche
local de nghttp2 están en `third_party`. No se modificó el núcleo de TinyCC.

Para Linux/macOS, se incluyen rutas POSIX, pero no binarios para esos sistemas:

```sh
python3 tools/bootstrap.py --cc cc
python3 build.py --cc cc
./bin/tiny run examples/hello.tc
./bin/tiny run examples/async.tc --cc cc
```

El bootstrap POSIX necesita `sh`, `make`, un compilador C y el SDK del sistema.
El backend externo permite ejecutar programas sin libtcc; el REPL necesita libtcc.
La compilación es nativa para el host; no hay interfaz de compilación cruzada.

## Herramientas

```powershell
.\bin\tiny.exe check examples\core.tc
.\bin\tiny.exe build examples\core.tc -o bin\core.exe
.\bin\tiny.exe --emit-c examples\core.tc -o build\core.c
.\bin\tiny.exe --emit-ast examples\core.tc
.\bin\tiny.exe --emit-tokens examples\core.tc
.\bin\tiny.exe --emit-asm examples\core.tc --cc gcc -o build\core.s
.\bin\tiny.exe fmt examples\hello.tc
.\bin\tiny.exe doc std\net.tc -o build\net-api.md
.\bin\tiny.exe test examples\tests.tc
.\bin\tiny.exe repl
```

`tinyc` equivale al comando de compilación. `fmt` escribe el archivo formateado;
con `-o` escribe una copia. Los argumentos de un programa van después de `--`.
`check` verifica sintaxis y tipos; `build` también verifica generación de código
y enlace. Los diagnósticos indican archivo, línea y columna. Errores del
frontend retornan 1; los fallos de bounds/assert retornan 101.

## Protobuf y gRPC

```powershell
python tools\tiny-protoc.py examples\users.proto -o examples\users.tc
.\bin\tiny.exe run examples\grpc_users.tc
```

El ejemplo ejecuta un servicio y cliente generados que intercambian mensajes
Protobuf reales. El transporte implementa gRPC unary, sin compresión, sobre
HTTP/2 cleartext (h2c), con nghttp2 integrado desde fuentes. Se prueba contra
`grpcio` en ambas direcciones. TLS, RPC streaming, autenticación, reflexión y
compresión no están implementados. Véase [protocolos](docs/PROTOCOLS.md).

## Verificar

```powershell
python -m pip install -r tests\requirements.txt
python tests\run_all.py --cc gcc --fuzz 1000
python tools\benchmark.py --iterations 7
python tools\package.py
```

La suite no conecta con servidores externos: crea endpoints efímeros en loopback.
Comprueba semántica y diagnósticos, compara todos los casos ejecutables contra el
backend externo, fuzzing determinista, primitivas concurrentes, I/O, ABI C,
interoperabilidad Protobuf/gRPC, widgets y edición con entradas reproducibles.
El informe JSON se escribe en `build/test-report.json`. `--skip-interop` omite
explícitamente las pruebas que necesitan paquetes Python; no es una validación completa.

## Documentación

- [Lenguaje, memoria y ejemplos](docs/LANGUAGE.md)
- [Arquitectura y costes](docs/ARCHITECTURE.md)
- [Concurrencia y asincronía](docs/CONCURRENCY.md)
- [Protocolos y generación](docs/PROTOCOLS.md)
- [TUI y editor](docs/TUI.md)
- [Cobertura del roadmap y límites](docs/STATUS.md)
- [Índice de API estándar](docs/API.md)
- [Dependencias y licencias](THIRD_PARTY.md)

La especificación original se conserva en [docs/SPEC.md](docs/SPEC.md) como
referencia de requisitos. El comportamiento implementado se describe en estos
documentos y en las pruebas; los ejemplos de intención de la especificación no
se presentan como pruebas de funcionalidad.
