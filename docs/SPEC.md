# TinyC+ — Especificación Maestra para Implementación

**Documento:** `TinyCPlus_SPEC_Codex.md`  
**Estado:** Especificación inicial de trabajo  
**Audiencia principal:** Codex / agentes de desarrollo / colaboradores humanos  
**Lenguaje de implementación inicial:** C11 compatible con TinyCC  
**Backend principal:** TinyCC / libtcc  
**Filosofía:** lenguaje moderno, implementación pequeña, lowering explícito a C, memoria manual, compilación rápida y comportamiento predecible.

---

## 0. Propósito de este documento

Este archivo reúne las decisiones de diseño tomadas para **TinyC+** y su relación con un fork mínimo de **TinyCC (TCC+)**.

Debe utilizarse como especificación de referencia para comenzar la implementación.

La prioridad no es implementar todas las características desde el primer momento. La prioridad es construir una base pequeña, verificable y extensible.

Regla principal:

> Si una característica de TinyC+ puede traducirse limpiamente a C sencillo, debe implementarse en el frontend/lowering de TinyC+ y NO dentro del parser o generador de código de TCC.

Otra regla principal:

> TinyC+ debe sentirse moderno por su sintaxis y abstracciones, pero debe conservar un modelo mental cercano a C: memoria manual, coste visible, interoperabilidad directa y ausencia de runtime obligatorio pesado.

---

# 1. Objetivos del proyecto

TinyC+ debe ser:

- pequeño;
- rápido de compilar;
- comprensible;
- portable;
- compatible con C;
- adecuado para aplicaciones generales y de sistemas;
- capaz de usar TCC como backend nativo;
- capaz de interoperar con bibliotecas C existentes;
- apto para concurrencia moderna en una fase posterior;
- apto para networking y gRPC en una fase posterior;
- apto para interfaces de texto y gráficas en una fase posterior;
- capaz de mostrar el C generado para depuración y aprendizaje.

TinyC+ NO pretende ser inicialmente:

- un reemplazo de C++;
- un reemplazo de Rust;
- un lenguaje con garbage collector;
- un lenguaje administrado;
- un lenguaje dinámico;
- un lenguaje con reflection universal;
- un lenguaje con runtime obligatorio grande;
- un compilador con arquitectura tipo LLVM.

---

# 2. Arquitectura general

```text
Aplicaciones TinyC+
        |
        v
Biblioteca estándar TinyC+
        |
        v
Frontend TinyC+
lexer -> parser -> AST -> símbolos -> tipos -> semántica
        |
        v
Lowering
        |
        v
C generado
        |
        v
TCC+ / libtcc
        |
        v
Código nativo
```

Separación lógica:

```text
tinycplus/
    lenguaje moderno

tccplus/
    backend C -> código nativo

runtime/
    funcionalidad que no puede resolverse puramente en lowering

stdlib/
    APIs de uso cotidiano
```

---

# 3. Repositorios

Se recomienda mantener dos repositorios principales.

## 3.1 `tinycplus`

```text
tinycplus/
├── compiler/
│   ├── lexer/
│   ├── parser/
│   ├── ast/
│   ├── symbols/
│   ├── types/
│   ├── semantic/
│   ├── lowering/
│   ├── cgen/
│   └── diagnostics/
│
├── runtime/
│   ├── core/
│   ├── memory/
│   ├── string/
│   ├── collections/
│   ├── thread/
│   ├── task/
│   ├── channel/
│   ├── net/
│   └── tui/
│
├── std/
│   ├── core/
│   ├── memory/
│   ├── string/
│   ├── collections/
│   ├── io/
│   ├── fs/
│   ├── time/
│   ├── math/
│   ├── process/
│   ├── concurrent/
│   ├── net/
│   ├── grpc/
│   └── tui/
│
├── tools/
│   ├── tiny/
│   ├── tinyc/
│   ├── tinyfmt/
│   ├── tinydoc/
│   └── tiny-protoc/
│
├── examples/
├── tests/
├── benchmarks/
└── third_party/
    └── tcc/
```

## 3.2 `tccplus`

Fork conservador de TinyCC.

Debe conservar una rama o referencia limpia de upstream.

Ramas sugeridas:

```text
mob                 espejo de upstream
tccplus/main        integración estable
feature/target-info
feature/asm-output
feature/arm64-asm
feature/debug-api
feature/haiku
```

---

# 4. Política del fork TCC+

La innovación principal debe vivir en TinyC+.

No agregar al núcleo de TCC archivos equivalentes a:

```text
tinyclass.c
tinylambda.c
tinyasync.c
tinygeneric.c
```

Esas capacidades pertenecen al frontend TinyC+.

TCC+ sólo debe modificarse cuando exista una limitación demostrable que no pueda resolverse limpiamente desde el frontend.

Cambios previstos posibles en TCC+:

1. mejoras pequeñas a `libtcc`;
2. API de información del target;
3. mejor diagnóstico;
4. eventual salida assembler `-S`;
5. mejoras ARM64;
6. nuevos targets o correcciones específicas de OS;
7. soporte requerido para integración con TinyC+.

Archivos previsiblemente modificables:

```text
libtcc.h
libtcc.c
tcc.c
tcc.h
tccelf.c

x86_64-gen.c
x86_64-link.c

arm64-gen.c
arm64-link.c
arm64-asm.c

tests/
```

---

# 5. Principios de diseño del lenguaje

## 5.1 Lowering primero

Toda característica nueva debe responder:

```text
¿Puede traducirse a C sencillo?
```

Si sí, debe preferirse lowering.

