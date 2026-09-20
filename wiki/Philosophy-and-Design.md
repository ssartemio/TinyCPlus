# Filosofía y diseño

TinyC+ parte de una preferencia deliberada: **la modernidad del lenguaje no debe
implicar opacidad del coste**.

## Regla cardinal

> Si una característica puede bajar limpiamente a C sencillo, debe resolverse en
> el frontend/lowering o en una biblioteca pequeña antes de modificar el backend.

Esta regla explica buena parte del proyecto.

Una clase baja a un `struct` y funciones. Una interfaz es un puntero más una
vtable. Un tuple es un `struct`. Un stream intenta fusionarse en un solo bucle.
Una closure owned hace visible que existe un entorno reservado. Una función
async baja a un frame/estado en lugar de requerir una VM.

## Lo que TinyC+ prioriza

**Simplicidad mecánica.** Debe ser posible relacionar una construcción TinyC+
con el C que produce.

**Memoria manual.** No hay GC ni ARC universal. Los recursos propietarios deben
tener una política de liberación comprensible.

**Interoperabilidad.** C es la frontera natural del lenguaje.

**Compilación rápida.** TinyCC/libtcc es un backend central, aunque GCC/Clang
también son útiles para validación, assembly y sanitizers.

**Abstracciones con coste conocido.** No se acepta una feature únicamente porque
su sintaxis sea agradable.

**Pruebas por capas.** Lexer, parser, semántica, lowering, runtime y
compatibilidad con otros compiladores se verifican separadamente.

## Lo que TinyC+ evita

TinyC+ 1.0 no intenta incorporar:

- garbage collector;
- ARC universal;
- exceptions;
- multiple inheritance;
- RTTI complejo;
- C++ templates;
- metaprogramación general;
- borrow checker;
- runtime dinámico estilo Objective-C;
- un IR/optimizador comparable a LLVM.

Esto no significa que esas ideas sean inútiles. Significa que no son necesarias
para el problema que TinyC+ quiere resolver.

## Influencias

De **C** toma el control, el FFI y la predictibilidad.

De **Go** toma ideas como defer, slices, interfaces estructurales y channels.

De **Objective-C** toma protocolos/interfaces, extensions/categories y una
preferencia por APIs legibles.

De **C#** toma propiedades y la experiencia de uso de async/await.

De **C++** toma objetos por valor y monomorfización, evitando gran parte de la
maquinaria de templates.

De **Rust** toma la disciplina de pensar en coste y ownership, pero no intenta
replicar el borrow checker.

## Diseño por evidencia

Antes de integrar una feature conviene responder:

```text
¿qué problema resuelve?
¿qué sintaxis mínima necesita?
¿cómo aparece en el AST?
¿cómo se tipa?
¿cómo baja a C?
¿reserva memoria?
¿quién libera esa memoria?
¿puede ser biblioteca?
¿qué pruebas demuestran su semántica?
```

Esta lista no es burocracia: evita que el lenguaje crezca por acumulación de
azúcar sintáctico sin un modelo coherente.
