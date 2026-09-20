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
| Parser (`selfhost/parser.tc`) | hecho: mismo árbol, mismos errores y mismo código de salida que `--emit-ast` | `tests/test_selfhost.py` |
| Modelo de tipos (`selfhost/ast.tc`, `selfhost/types.tc`) | hecho: la tabla de tipos que construye el parser, con los mismos ids, es idéntica a la de `--emit-types` | `tests/test_selfhost.py` |
| Carga de módulos y genéricos | pendiente | por añadir a la etapa 0 |
| Semántica | pendiente | `--emit-typed-ast` |
| Generación de C | pendiente | `--emit-c` y las pruebas existentes |

`tests/test_selfhost.py` compila la etapa 1 con la etapa 0 y exige el mismo stdout,
stderr y código de salida en cada fase portada. Compara, con `--emit-tokens`,
`--emit-ast` y `--emit-types`:

- todos los `.tc` del repositorio;
- 64 casos límite del lexer (errores léxicos, literales, BOM, NUL, el límite de tokens);
- más de 180 casos del parser: ambigüedad entre tipos y expresiones, la división
  de `>>` en genéricos, valores de `enum` y longitudes de array, `switch`, límites
  de anidamiento en el borde exacto, la ventana de 128 tokens de las llamadas
  genéricas, la matriz completa de precedencias y los errores de sintaxis;
- todos los prefijos de un programa que usa casi toda la gramática (cada corte es
  un error distinto o un programa más corto);
- entradas aleatorias del lexer y mutaciones de los fuentes reales, con semilla fija
  (`--fuzz N`, `--mutations N`; una pasada de 3.000 y 6.000 no encontró
  diferencias).

Se ejecuta dentro de `tests/run_all.py`. Para probarlo a mano:

```sh
./bin/tiny build selfhost/main.tc -o build/selfhost/tinyc1
./build/selfhost/tinyc1 --emit-ast examples/hello.tc
python3 tests/test_selfhost.py
```

Para saber si el arnés detecta divergencias, se le hicieron mutaciones deliberadas
al port (una precedencia, el límite de anidamiento, un registro de tipos, el valor
de un `enum`, la ventana de 128 tokens…). Las que sobrevivieron mostraron casos que
faltaban, y se añadieron hasta que todas fallaron.

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
- TinyC+ no tiene operador ternario ni `do … while`, y no permite llamar a un
  método sobre un valor temporal (`f().g()`): el port usa `while (true)` con la
  condición al final y variables intermedias.
- El parser de la etapa 0 es sensible al contexto: recuerda qué nombres ha visto
  como tipos (incluido `Function` tras un `func<…>` y el nombre de una llamada
  genérica) y con eso decide si `Foo x;` es una declaración. La etapa 1 reproduce
  esa tabla como un conjunto de nombres, y los duplicados de tipo (`duplicate type`)
  se detectan por el nombre canónico (`int` e `i32` son el mismo tipo).
- Los tipos son los de verdad: se internan en una tabla compartida (`types.tc`) y
  toman ids del mismo contador que los nodos, como en la etapa 0. Eso importa: un
  tipo tupla se llama `tc_tuple_<id>` y uno de array `tc_array_<id>`, así que los
  nombres dependen de cuántos nodos se crearon antes. Un `const T` es una copia
  aparte, `size_t` copia a `u64` sin consumir id y un `func<…>` reutiliza el tipo
  `Function` y lo reetiqueta; todo eso se reproduce y se comprueba.
- `--emit-types` (etapa 0) imprime esa tabla en orden de creación. Existe para poder
  exigir que el modelo de tipos sea idéntico antes de construir el análisis
  semántico encima.
- Detalles que hay que copiar tal cual: `>>` se parte reescribiendo el token a `>`;
  las longitudes de array y los valores de `enum` se leen con la semántica de
  `strtoull` en base 0 (`089` vale 0); y el límite de anidamiento es 256.

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