Si requiere runtime pesado, debe reconsiderarse o moverse a una biblioteca opcional.

## 5.2 Coste visible

No debe existir asignación implícita de memoria en construcciones que aparenten ser triviales, salvo cuando esté explícitamente documentado.

## 5.3 Memoria manual

TinyC+ NO tendrá inicialmente:

- GC;
- ARC;
- autorelease;
- borrow checker;
- ownership inference;
- conteo de referencias automático universal.

Modelos base:

```text
stack
heap manual
```

## 5.4 Interoperabilidad C

El código generado debe usar tipos, convenciones y layouts razonablemente compatibles con C siempre que sea posible.

## 5.5 Runtime pequeño

Una aplicación simple debe poder usar sólo una fracción pequeña del runtime.

## 5.6 Diagnósticos útiles

Los errores de TinyC+ deben referirse al fuente TinyC+, no al C generado.

---

# 6. Compilador TinyC+

El compilador inicial debe escribirse en C11 y ser compilable con TinyCC.

Pipeline:

```text
source.tc
   |
   v
lexer
   |
   v
parser
   |
   v
AST
   |
   v
symbol resolution
   |
   v
type resolution
   |
   v
semantic analysis
   |
   v
lowering
   |
   v
C generation
   |
   v
libtcc
   |
   v
native code
```

No introducir inicialmente un IR complejo.

El AST tipado puede funcionar como representación semántica suficiente.

---

# 7. CLI prevista

Comando principal:

```bash
tiny
```

Subcomandos:

```bash
tiny build
tiny run
tiny check
tiny test
tiny fmt
tiny doc
tiny repl
```

Comando del compilador:

```bash
tinyc programa.tc
```

Opciones de inspección:

```bash
tinyc --emit-c programa.tc
tinyc --emit-ast programa.tc
tinyc --emit-typed-ast programa.tc
tinyc --emit-tokens programa.tc
tinyc --emit-asm programa.tc
```

`--emit-asm` puede inicialmente depender de TCC+ sólo cuando dicha capacidad exista.

---

# 8. Tipos primitivos

Tipos canónicos:

```text
void
bool

i8
u8
i16
u16
i32
u32
i64
u64

float
double

char
```

Aliases compatibles/ergonómicos:

```text
byte
short
int
uint
long
ulong
```

Los aliases deben resolverse internamente a tipos canónicos.

---

# 9. Tipos derivados

```text
T*
T[N]
Slice<T>
Array<T>
string
class
interface
func<...>
Task<T>
Future<T>
Channel<T>
```

Los últimos tipos de concurrencia no forman parte del milestone inicial.

---

# 10. `var`

Inferencia local sencilla.

Ejemplo:

```c
var x = 10;
var pi = 3.14159;
var name = "TinyC+";
```

Restricciones:

- debe existir inicializador;
- el tipo debe inferirse de forma estática;
- no existe tipado dinámico;
- el tipo inferido no cambia.

Lowering:

```c
int x = 10;
double pi = 3.14159;
TinyString name = ...;
```

---

# 11. Funciones

Sintaxis base cercana a C:

```c
int add(int a, int b)
{
    return a + b;
}
```

Debe soportar:

- parámetros;
- retorno;
- `void`;
- funciones estáticas/globales;
- function pointers / funciones como valores en fases posteriores.

Overloading general NO es prioridad inicial.

---

# 12. Named arguments

Característica aceptada.

Ejemplo:

```c
Window.create(
    title: "TinyEdit",
    width: 800,
    height: 600
);
```

Lowering:

```c
Window_create(
    "TinyEdit",
    800,
    600
);
```

La asociación entre label y parámetro debe resolverse en compile time.

No tiene coste runtime.

---

# 13. Múltiples valores de retorno

Ejemplo:

```c
(File*, Error) openFile(string path);
```

Uso:

```c
var file, err = openFile("test.txt");
```

Lowering aproximado:

```c
typedef struct {
    File *v0;
    Error v1;
} __ret_FilePtr_Error;
```

La ABI exacta puede evolucionar, pero debe mantenerse predecible.

---

# 14. Errores como valores

No implementar exceptions en las primeras versiones.

Ejemplo:

```c
var file, err = File.open(path);

if (err)
    return err;
```

`Error` debe ser un tipo pequeño y barato.

`Result<T>` puede existir más adelante como biblioteca genérica.

---

# 15. Memoria

## 15.1 Stack

Objetos locales por valor:

```c
Person p("Ana", 30);
p.show();
```

Lowering conceptual:

```c
Person p;
Person_init(&p, ...);
Person_show(&p);
Person_destroy(&p);
```

La llamada al destructor puede insertarse en salida de scope si el tipo define destructor.

Esto NO equivale a GC.

## 15.2 Heap

Asignación explícita:

```c
Person* p = new Person("Ana", 30);

p.show();

delete p;
```

Lowering conceptual:

```c
Person *p = malloc(sizeof(Person));
Person_init(p, ...);

Person_show(p);

Person_destroy(p);
free(p);
```

## 15.3 Regla general

Si se usa `new`, el programador es responsable de `delete`, salvo que utilice un contenedor explícito que documente otra política.

No implementar ownership automático universal.

---

# 16. `defer`

Característica de alta prioridad.

Ejemplo:

```c
void* buffer = malloc(4096);
defer free(buffer);
```

Ejemplo:

```c
var file, err = File.open(path);
if (err)
    return err;

defer file.close();
```

Semántica:

