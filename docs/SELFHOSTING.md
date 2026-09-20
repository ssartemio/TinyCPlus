# Plan de autohospedaje

Objetivo: que el compilador de TinyC+ esté escrito en TinyC+ y se compile a sí
mismo. Este documento fija las decisiones previas al port; es una propuesta de
trabajo y puede ajustarse antes de iniciar cada fase.

## Etapas

| Etapa | Qué es | Cómo se obtiene |
|---|---|---|
| 0 | Compilador actual en C (`compiler/`) | `python build.py`, desde una etiqueta estable |
| 1 | Compilador escrito en TinyC+ (`selfhost/`) | compilado por la etapa 0 |
| 2 | El mismo código fuente | compilado por la etapa 1 |
| 3 | El mismo código fuente | compilado por la etapa 2 |

El autohospedaje queda verificado en el **punto fijo**: el C que emiten las
etapas 2 y 3 es idéntico byte a byte. `compiler/` se conserva como semilla; el
C generado de la etapa 1 podrá distribuirse para arrancar con solo un
compilador de C.

## Estado

| Pieza | Estado | Verificación |
|---|---|---|
| `--emit-typed-ast` en la etapa 0 | hecho | determinista, anotado con tipos y con el mismo error semántico que `check` |
| Lexer (`selfhost/lexer.tc`) | hecho: mismo stdout, stderr y código de salida que `--emit-tokens` | `tests/test_selfhost.py` |
| Parser | pendiente | `--emit-ast` |
| Semántica | pendiente | `--emit-typed-ast` |
| Generación de C | pendiente | `--emit-c` y las pruebas existentes |

`tests/test_selfhost.py` compila la etapa 1 con la etapa 0 y compara ambas sobre
todos los `.tc` del repositorio, 64 casos límite escritos a mano (errores léxicos,
literales, BOM, NUL, el límite de tokens) y entradas aleatorias con semilla fija.
Se ejecuta dentro de `tests/run_all.py`. Para probarlo a mano:

```sh
./bin/tiny build selfhost/main.tc -o build/selfhost/tinyc1
./build/selfhost/tinyc1 --emit-tokens examples/hello.tc
python3 tests/test_selfhost.py
```

## Notas del port

- Los módulos de la etapa 1 son planos: `import lexer;` se resuelve relativo al
  directorio del archivo que importa, y los nombres de tipo deben ser únicos en
  todo el programa (véase STATUS.md).
- El lexer reproduce detalles de la etapa 0 que no son evidentes: las columnas
  cuentan bytes, el texto se corta en el primer NUL, un literal con más de un
  carácter UTF-8 no es un `char` válido y la etapa 0 rechaza más de 1.048.576
  tokens.
- Una sola función `fail(archivo, línea, columna, mensaje)` emite cada diagnóstico
  y termina con `exit(1)`, como describe la estrategia de errores.
- TinyC+ no tiene operador ternario: el port usa variables intermedias.

## Reglas durante el port

- La sintaxis del lenguaje se congela mientras se porta. Un cambio de lenguaje
  implica actualizar la etapa 0 y la etapa 1 en el mismo PR.
- La etapa 1 solo usa lo que acepta la etapa 0 de la etiqueta de referencia.
- Los formatos de `--emit-tokens`, `--emit-ast` y `--emit-typed-ast` son un
  contrato: cualquier cambio se hace en ambos compiladores a la vez.

## Alcance de la primera versión

Se porta el frontend y la CLI de `build`/`run`/`check`: lexer, parser,
módulos, genéricos, análisis semántico y generación de C. `fmt`, `doc`, el
REPL y la carga de libtcc siguen en C hasta que el frontend alcance el punto
fijo; la etapa 1 usa un compilador de C externo mediante `Process.spawn`.

## Estrategia de errores

TinyC+ no tiene excepciones y la etapa 0 detiene la compilación en el primer
error (`tc_error` con `longjmp`). La etapa 1 hará lo mismo sin `longjmp`:

1. Toda diagnosis pasa por una sola función `error(Loc, string)` que escribe
   `archivo:línea:columna: error: mensaje` con `Console.writeErrorLine`,
   exactamente con el formato actual.
2. Después llama a `exit(1)`. No hay recuperación de errores ni diagnósticos
   múltiples en la primera versión.
3. La memoria del compilador no se libera explícitamente: es un proceso por
   lotes de vida corta. Los nodos y tipos se crean con `new` y viven hasta el
   final del proceso, como el arena de la etapa 0.

Así las pruebas negativas existentes (`FAIL` en `tests/test_compiler.py`), que
comparan el texto del error, sirven sin cambios para ambas etapas.

## Pruebas diferenciales

Por cada fase portada, un arnés ejecuta ambos compiladores sobre `examples/`,
`std/`, `apps/`, `tests/` y los ejercicios del tutorial, y compara:

| Fase | Salida comparada |
|---|---|
| Lexer | `--emit-tokens` |
| Parser | `--emit-ast` |
| Semántica | `--emit-typed-ast` (ya disponible en la etapa 0) |
| Generación | `--emit-c` y ejecución de las pruebas existentes |

## Librería estándar disponible para el port

`Array<T>`, `Map<K,V>`, `hash()`, `switch` exhaustivo sobre enums,
`StringBuilder`, `String.fromDouble`, `Console.writeError`, `Process.spawn`,
`File.readAll`/`writeAll` y `Arguments`.
