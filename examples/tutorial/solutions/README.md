# Soluciones ejecutables del curso

Cada archivo resuelve explícitamente los ejercicios de programación del capítulo
correspondiente. Los ejemplos acumulativos de `examples/tutorial/01_...` a
`12_...` muestran la evolución de TinyStatus; esta carpeta separa las
**soluciones de práctica** para que el alumno pueda comparar después de intentar
los ejercicios por su cuenta.

Los ejercicios de inspección (`--emit-c`, AST, tokens), ASan y fallos
deliberados se validan por comandos o por `examples/tutorial/exercises/`.

El capítulo 7 reutiliza `../07_modules/` porque la solución correcta es
multiarchivo y necesita `checksum.c`.

El capítulo 13 se encuentra en `../experimental/` y no se ejecuta contra
`main` porque depende de la GUI post-1.0.