- se registra cleanup en el scope actual;
- se ejecuta en orden LIFO;
- debe ejecutarse en cada ruta normal de salida del scope;
- debe ejecutarse en `return`;
- debe considerar `break`/`continue` si salen del scope correspondiente.

No existen exceptions inicialmente, por lo que no debe diseñarse alrededor de unwinding de exceptions.

---

# 17. Clases

Las clases son azúcar estructurado sobre structs + funciones.

Ejemplo:

```c
class Person
{
    string name;
    int age;

    Person(string name, int age)
    {
        this.name = name;
        this.age = age;
    }

    void show()
    {
        println(name);
    }
}
```

Lowering aproximado:

```c
typedef struct Person Person;

struct Person {
    TinyString name;
    int age;
};

void Person_init(Person *self, TinyString name, int age);
void Person_show(Person *self);
void Person_destroy(Person *self);
```

Método:

```c
p.show();
```

Lowering:

```c
Person_show(&p);
```

Pointer:

```c
p->show();
```

puede bajar a:

```c
Person_show(p);
```

La sintaxis final debe decidirse de forma consistente.

---

# 18. `this`

Dentro de métodos:

```c
this.name
```

Lowering:

```c
self->name
```

Puede permitirse acceso implícito:

```c
name
```

si no existe ambigüedad.

---

# 19. Constructores y destructores

Constructor:

```c
Person(string name, int age)
{
    ...
}
```

Destructor:

```c
~Person()
{
    ...
}
```

o sintaxis equivalente si se decide otra más adelante.

El destructor:

- no gestiona memoria del objeto por sí mismo;
- limpia recursos internos;
- debe ser llamado antes de `free` en heap;
- puede llamarse automáticamente al terminar scope para valores de stack.

---

# 20. Composición

Preferencia explícita sobre herencia.

Ejemplo:

```c
class Window
{
    Position position;
    Size size;
    Surface surface;
}
```

No implementar inicialmente:

- herencia múltiple;
- virtual inheritance;
- jerarquías complejas;
- RTTI.

---

# 21. Interfaces estructurales

Inspiradas en Go y Objective-C protocols.

Ejemplo:

```c
interface Writer
{
    Error write(Slice<byte> data);
}
```

Un tipo satisface `Writer` si posee la firma requerida.

No necesita:

```c
implements Writer
```

Representación dinámica aproximada:

```c
typedef struct {
    void *object;
    const WriterVTable *vtable;
} Writer;
```

La creación/adaptación de la interfaz debe generarse en compile time.

---

# 22. Composición de interfaces

Debe poder representarse posteriormente:

```c
Drawable & Clickable
```

o una sintaxis equivalente.

No es necesaria para la primera versión.

---

# 23. Properties

Properties sencillas, sin KVO ni binding automático.

Ejemplo:

```c
class Rectangle
{
    float width;
    float height;

    property float area
    {
        get => width * height;
    }
}
```

Uso:

```c
println(rect.area);
```

Lowering:

```c
Rectangle_get_area(&rect);
```

Propiedad almacenada:

```c
property string title;
```

puede generar getter/setter.

No implementar observabilidad automática en core.

---

# 24. Extensions

Inspiradas en Objective-C categories.

Ejemplo:

```c
extension String
{
    bool startsWith(string prefix)
    {
        ...
    }
}
```

No modifica layout del tipo.

Lowering:

```c
bool String_startsWith(TinyString *self, TinyString prefix);
```

Uso:

```c
name.startsWith("A");
```

Debe poder aplicarse a tipos definidos en otros módulos si no existe conflicto.

---

# 25. Strings

Tipo `string` de biblioteca/runtime.

Primera implementación recomendada:

```c
typedef struct {
    char *data;
    size_t length;
} TinyString;
```

La propiedad de memoria debe ser explícita.

Debe distinguirse entre:

- string literal/view;
- string owned;
- buffer mutable.

Se recomienda NO esconder asignaciones costosas en operaciones aparentemente triviales.

Puede existir `StringView`:

```c
typedef struct {
    const char *data;
    size_t length;
} StringView;
```

Para paths, parsing y slicing sin copia.

---

# 26. Slices

Tipo central.

Ejemplo:

```c
Slice<int> values;
```

Representación:

```c
typedef struct {
    int *data;
    size_t length;
} Slice_i32;
```

Slicing:

```c
var part = values[10:20];
```

Debe producir pointer + length, sin copia.

Bounds checking:

- habilitado por defecto en debug;
- configurable;
- posibilidad de eliminarlo en release cuando sea seguro.

---

# 27. Arrays dinámicos

Tipo de biblioteca:

```c
Array<T>
```

Layout conceptual:

```c
data
length
capacity
```

Memoria manual.

Ejemplo:

```c
var numbers = Array<int>.create();
defer numbers.destroy();

numbers.push(10);
numbers.push(20);
```

La implementación puede usar generics monomorfizados.

---

# 28. `range`

Ejemplo:

```c
for (i, value in values)
{
    println(value);
}
```

Lowering:

```c
for (size_t i = 0; i < values.length; ++i)
{
    int value = values.data[i];
    ...
}
```

Sin iterator object obligatorio.

---

# 29. Generics

Implementar mediante monomorfización.

Ejemplo:

```c
Array<int>
Array<Person>
```

Generación:

```text
Array_i32
Array_Person
```

No implementar:

- type erasure universal;
- templates estilo C++;
- metaprogramming arbitrario;
- specialization compleja inicialmente.

El compilador debe cachear instanciaciones.

---

# 30. Funciones como valores

Tipo conceptual:

```c
func<int(int)>
```

