# Roadmap y estado

## 1.0.0-rc.1

La rama `main` cubre el roadmap funcional hasta 1.0:

```text
0.0 frontend/bootstrap
0.1 core procedural
0.2 modelo de objetos
0.3 genéricos/FFI/colecciones
0.4 closures/streams/tooling
0.5 threads
0.6 tasks/channels
0.7 async/await
0.8 networking
0.9 protobuf/gRPC
0.10 TUI/TinyEdit
1.0 integración, pruebas y release qualification
```

La matriz multiplataforma requerida quedó verde. La etiqueta RC permanece hasta
completar la integración/release formal.

## Límites contractuales de 1.0

Memoria manual.

Sin borrow checker.

Sin exceptions.

Sin herencia clásica.

Sin cross-compilation oficial.

Sin ABI 32-bit validada.

gRPC unary/h2c, sin TLS/streaming.

TUI sin grapheme shaping completo.

Módulos con algunas restricciones de nombres de tipos.

## Post-1.0

La línea actual de experimentación principal es GUI gráfica:

```text
foundation
  ↓
Win32/GDI
  ↓
widgets básicos
  ↓
Linux/X11
  ↓
macOS/Cocoa
  ↓
widgets/layout más ricos
```

La regla para promover una feature experimental debe ser la misma que para el
resto del proyecto:

```text
API coherente
ownership claro
coste conocido
tests headless
test nativo de plataforma cuando aplica
CI verde
documentación
```

## Posibles ampliaciones posteriores

Sin convertirlas todavía en promesas de release:

- TLS para gRPC/networking;
- RPC streaming;
- mayor cobertura Protobuf;
- select/structured concurrency más rica;
- mejores backends de fuente/texto;
- GUI multiplataforma madura;
- mejor story de módulos/namespaces;
- ports adicionales de plataforma;
- tooling de distribución/paquetes.

El roadmap debe seguir siendo consecuencia de casos de uso reales, no una lista
de features por acumulación.
