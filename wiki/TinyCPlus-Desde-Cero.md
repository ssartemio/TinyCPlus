# TinyC+ desde cero

Curso progresivo de 13 capítulos. El proyecto acumulativo se llama **TinyStatus**:
empieza como un programa de consola, añade modelo de datos, colecciones,
persistencia, concurrencia y TCP, y termina con un dashboard TUI. El último
capítulo porta la idea a la GUI experimental post-1.0.

| # | Capítulo | Resultado |
|---:|---|---|
| 1 | [Primer programa](Curso-01-Primer-Programa) | variables, build y lowering |
| 2 | [Funciones y control](Curso-02-Funciones-y-Control) | funciones, defaults, named args |
| 3 | [Arrays, slices y errores](Curso-03-Arrays-Slices-y-Errores) | datos por lotes + Error |
| 4 | [Memoria y recursos](Curso-04-Memoria-y-Recursos) | ownership, defer, OwnedString |
| 5 | [Objetos y modelos](Curso-05-Objetos-y-Modelos) | clases, propiedades, interfaces |
| 6 | [Colecciones y streams](Curso-06-Colecciones-Closures-y-Streams) | Array<T>, lambdas, análisis |
| 7 | [Módulos y FFI](Curso-07-Modulos-y-FFI) | separar proyecto + C |
| 8 | [Persistencia](Curso-08-Persistencia-y-Archivos) | guardar/cargar estado |
| 9 | [Concurrencia](Curso-09-Concurrencia) | Thread, Future, Channel, Atomic |
| 10 | [Async y networking](Curso-10-Async-y-Networking) | TCP + await |
| 11 | [Servidor TinyStatus](Curso-11-Servidor-TinyStatus) | protocolo y servidor |
| 12 | [Dashboard TUI](Curso-12-Dashboard-TUI) | red concurrente + TUI |
| 13 | [GUI experimental](Curso-13-GUI-Experimental) | port post-1.0 |

## Método de trabajo

En cada capítulo:

1. escriba el ejemplo;
2. ejecute `tiny check`;
3. ejecútelo con `tiny run`;
4. inspeccione `tiny --emit-c`;
5. haga los ejercicios;
6. continúe sólo cuando pueda explicar ownership y coste.

Convención: `Error == 0` significa éxito. Los capítulos 1–12 usan capacidades
estables de `main`; el 13 está marcado como experimental.

[Empezar → Capítulo 1](Curso-01-Primer-Programa)


## Soluciones ejecutables

Las soluciones estables viven en
[`examples/tutorial/`](https://github.com/ssartemio/TinyCPlus/tree/main/examples/tutorial)
y forman parte de la CI mediante `tests/test_tutorial.py`.

El capítulo 13 se conserva en
[`examples/tutorial/experimental/`](https://github.com/ssartemio/TinyCPlus/tree/main/examples/tutorial/experimental)
porque requiere `std.gui` post-1.0.
