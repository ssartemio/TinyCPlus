# Mapa del repositorio

Conocer dónde vive cada responsabilidad ayuda a evitar cambios en la capa
equivocada.

```text
TinyCPlus/
├── compiler/      frontend, tipos, lowering, backend y tooling
├── runtime/       primitivas C incluidas según módulos utilizados
├── std/           biblioteca estándar escrita/expuesta a TinyC+
├── examples/      programas pequeños representativos
├── apps/          aplicaciones completas como TinyEdit
├── tests/         pruebas del lenguaje, runtime e interoperabilidad
├── tools/         bootstrap, protobuf, benchmarks, empaquetado
├── third_party/   TinyCC, nghttp2 y fuentes/licencias empaquetadas
├── docs/          especificación, estado y documentación técnica
├── wiki/          fuente de esta Wiki
├── bin/           ejecutables producidos/distribuidos
└── build/         artefactos temporales de construcción/prueba
```

## compiler/

La implementación del lenguaje.

```text
main.c       CLI
lexer.c      tokens
parser.c     AST
modules.c    imports/resolución
generic.c    monomorfización
semantic.c   tipos y validación
cgen.c       generación C
backend.c    libtcc / GCC / Clang
tooling.c    fmt/doc/REPL
types.c      representación de tipos
util.c       arena/buffers/diagnósticos
```

Si una feature sólo requiere lowering, normalmente termina aquí o en std/runtime;
no en TinyCC.

## runtime/

Código C de soporte.

El generador incluye los runtimes necesarios según los módulos usados. Esto
evita que un hello world arrastre networking, gRPC o TUI.

Ejemplos de responsabilidades:

```text
concurrencia portable
sockets/reactor
strings owned
gap buffer
TUI
gRPC/nghttp2 glue
GUI experimental
```

## std/

API de alto nivel visible al programador TinyC+.

Una práctica útil al diseñar nuevas APIs es empezar aquí y bajar a runtime sólo
cuando realmente se necesita una primitiva de plataforma o rendimiento.

## examples/

Los ejemplos son parte de la documentación ejecutable.

Úselos como primera referencia para sintaxis:

```text
hello.tc
core.tc
collections.tc
streams.tc
async.tc
ffi.tc
grpc_users.tc
tui.tc
```

## apps/

Programas más cercanos a uso real. `tinyedit.tc` es la principal prueba de
integración de UI terminal.

## tests/

Incluye pruebas del compilador, casos positivos/negativos, runtime C,
interoperabilidad Protobuf/gRPC, fuzzing y aplicaciones.

Un cambio que modifica semántica pero no modifica/agrega tests casi siempre está
incompleto.

## third_party/

Las dependencias se empaquetan con fuentes/licencias para bootstrap offline.

No coloque una biblioteca aquí sólo por comodidad: documente por qué debe formar
parte del toolchain/runtime.

## docs/ vs wiki/

`docs/` contiene documentación técnica próxima a la implementación y evidencia
de release.

`wiki/` organiza la misma realidad desde el punto de vista de aprendizaje,
navegación y uso del proyecto.

Ambas deben coincidir en hechos; cuando discrepen, el comportamiento implementado
y las pruebas son la referencia final.