Debe poder representar function pointer sin captura.

Sintaxis exacta puede evolucionar.

---

# 31. Lambdas sin captura

Primera fase.

Ejemplo:

```c
var square = (int x) => x * x;
```

Lowering:

```c
static int __lambda_1(int x)
{
    return x * x;
}
```

y function pointer correspondiente.

---

# 32. Closures con captura

Segunda fase.

Ejemplo:

```c
int factor = 10;

var multiply = (int x) => x * factor;
```

Representación:

```c
struct Env {
    int factor;
};

struct Closure_i32_i32 {
    void *env;
    int (*invoke)(void *, int);
};
```

Reglas de memoria:

- captura por valor por defecto;
- closures que NO escapan pueden usar environment de stack;
- closures que escapan requieren asignación explícita;
- no promover automáticamente a heap sin que el programador lo sepa.

Debe diseñarse una API explícita para closures owned/heap cuando llegue esa fase.

---

# 33. Streams

Fase posterior del core.

Ejemplo:

```c
values
    .stream()
    .filter(x => x > 10)
    .map(x => x * 2)
    .forEach(x => println(x));
```

Objetivo:

**stream fusion**

Lowering deseado:

```c
for (...)
{
    if (...)
    {
        int temp = ...;
        println(temp);
    }
}
```

No crear cadena de objetos intermedios si se puede fusionar.

Operaciones candidatas:

```text
filter
map
forEach
reduce
count
first
any
all
collect
```

---

# 34. Módulos

Debe existir sistema de módulos/imports pequeño.

Ejemplo:

```c
module app.main;

import std.fs;
import std.string;
```

Objetivos:

- evitar headers manuales;
- nombres cualificados;
- resolución estática;
- generación de C controlada.

No diseñar un package manager complejo en la primera etapa.

---

# 35. FFI C

Característica fundamental.

TinyC+ debe poder invocar bibliotecas C existentes.

Sintaxis propuesta:

```c
extern C
{
    int puts(const char* text);
    void* malloc(size_t size);
    void free(void* ptr);
}
```

También debe poder mapear:

- structs C;
- enums;
- function pointers;
- calling conventions relevantes;
- `const`;
- arrays/pointers;
- variadic C cuando sea viable.

Objetivo de largo plazo: usar bibliotecas como:

```text
SQLite
OpenSSL
nghttp2
zlib
libpng
```

sin reimplementarlas.

Importación automática de headers puede evaluarse después. No es requisito inicial.

---

# 36. Biblioteca estándar inicial

Fase de estabilización después del core.

Módulos:

```text
std.core
std.memory
std.string
std.collections
std.io
std.fs
std.time
std.math
std.process
```

No debe crecer demasiado antes de estabilizar el lenguaje.

---

# 37. REPL

Usará `libtcc`.

Pipeline:

```text
entrada TinyC+
      |
      v
lowering C
      |
      v
tcc_compile_string()
      |
      v
tcc_relocate()
      |
      v
símbolo / ejecución
```

Ejemplo esperado:

```text
TinyC+ REPL

>>> var x = 10;
>>> x * 20
200
```

No es milestone 0.1.

---

# 38. Concurrencia — principios

La concurrencia NO condiciona TinyC+ 0.1.

Debe añadirse cuando core + stdlib sean estables.

Separar:

```text
Thread
Task<T>
Future<T>
async/await
Channel<T>
```

No tratarlos como sinónimos.

---

# 39. Threads

Modelo inicial: 1:1 con hilos del sistema operativo.

API conceptual:

```c
Thread t = Thread.start(worker, arg);
t.join();
```

Runtime por plataforma:

```text
Linux       pthread
Windows     Win32 threads
Haiku       native threads
otros       backend específico
```

Primitivas:

```text
Thread
Mutex
Semaphore
Condition
Event
Atomic
```

Cancelación preferentemente cooperativa.

---

# 40. `spawn`

Sintaxis de alto nivel posterior:

```c
var task = spawn calculate();
```

`spawn` debe producir un `Task<T>`, no necesariamente un `Thread`.

---

# 41. `Task<T>` y `Future<T>`

Definiciones conceptuales:

```text
Thread
    entidad del scheduler/OS

Task<T>
    unidad lógica de trabajo

Future<T>
    resultado pendiente
```

Runtime puede usar worker pool.

Ejemplo:

```text
1000 Tasks
4 Threads
```

---

# 42. `async` / `await`

No implementar como 1 task = 1 thread en la versión final.

Ejemplo:

```c
async int calculate()
{
    var data = await readData();
    return process(data);
}
```

Lowering recomendado: máquina de estados.

Concepto:

```text
state 0
    iniciar operación
    suspender

evento ready

state 1
    continuar
    completar Task
```

`await`:

- suspende Task;
- NO debe bloquear el thread de worker;
- reanuda cuando Future esté completo.

No requiere GC obligatorio.

Estado async debe tener lifetime gestionado explícitamente por runtime de Task.

---

# 43. Channels

API conceptual:

```c
Channel<int> jobs = Channel<int>.create(128);
defer jobs.destroy();

jobs.send(42);

var value = jobs.receive();
```

Versión async posterior:

```c
await jobs.send(value);
var x = await jobs.receive();
```

Tipos genéricos monomorfizados cuando sea posible.

---

# 44. `select`

Fase avanzada.

Ejemplo conceptual:

```c
select
{
    case msg = await messages.receive():
        process(msg);

    case await Timer.after(1000):
        timeout();
}
```

No es requisito para TinyC+ 1.0 si retrasa estabilidad.

