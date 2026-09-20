# Recorrido por el lenguaje

Esta página resume la superficie del lenguaje estable. Para reglas detalladas,
consulte `docs/LANGUAGE.md`.

## Tipos

Tipos enteros explícitos:

```text
i8 u8
i16 u16
i32 u32
i64 u64
```

También existen:

```text
bool
float
double
char
string
void
T*
T[N]
Slice<T>
```

Aliases comunes: `int` y `Error` → i32, `uint` → u32, `byte` → u8,
`long` → i64 y `size_t` → u64.

## Inferencia local

```c
var answer = 42;
var name = "TinyC+";
```

`var` exige inicializador y el tipo sigue siendo estático.

## Funciones y argumentos nombrados

```c
int scale(int value, int factor = 2)
{
    return value * factor;
}

int main()
{
    var result = scale(factor: 3, value: 14);
    println(result);
    return 0;
}
```

Los argumentos nombrados se evalúan en el orden escrito y se reordenan antes de
la ABI.

## Control de flujo

```c
for (int i = 0; i < 10; i = i + 1) {
    if (i == 3)
        continue;
    if (i == 8)
        break;
    println(i);
}
```

También existe range:

```c
int[3] values = {10, 20, 30};

for (index, value in values) {
    println(index);
    println(value);
}
```

## Switch

`switch` compara un entero, `char`, `bool`, enum o string contra etiquetas
constantes. Los casos **no caen** al siguiente: no hace falta (ni se permite) un
`break` directo.

```c
enum Kind { Fn, Var, Type }

string describe(Kind kind)
{
    switch (kind)
    {
        case Kind.Fn:
            return "función";
        case Kind.Var, Kind.Type:
            return "dato";
    }
}

string size(int n)
{
    switch (n)
    {
        case 0:
            return "vacío";
        case 1, 2, 3:
            return "pocos";
        default:
            return "muchos";
    }
}
```

Sobre un enum sin `default` hay que cubrir todos los valores; si falta uno, el
compilador dice cuál. `continue` dentro de un caso actúa sobre el bucle que lo
contiene.

## Slices

```c
int[5] numbers = {10, 20, 30, 40, 50};
var middle = numbers[1:4];

for (value in middle)
    println(value);
```

La slice es una vista. No copia elementos.

## Resultados múltiples

```c
(int, Error) divide(int a, int b)
{
    if (b == 0)
        return (0, 1);
    return (a / b, 0);
}

int main()
{
    var value, error = divide(84, 2);
    if (error != 0)
        return error;

    println(value);
    return 0;
}
```

No hay exceptions. `Error == 0` significa éxito por convención de la stdlib.

## Cast y sizeof

```c
void* raw = null;
int* value = cast<int*>(raw);
println(sizeof(int));
```

## Const

`const` evita la mutación directa del valor. No constituye un sistema
transitivo de inmutabilidad sobre todos los recursos apuntados.

## Errores que conviene evitar

El lenguaje conserva varias propiedades de C:

- signed overflow sigue siendo peligroso;
- shifts inválidos no se convierten mágicamente en operaciones checked;
- un puntero puede quedar dangling;
- una slice puede sobrevivir incorrectamente al storage del que depende;
- copiar un objeto propietario puede copiar únicamente su puntero.

Esa elección hace que **[Memoria y recursos](Memory-and-Resources)** sea lectura
obligatoria.
