# Arquitectura del compilador

## Pipeline

```text
.tc
 ↓
lexer
 ↓
parser / AST
 ↓
modules
 ↓
generic instantiation
 ↓
semantic / type analysis
 ↓
C11 generation
 ↓
backend
 ├─ libtcc
 └─ GCC / Clang
```

## Componentes

| Archivo | Responsabilidad |
|---|---|
| compiler/main.c | CLI y ciclo de vida |
| lexer.c | Tokens y ubicación |
| parser.c | AST |
| modules.c | Imports y símbolos de módulo |
| generic.c | Monomorfización |
| semantic.c | Tipos y validación |
| cgen.c | Lowering/generación C |
| backend.c | libtcc o compilador externo |
| tooling.c | fmt/doc/REPL |
| types.c | Internado/representación de tipos |
| util.c | Arena, buffers, diagnósticos |

## Sin IR pesado

TinyC+ no introduce actualmente un IR optimizador tipo SSA.

El AST tipado y las transformaciones de lowering son suficientes para el
objetivo actual.

Esto mantiene pequeño al compilador y hace que la salida C sea la representación
intermedia práctica.

## Representaciones típicas

| TinyC+ | Representación conceptual |
|---|---|
| clase | struct C |
| tuple | struct C |
| slice/string | pointer + length |
| interface | object pointer + vtable |
| lambda sin captura | function pointer |
| closure | env + invoke |
| Array<T> | data + length + capacity |
| async | frame de estado + Future |
| stream | bucle fusionado |

## Runtime selectivo

El C generado incluye runtime según los módulos realmente usados.

Una aplicación simple no incorpora automáticamente nghttp2, gRPC o TUI.

## Backend

El backend intenta usar libtcc cuando es apropiado y puede usar GCC/Clang como
alternativa.

Esto permite:

- compilación rápida con TinyCC;
- comparación diferencial con otros compiladores;
- sanitizers;
- assembly textual;
- integración con toolchains específicos del host.

## Límites defensivos

El compilador impone límites a tamaño de fuente, número de tokens, profundidad,
módulos e instancias genéricas. Son defensas contra inputs patológicos, no una
afirmación de seguridad formal.

## Regla para contribuir al compilador

Antes de modificar TCC o añadir una capa nueva, compruebe si la feature puede
vivir en:

```text
parser/semantic
lowering
runtime pequeño
stdlib
tooling
```

La modificación del backend debe ser la última opción, no la primera.