---

# 45. Structured concurrency

Objetivo posterior.

Ejemplo:

```c
taskgroup group;

group.spawn(a);
group.spawn(b);

group.wait();
```

O sintaxis de bloque:

```c
taskgroup
{
    spawn a();
    spawn b();
}
```

Reglas:

- no dejar Tasks hijas huérfanas;
- propagación clara de error;
- cancelación cooperativa.

---

# 46. Cancellation

Tipo:

```c
CancellationToken
```

Debe poder pasarse a operaciones bloqueantes/async.

Ejemplo:

```c
await socket.read(token);
```

No terminar threads arbitrariamente salvo mecanismo de emergencia fuera de API normal.

---

# 47. Networking

Fase posterior a concurrencia base.

Módulo:

```text
std.net
```

Tipos:

```text
Socket
TcpSocket
UdpSocket
Address
Dns
```

Primero APIs síncronas.

Después APIs async.

Ejemplo:

```c
var socket, err = TcpSocket.connect(
    host: "localhost",
    port: 5000
);
```

Posterior:

```c
var socket =
    await TcpSocket.connect(...);
```

---

# 48. HTTP/2

Necesario para gRPC.

No reimplementar inicialmente toda la pila.

Preferir integración FFI con una biblioteca C como `nghttp2` o equivalente.

El objetivo es validar el lenguaje, no construir HTTP/2 desde cero.

---

# 49. Protocol Buffers

Crear herramienta:

```text
tiny-protoc
```

Input:

```protobuf
message UserRequest
{
    int64 id = 1;
}

message UserResponse
{
    string name = 1;
}

service Users
{
    rpc GetUser(UserRequest)
        returns (UserResponse);
}
```

Output TinyC+ aproximado:

```c
class UserRequest
{
    i64 id;
}

class UserResponse
{
    string name;
}

interface UsersService
{
    async UserResponse getUser(UserRequest request);
}
```

La representación final debe respetar el wire format protobuf.

Se recomienda inicialmente apoyarse en biblioteca C existente si simplifica la implementación.

---

# 50. gRPC

Fase posterior a networking + async.

Cliente conceptual:

```c
var client = UsersClient.connect(
    address: "localhost:50051"
);

var response =
    await client.getUser(request);
```

Servidor conceptual:

```c
class UserService
{
    async UserResponse getUser(UserRequest request)
    {
        ...
    }
}

GrpcServer server;

server.addService(new UserService());
server.listen(50051);
```

Objetivo: demostrar integración de:

```text
interfaces
generics
networking
async/await
code generation
FFI C
```

---

# 51. Interfaces de texto — TinyUI/TUI

Antes de GUI gráfica, construir biblioteca estilo ncurses pero con API moderna.

Nombre provisional:

```text
TinyUI
```

o:

```text
std.tui
```

Concepto:

```text
Aplicación
    |
    v
TinyUI
    |
    v
Terminal backend
```

---

# 52. Modelo de terminal

Framebuffer de celdas:

```c
struct Cell
{
    char32 ch;
    Color fg;
    Color bg;
    uint8 attributes;
};
```

Buffers:

```text
frontBuffer
backBuffer
```

`refresh()` compara buffers y actualiza sólo celdas modificadas.

Esto debe funcionar bien sobre:

- terminal ANSI;
- consola local;
- framebuffer de caracteres futuro;
- UART/serial si existe backend.

---

# 53. Widgets TUI

Primera lista:

```text
Window
Panel
Label
Button
TextBox
ListBox
Table
Menu
MenuBar
StatusBar
Dialog
ProgressBar
ScrollBar
```

No implementar todos en la primera versión.

Mínimo:

```text
Window
Label
Button
TextBox
Panel
Row
Column
```

---

# 54. Layout TUI

Evitar coordenadas absolutas como API primaria.

Contenedores:

```text
Row
Column
Stack
```

Ejemplo:

```c
window.content =
    Column {
        Label("Servidor"),

        TextBox(&address),

        Row {
            Button("Connect"),
            Button("Exit")
        }
    };
```

---

# 55. Eventos TUI

Modelo event-driven.

Tipos:

```text
Key
Resize
Timer
Mouse
Custom
```

Loop conceptual:

```c
while (window.running)
{
    var event = ui.nextEvent();
    dispatch(event);
}
```

Lambdas y closures deben facilitar callbacks.

---

# 56. Mouse en TUI

Opcional, pero arquitectura preparada.

Hit testing por coordenadas de celdas.

---

# 57. Aplicaciones TUI objetivo

TinyUI debe permitir construir:

```text
tinyedit
file manager
system monitor
network monitor
gRPC inspector
database client
```

La primera aplicación importante recomendada: `tinyedit`.

---

# 58. `tinyedit`

Editor tipo nano, no port directo de GNU nano.

Arquitectura:

```text
TinyEdit
├── EditorBuffer
├── Cursor
├── Viewport
├── KeyboardHandler
├── FileManager
├── Search
└── TerminalRenderer
```

Buffer recomendado:

```text
GapBuffer
```

Estructura:

```c
struct GapBuffer
{
    char *data;
    size_t gap_start;
    size_t gap_end;
    size_t capacity;
};
```

Features progresivas:

```text
0.1 open/edit/save/exit
0.2 search/goto-line/cut-paste
0.3 syntax highlighting
0.4 compile TinyC+
0.5 project browser/autocomplete
```

---

# 59. GUI gráfica

La GUI gráfica es fase FINAL posterior a:

```text
core estable
stdlib
concurrencia
networking/gRPC
TinyUI/TUI
```

