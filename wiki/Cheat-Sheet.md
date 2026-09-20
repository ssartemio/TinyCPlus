# Cheat sheet

## Programa

```c
int main()
{
    println("TinyC+");
    return 0;
}
```

## Variables

```c
int count = 10;
var inferred = 20;
bool ready = true;
double pi = 3.14159;
```

## Función

```c
int add(int a, int b = 1)
{
    return a + b;
}

var value = add(b: 2, a: 40);
```

## Array / slice

```c
int[5] data = {1, 2, 3, 4, 5};
var middle = data[1:4];

for (i, value in middle)
    println(value);
```

## Resultado múltiple

```c
(int, Error) parse()
{
    return (42, 0);
}

var value, error = parse();
```

## Heap

```c
int* value = new int(42);
defer delete value;
```

## Array<T>

```c
import std.collections;

var values = Array<int>.create();
defer values.destroy();

values.push(42);
```

## Switch

```c
switch (n)
{
    case 0:
        return "vacío";
    case 1, 2, 3:
        return "pocos";
    default:
        return "muchos";
}
```

## Map<K,V>

```c
import std.collections;

var ages = Map<string, int>.create();
defer ages.destroy();

ages.put("Ana", 30);
var age, found = ages.get("Ana");
println(ages.getOr("Marta", -1));
```

## StringBuilder

```c
import std.string;

var text = StringBuilder.create();
defer text.destroy();

text.append("n=");
text.appendInt(-42);
println(text.view());
```

## Lanzar un proceso (sin shell)

```c
import std.collections;
import std.process;

var args = Array<string>.create();
defer args.destroy();
args.push("echo");
args.push("hola");

var status, error = Process.spawn(&args);
```

## Clase

```c
class Box {
    int value;

    Box(int value) {
        this.value = value;
    }

    int get() {
        return value;
    }
}
```

## Interface

```c
interface Reader {
    int read();
}
```

## Lambda

```c
var square = (int x) => x * x;
```

## Closure owned

```c
var f = owned((int x) => x * factor);
defer f.destroy();
```

## Spawn

```c
var task = spawn work(42);
defer task.destroy();
var result = task.get();
```

## Await

```c
async int later()
{
    await Timer.after(10);
    return 42;
}
```

## FFI

```c
extern C {
    int puts(const char* text);
}
```

## CLI

```bash
tiny check file.tc
tiny run file.tc
tiny build file.tc -o app
tiny --emit-c file.tc -o file.c
tiny --emit-ast file.tc
tiny --emit-tokens file.tc
tiny --emit-asm file.tc --cc clang -o file.s
tiny fmt file.tc
tiny doc file.tc -o api.md
tiny test file.tc
tiny repl
```

## Ownership

```text
string / Slice<T>      normalmente vista
OwnedString            propietario
Array<T>               propietario de su buffer
Task/Future/Thread     handle con destroy/join según API
interface              presta el objeto concreto
borrowed Surface       no destruir
```

Regla corta: **si una API devuelve un propietario, guárdelo y programe su
liberación en el mismo bloque siempre que sea posible.**
