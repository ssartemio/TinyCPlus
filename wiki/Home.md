# TinyC+

> **C con abstracciones modernas, memoria manual, lowering visible y compilación nativa rápida.**

TinyC+ es un lenguaje compilado nativo cuyo frontend está escrito en C11. Su
objetivo no es esconder C bajo una máquina virtual, sino conservar su modelo
mental —costes visibles, layouts simples, interoperabilidad y control— añadiendo
herramientas modernas como inferencia local, `defer`, clases ligeras,
interfaces estructurales, genéricos monomorfizados, closures, streams,
concurrencia, async/await, networking, Protobuf, gRPC y una TUI integrada.

El compilador genera **C11 inspeccionable** y usa **libtcc/TinyCC** o un
compilador C externo. No requiere LLVM, VM, GC ni Python para ejecutar programas
compilados.

## Estado

La rama `main` corresponde a **TinyC+ 1.0.0-rc.1**. La matriz de CI ha sido
validada en Windows x64/GCC, Ubuntu x86-64/GCC, Ubuntu ARM64/GCC, macOS
ARM64/Clang y Ubuntu/Clang con ASan/UBSan.

La GUI gráfica se desarrolla como trabajo **post-1.0 experimental** en PRs
separados. La TUI y TinyEdit sí forman parte del alcance 1.0.

## El programa más pequeño

```c
int main()
{
    var x = 10;
    var y = 20;
    println(x + y);
    return 0;
}
```

Ejecutar:

```bash
tiny run examples/hello.tc
```

Salida:

```text
30
```

## El modelo mental

```text
archivo .tc
   ↓
lexer
   ↓
parser / AST
   ↓
módulos + genéricos
   ↓
análisis semántico y tipos
   ↓
lowering
   ↓
C11 legible
   ↓
libtcc / GCC / Clang
   ↓
ejecutable nativo
```

TinyC+ intenta que cada abstracción tenga una respuesta concreta a estas
preguntas:

- ¿qué C se genera?
- ¿reserva memoria?
- ¿quién la libera?
- ¿qué coste tiene?
- ¿puede resolverse en compile-time?
- ¿puede implementarse como biblioteca en vez de complicar el compilador?

## Curso guiado: TinyC+ desde cero

Si prefiere aprender construyendo una aplicación completa, siga
**[TinyC+ desde cero](TinyCPlus-Desde-Cero)**.

Son 13 capítulos acumulativos. El proyecto empieza como un programa de consola,
pasa por memoria manual, clases, colecciones, archivos y concurrencia, añade un
protocolo TCP y termina en un dashboard TUI no bloqueante. El capítulo final
muestra la migración a la GUI gráfica experimental.

## Dónde empezar

Si es su primera visita, continúe con:

**[Primeros pasos](Getting-Started)** → instalación, build y comandos básicos.

Después:

**[Recorrido por el lenguaje](Language-Tour)** → sintaxis y construcciones.

**[Memoria y recursos](Memory-and-Resources)** → la parte más importante del
modelo de TinyC+.

**[Arquitectura del compilador](Compiler-Architecture)** → cómo se implementa.

**[Mapa del repositorio](Project-Structure)** → dónde vive cada responsabilidad.

**[Cookbook de ejemplos](Examples-Cookbook)** → programas representativos.

## Capacidades principales

| Área | Estado en 1.0 RC |
|---|---|
| Frontend C11 + AST + tipos | Implementado |
| Lowering a C11 | Implementado |
| libtcc y compilador externo | Implementado |
| Memoria manual, defer, new/delete | Implementado |
| Strings, slices, arrays, tuples | Implementado |
| Clases, propiedades, extensiones | Implementado |
| Interfaces estructurales | Implementado |
| Genéricos monomorfizados | Implementado |
| Lambdas y closures owned | Implementado |
| Streams fusionados | Implementado |
| FFI C | Implementado |
| Threads, Task, Future, Channel | Implementado |
| async/await | Implementado |
| TCP/UDP/DNS/timers | Implementado |
| Protobuf proto3 | Implementado |
| gRPC unary/h2c | Implementado |
| TUI + TinyEdit | Implementado |
| GUI gráfica | Experimental post-1.0 |

La regla editorial de esta Wiki es simple: **lo marcado como estable debe estar
respaldado por código y pruebas del repositorio**. Las ideas futuras se marcan
como tales.


## Lecturas de referencia

Para evaluar una abstracción en detalle consulte
**[Modelo de costes y rendimiento](Cost-Model-and-Performance)**.

Para fallos frecuentes consulte **[Troubleshooting](Troubleshooting)** y para
supuestos de seguridad **[Seguridad y límites](Security-and-Limits)**.