No debe bloquear la evolución inicial del lenguaje.

---

# 60. Arquitectura GUI futura

Reutilizar conceptos de TinyUI cuando sea razonable.

```text
                 TinyUI API
                     |
           +---------+---------+
           |                   |
       TUI backend         GUI backend
           |                   |
       terminal             surfaces
                               |
                           compositor
```

---

# 61. GUI gráfica mínima

Conceptos:

```text
Window
Surface
Canvas
Label
Button
TextBox
Row
Column
Keyboard
Mouse
```

No introducir inicialmente:

```text
GPU compositor
CSS
animations complejas
3D
scene graph grande
```

---

# 62. Surface

Abstracción principal:

```c
class Surface
{
    int width;
    int height;
    int stride;
    Pixel* pixels;
}
```

Las aplicaciones dibujan en `Surface`, no directamente en framebuffer.

---

# 63. Canvas

API de dibujo:

```text
pixel
line
rect
fillRect
text
blit
```

---

# 64. Window server futuro

Preferentemente en userspace si se construye sobre un OS propio.

Aplicaciones:

```text
tinyedit
terminal
file manager
```

hablan por IPC con:

```text
windowd
```

No poner política de ventanas compleja dentro del kernel.

---

# 65. API gráfica deseada

Ejemplo:

```c
class MainWindow
{
    property string server;

    async void connect()
    {
        status.text = "Connecting...";

        var client =
            await GrpcClient.connect(server);

        status.text = "Connected";
    }
}
```

Layout:

```c
Column {
    Label("Server"),

    TextBox(
        value: &server
    ),

    Button(
        text: "Connect",
        onClick: () => connect()
    )
}
```

---

# 66. Reflection

NO implementar reflection completa.

Puede evaluarse después reflection opt-in:

```c
@reflect
class Person
{
    string name;
    int age;
}
```

Usos posibles:

```text
serialization
debugger
GUI inspector
documentation
RPC tooling
```

Pero no es parte de core inicial.

---

# 67. Selectors / dynamic messaging

No copiar el runtime dinámico universal de Objective-C.

Métodos normales deben resolver estáticamente o mediante vtable de interface.

Selectors sólo si aparece caso de uso real.

Posible sintaxis futura:

```c
var method = Button::click;
```

No prioridad.

---

# 68. Null safety ligera

No adoptar `nil messaging` de Objective-C.

Se puede considerar operador seguro:

```c
window?.close();
```

Lowering:

```c
if (window != NULL)
    Window_close(window);
```

No prioridad del milestone inicial.

---

# 69. Características explícitamente fuera del alcance inicial

No implementar inicialmente:

```text
garbage collector
ARC universal
exceptions
multiple inheritance
virtual inheritance
RTTI complejo
reflection completa
Objective-C style message forwarding
KVC/KVO
operator overloading general
C++ templates
compile-time metaprogramming complejo
borrow checker
automatic ownership
fibers + coroutines + async como sistemas separados
JIT optimizer
LLVM-style IR
```

---

# 70. Documentación integrada

Doc comments:

```c
/// Lee un archivo.
///
/// @param path Ruta.
/// @return archivo y error.
(File*, Error) openFile(string path);
```

Herramienta:

```bash
tiny doc
```

Anotaciones futuras:

```text
@unsafe
@blocking
@noalloc
@deprecated
@since("0.3")
@test
```

No todas deben implementarse de inmediato.

---

# 71. Formatter

Herramienta:

```bash
tiny fmt
```

Debe existir relativamente temprano para evitar fragmentación de estilo.

Inicialmente puede operar sobre AST.

---

# 72. Tests integrados

Anotación futura:

```c
@test
void test_add()
{
    assert(add(2, 3) == 5);
}
```

Ejecutar:

```bash
tiny test
```

---

# 73. Testing — niveles

## T0 — TCC upstream

Cada cambio de TCC+ debe pasar tests upstream.

## T1 — Lexer

Entrada -> tokens esperados.

## T2 — Parser

Entrada -> AST esperado.

## T3 — Tipos

Casos válidos y errores esperados.

## T4 — Lowering

```text
input.tc
expected.c
```

Comparar C generado.

## T5 — Integración

TinyC+ -> TCC -> ejecución.

## T6 — Diferencial

Compilar C generado con:

```text
TCC
GCC
Clang
```

Los tres deben producir mismo resultado cuando el programa no depende de comportamiento indefinido.

---

# 74. Estructura de pruebas

```text
tests/
├── lexer/
├── parser/
├── types/
├── var/
├── defer/
├── memory/
├── class/
├── property/
├── extension/
├── interface/
├── generic/
├── lambda/
├── closure/
├── slice/
├── array/
├── range/
├── stream/
├── ffi/
├── thread/
├── task/
├── async/
├── channel/
├── net/
├── grpc/
└── tui/
```

Dentro de cada módulo:

```text
compile-pass/
compile-fail/
run-pass/
lowering/
```

---

# 75. Sanitizers

Durante desarrollo, además de TCC, compilar el compilador con:

```text
GCC
Clang
```

y usar:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

cuando sea posible.

---

# 76. Fuzzing

Aplicar a:

```text
lexer
parser
type checker
```

El compilador jamás debe:

```text
crash
hang
corromper memoria
```

ante input inválido.

---

# 77. Benchmarks

Medir:

```text
tiempo de compilación
RAM del compilador
tamaño del ejecutable
runtime
allocations
```

Toda abstracción de alto nivel debe tener coste conocido.

Streams, lambdas, interfaces y async deben tener benchmarks dedicados.

