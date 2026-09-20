# Seguridad, límites y supuestos

TinyC+ es un lenguaje de sistemas con memoria manual. No debe confundirse
“tipado estático” con “memoria automáticamente segura”.

## Memoria

No existe borrow checker. El programador puede crear:

- dangling pointers;
- use-after-free;
- double free;
- slices/views inválidas;
- races si comparte memoria sin sincronización.

Las APIs y `defer` reducen errores, pero no eliminan la responsabilidad manual.

Algunas APIs añaden reglas propias:

- `Map<K,V>` y `StringBuilder`: las copias comparten el mismo almacenamiento; un
  `put` o `append` que reasigna deja colgantes las demás copias. Llame a `destroy`
  una sola vez.
- Las claves string de un `Map` son vistas: el texto debe vivir más que la
  entrada. `StringBuilder.view()` se invalida con el siguiente cambio.
- `Process.spawn` no usa shell: cada argumento llega tal cual, sin expansión ni
  redirecciones. `Process.run` sí pasa el comando a la shell del sistema.

## Aritmética

Signed overflow y shifts inválidos conservan riesgos comparables a C. No hay
aritmética checked universal.

## Concurrencia

Cancellation es cooperativa.

Bloquear todos los workers puede producir deadlock.

Los handles tienen ciclos de vida explícitos.

## Networking

Los límites de buffers, operaciones pendientes y conexiones son deliberados.

La versión gRPC 1.0 RC usa h2c: **no proporciona cifrado de transporte**.

No use esa ruta para datos sensibles a través de redes no confiables sin una
capa segura adecuada.

## FFI

El FFI confía en que el programa declare correctamente ABI y layouts. Una
declaración incorrecta puede causar corrupción de memoria.

## Protobuf

El codec tiene límites de tamaño/profundidad y soporta un subconjunto de proto3.
Schemas fuera de ese contrato deben rechazarse, no asumirse compatibles.

## TUI/Unicode

La edición trabaja con UTF-8/codepoints, pero no implementa grapheme clusters ni
shaping completo. Esto afecta experiencia visual, no sólo estética, para ciertos
scripts y combinaciones Unicode.

## Compilación

La CLI compila nativamente para el host. No existe todavía una interfaz formal
de cross-compilation.

## Qué hacer antes de producción

Para software sensible:

```text
ejecute sanitizers
active bounds checks donde tenga sentido
revise ownership
fuzz entradas no confiables
valide ABI FFI
ponga TLS/autenticación fuera del gRPC h2c actual
revise límites del reactor/servidor
haga pruebas de carga del caso real
```

La Wiki documenta capacidades; no reemplaza un threat model específico de la
aplicación.
