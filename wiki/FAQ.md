# Preguntas frecuentes

## ¿TinyC+ es C++ simplificado?

No. Comparte con C++ algunas ideas —objetos por valor y especialización
genérica— pero evita herencia, templates complejos, exceptions y gran parte de
su runtime/ABI.

## ¿Tiene garbage collector?

No.

## ¿Tiene ARC?

No como política universal.

## ¿Tiene borrow checker?

No.

## Entonces, ¿cómo se evita perder memoria?

Con ownership explícito, `destroy()`, destructores, `delete`, `free`,
`defer`, pruebas y sanitizers.

## ¿Por qué generar C?

Porque C ofrece una frontera portable y comprensible, permite usar TinyCC,
GCC/Clang y herramientas existentes, y mantiene visible el lowering.

## ¿TinyCC es obligatorio?

No para todos los flujos. libtcc es central y requerido por el REPL, pero el
backend externo permite compilar con GCC/Clang.

## ¿Hay JIT?

El REPL usa libtcc para compilar celdas en memoria. TinyC+ no se diseña como una
VM/JIT optimizadora.

## ¿Puedo llamar bibliotecas C?

Sí, mediante `extern C` y opciones de include/link.

## ¿Puedo usar C++?

No hay ABI C++ automática. Exponga una frontera C si necesita integrar una
biblioteca C++.

## ¿Las clases son referencias?

No necesariamente. Las clases TinyC+ son valores por defecto.

## ¿Las interfaces reservan memoria?

La conversión estructural normal usa objeto + vtable y no requiere boxing heap.

## ¿Los streams crean arrays intermedios?

El objetivo del lowering implementado es fusionar filter/map/terminal en loops
sin colecciones intermedias cuando aplica.

## ¿Async crea un hilo por operación?

No. Las funciones async usan frames/continuaciones y el runtime comparte workers.

## ¿gRPC usa TLS?

No en 1.0 RC. Usa h2c.

## ¿Existe GUI gráfica?

Existe como trabajo post-1.0 experimental. La TUI es la interfaz de usuario
estable incluida en el alcance 1.0.

## ¿TinyEdit es un ejemplo o una aplicación?

Ambas cosas. Es una aplicación funcional y una prueba de integración del TUI,
filesystem, strings, input y ownership.

## ¿Cómo depuro el lowering?

Use:

```bash
tiny --emit-c programa.tc
tiny --emit-ast programa.tc
tiny --emit-tokens programa.tc
```

## ¿Cómo reporto un bug útil?

Incluya:

```text
fuente mínima .tc
comando exacto
plataforma
compilador backend
salida esperada
salida observada
C generado si es relevante
```