---

# 78. CI

Matriz inicial deseada:

```text
Linux x86-64
Linux ARM64
Windows x86-64
macOS ARM64
```

Posterior:

```text
Haiku x86-64
```

Compiladores del frontend:

```text
TCC
GCC
Clang
```

cuando sea viable.

---

# 79. Target information en TCC+

Posible API:

```c
typedef struct
{
    const char *arch;
    const char *os;

    int pointer_size;
    int long_size;

    int little_endian;
} TCCTargetInfo;

int tcc_get_target_info(
    TCCState *s,
    TCCTargetInfo *info
);
```

No es requisito 0.1.

---

# 80. Salida assembler `-S`

Fase avanzada de TCC+.

TCC actualmente genera bytes de máquina directamente.

Diseño propuesto:

```text
                   +-> BinaryEmitter
MachineInst -------|
                   +-> AsmEmitter
```

No construir un IR tipo LLVM.

Machine IR mínimo:

```text
MOV
LOAD
STORE
ADD
SUB
MUL
CMP
CALL
JUMP
RET
...
```

Primero x86-64.

Después ARM64.

El backend binario actual debe seguir siendo el camino principal y rápido.

---

# 81. ARM64

ARM64 es target estratégico.

Mejoras posibles:

```text
inline asm
assembler integrado
ABI
relocations
code generation tests
```

No mezclar estas tareas con TinyC+ core.

---

# 82. Roadmap de versiones

## TinyC+ 0.0 — Bootstrap

Entregables:

```text
repositorio
build
lexer
parser mínimo
AST
C generator
libtcc integration
hello world
tests base
```

Programa:

```c
int main()
{
    println("Hola TinyC+");
    return 0;
}
```

---

## TinyC+ 0.1 — Core language

Implementar:

```text
tipos
variables
var
expresiones
funciones
control flow
pointers
manual memory
new/delete
defer
arrays estáticos
slices
strings mínimos
multiple returns
errors as values
```

Objetivo:

lenguaje usable sin OOP.

---

## TinyC+ 0.2 — Object model

Implementar:

```text
classes
fields
methods
this
constructors
destructors
properties
extensions
interfaces estructurales
composition
named arguments
```

No herencia clásica.

---

## TinyC+ 0.3 — Generics + collections + FFI

Implementar:

```text
monomorphized generics
Array<T>
func values
lambdas no-capture
FFI C
stdlib inicial
```

Objetivo:

programas de utilidad reales.

---

## TinyC+ 0.4 — Closures + streams + tooling

Implementar:

```text
closures
range
streams
stream fusion
tiny fmt
tiny doc
tiny repl
diagnostics maduros
```

No async todavía.

---

## TinyC+ 0.5 — Threads

Implementar:

```text
Thread
Mutex
Semaphore
Condition
Event
Atomic
```

Cross-platform runtime.

---

## TinyC+ 0.6 — Tasks + Channels

Implementar:

```text
Task<T>
Future<T>
spawn
worker pool
Channel<T>
CancellationToken
```

---

## TinyC+ 0.7 — Async/Await

Implementar:

```text
async
await
state machine lowering
async filesystem/network adapters
structured concurrency inicial
```

Criterio importante:

`await` no debe bloquear worker thread.

---

## TinyC+ 0.8 — Networking

Implementar:

```text
std.net
TCP
UDP
DNS
sync + async sockets
TLS vía FFI si procede
HTTP/2 integration
```

---

## TinyC+ 0.9 — Protobuf + gRPC

Implementar:

```text
tiny-protoc
protobuf mappings
grpc client
grpc server
streaming RPC posteriormente
```

---

## TinyC+ 0.10 — TinyUI/TUI

Implementar:

```text
terminal backend
cell buffer
diff refresh
Window
Panel
Label
Button
TextBox
Row
Column
events
tinyedit básico
```

---

## TinyC+ 1.0 — Estabilidad

Requisitos:

```text
lenguaje core estable
stdlib coherente
FFI estable
concurrencia estable
networking funcional
gRPC funcional
TinyUI usable
tooling consistente
tests extensivos
documentación
```

La GUI gráfica puede ser 1.x.

---

## TinyC+ 1.x — GUI gráfica

Implementar progresivamente:

```text
Surface
Canvas
Window
input
widgets
layouts
event loop
GUI backend de TinyUI
```

---

# 83. Milestones técnicos

## Milestone A

```c
int main()
{
    var x = 10;
    var y = 20;

    println(x + y);
}
```

Debe producir:

```text
30
```

Pipeline completo:

```text
TinyC+
 -> AST
 -> C
 -> libtcc
 -> native
```

---

## Milestone B

```c
class Person
{
    string name;
    int age;

    void show()
    {
        println(name);
    }
}

int main()
{
    Person p("Ana", 30);

    p.show();

    return 0;
}
```

Debe generar C legible.

---

## Milestone C

```c
var values = Array<int>.create();
defer values.destroy();

values.push(1);
values.push(2);
values.push(3);

for (i, value in values)
{
    println(value);
}
```

---

## Milestone D

```c
values
    .stream()
    .filter(x => x > 10)
    .map(x => x * 2)
    .forEach(x => println(x));
```

Debe bajar a loop fusionado.

---

## Milestone E

```c
var t = Thread.start(worker);
t.join();
```

---

## Milestone F

```c
async Response request()
{
    var data = await socket.read();
    return parse(data);
}
```

---

## Milestone G

```c
var client =
    UsersClient.connect(
        address: "localhost:50051"
    );

var response =
    await client.getUser(request);
```

