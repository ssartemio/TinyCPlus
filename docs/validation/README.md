# Resultados de validación

Ejecuciones locales del 15 de septiembre de 2026 en Windows x64.

| Artefacto | Resultado |
|---|---|
| windows-x64-tests.json | 22/22 etapas; 1.163 checks del compilador, incluidos 1.000 casos fuzz y 64 ejecuciones diferenciales |
| distribution.json | SHA-256 de todos los archivos y diez comprobaciones tras extraer el ZIP en otra ruta |
| terminal-session.json | Editor en ConPTY: texto Unicode, salto de línea, guardar/salir y contenido final |
| benchmarks.json / benchmarks.md | Siete repeticiones por programa; tiempo, tamaño y peak working set |

Frontend construido tanto con TinyCC 0.9.28rc como con GCC 9.2.0. Los tests
de runtime y de interoperabilidad se ejecutaron con ambos backends. La compilación
del frontend con GCC produjo cero advertencias con `-Wall -Wextra`.

Tras mejorar únicamente la presentación de tipos en `tiny doc`, se reconstruyó
con TinyCC y se repitieron las pruebas del tooling y 1.099 checks del compilador.
Los binarios finales `tiny`, `tinyc` y `tinyedit` fueron reconstruidos antes del ZIP.

El informe de distribución se obtuvo antes de agregar este índice y los propios
informes al ZIP; los tests de código usan los mismos fuentes y ejecutables.
El manifiesto SHA256SUMS del ZIP final permite comprobar su contenido completo.

La validación multiplataforma posterior está registrada en
[ci-2026-09-19.md](ci-2026-09-19.md): la matriz GitHub Actions pasó en
Windows x64/GCC, Ubuntu x86-64/GCC, Ubuntu ARM64/GCC, macOS ARM64/Clang y
Ubuntu/Clang con ASan/UBSan.

Los informes de este directorio describen la entrega rc.1 (15–19 de septiembre
de 2026). No cubren lo añadido después (`switch`, `hash()`, `Map<K,V>`,
`StringBuilder`, `Process.spawn`, finales de línea LF): esos cambios se integraron
con la misma matriz de CI, cuyo estado actual está en la pestaña Actions.