---

## Milestone H

TinyUI:

```c
window.content =
    Column {
        Label("Server"),

        TextBox(&address),

        Button(
            text: "Connect",
            onClick: () => connect()
        )
    };
```

---

# 84. Primer paquete de trabajo para Codex

Codex NO debe comenzar implementando clases, async o GUI.

Debe comenzar con un bootstrap mínimo.

## Sprint 0 — repositorio

Crear estructura:

```text
compiler/
runtime/
std/
tests/
examples/
tools/
```

Build inicial.

## Sprint 1 — Lexer

Tokens mínimos:

```text
identifiers
integer literals
float literals
string literals

keywords:
int
void
return
if
else
while
for
var
true
false

operators:
+
-
*
/
%
=
==
!=
<
>
<=
>=
&&
||
!
&
|
^

punctuation:
(
)
{
}
[
]
;
,
.
:
```

Tests completos.

## Sprint 2 — AST + Parser

AST mínimo:

```text
Program
FunctionDecl
VarDecl
Block
ReturnStmt
IfStmt
WhileStmt
BinaryExpr
UnaryExpr
CallExpr
IdentifierExpr
LiteralExpr
```

Parser:

- recursive descent;
- Pratt parser para expresiones.

## Sprint 3 — símbolos y tipos

Implementar:

```text
scopes
symbol table
primitive types
function signatures
local variables
var inference
type checking
```

## Sprint 4 — C generator

Generar C válido y legible.

Mantener `#line` para diagnósticos posteriores si resulta práctico.

## Sprint 5 — libtcc

Integrar:

```text
tcc_new
tcc_set_output_type
tcc_compile_string
tcc_relocate / run
```

Crear:

```bash
tiny run examples/hello.tc
```

## Sprint 6 — runtime mínimo

Implementar:

```text
println(int)
println(string)
basic string literal wrapper
assert
```

## Sprint 7 — first milestone

Este programa debe pasar:

```c
int main()
{
    var x = 10;
    var y = 20;

    println(x + y);

    return 0;
}
```

---

# 85. Reglas para Codex durante implementación

Codex debe seguir estas reglas:

1. No implementar características futuras sin necesidad.
2. Mantener cada cambio pequeño y testeable.
3. No modificar TCC+ si la solución puede vivir en TinyC+.
4. No introducir GC.
5. No introducir ARC.
6. No introducir herencia clásica.
7. No introducir exceptions.
8. No introducir un IR complejo.
9. Generar C legible.
10. Agregar tests por cada feature.
11. Mantener compilación con TCC cuando sea posible.
12. Mantener también compatibilidad con GCC/Clang durante desarrollo.
13. Evitar dependencias externas en el compilador core.
14. Documentar toda desviación de esta especificación.
15. Antes de agregar una abstracción, mostrar el C esperado del lowering.

---

# 86. Criterio para aceptar una feature

Antes de integrar cualquier característica nueva responder:

```text
1. ¿Qué problema resuelve?
2. ¿Cuál es su sintaxis mínima?
3. ¿Cuál es su AST?
4. ¿Cuál es su tipo semántico?
5. ¿Cómo baja a C?
6. ¿Asigna memoria?
7. ¿Quién libera esa memoria?
8. ¿Qué coste runtime introduce?
9. ¿Puede implementarse como biblioteca?
10. ¿Qué tests demuestran que funciona?
```

Si no hay respuestas claras, la feature no entra todavía.

---

# 87. Criterio de estabilidad

Una versión se considera estable si:

```text
tests unitarios pasan
tests de lowering pasan
tests de integración pasan
no hay leaks conocidos del compilador
no hay crashes con input inválido común
C generado compila con TCC
casos principales también compilan con GCC/Clang
documentación actualizada
```

---

# 88. Filosofía final

TinyC+ toma ideas útiles de varios lenguajes sin copiar su complejidad completa:

```text
C
    modelo mental, control, interoperabilidad

Go
    defer, slices, interfaces estructurales,
    channels, concurrencia simple

Objective-C
    protocols, categories/extensions,
    properties, legibilidad de APIs

C#
    properties, async/await como experiencia de uso

C++
    objetos por valor, generics monomorfizados,
    pero sin templates complejos

Rust
    atención al coste y abstracciones eficientes,
    pero sin borrow checker
```

La personalidad buscada es:

> **C con abstracciones modernas, memoria manual, objetos ligeros, interfaces estructurales, generics monomorfizados, lambdas, tooling simple y concurrencia moderna opcional, compilado muy rápidamente mediante TCC.**

---

# 89. Regla de oro para el proyecto

Cuando exista duda entre una implementación sofisticada y una implementación sencilla que cubra el 80–90 % del caso de uso, elegir inicialmente la sencilla.

El proyecto debe poder evolucionar sin dejar de ser comprensible.

---

# 90. Próximo objetivo inmediato

El agente debe comenzar por:

```text
TinyC+ bootstrap
```

y NO por TCC+ profundo.

Orden inmediato:

```text
repo
build
lexer
AST
parser
symbols
types
var
functions
C generator
libtcc
runtime println
tests
```

Criterio de salida:

```bash
tiny run examples/hello.tc
```

con:

```c
int main()
{
    var x = 10;
    var y = 20;

    println(x + y);

    return 0;
}
```

salida:

```text
30
```

Una vez conseguido, congelar el milestone y continuar con:

```text
manual memory
defer
slices
strings
multiple returns
errors as values
```

antes de introducir clases.

---

**Fin de la especificación inicial.**
