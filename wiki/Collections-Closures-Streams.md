# Colecciones, closures y streams

## Arrays dinámicos

`Array<T>` es la colección dinámica base.

```c
import std.collections;

int main()
{
    var values = Array<int>.create();
    defer values.destroy();

    values.push(10);
    values.push(20);
    values.push(30);

    for (value in values)
        println(value);

    return 0;
}
```

La memoria del buffer es propia del Array; los recursos de los elementos no se
destruyen recursivamente por arte de magia.

## Mapas hash

`Map<K,V>` es una tabla hash propietaria con direccionamiento abierto. Las
claves pueden ser enteros, `char`, `bool`, enums, punteros o strings, y se
comparan con `==`.

```c
import std.collections;

int main()
{
    var ages = Map<string, int>.create();
    defer ages.destroy();

    ages.put("Ana", 30);
    ages.put("Luis", 41);

    var age, found = ages.get("Ana");
    if (found)
        println(age);

    println(ages.getOr("Marta", -1));
    ages.remove("Luis");
    println(ages.length);
    return 0;
}
```

Ofrece `put`, `get`, `getOr`, `contains`, `remove`, `keys`, `values`, `clear` y
`destroy`. Las claves string son **vistas**: el mapa no copia el texto, que debe
vivir más que la entrada. El orden de `keys()` y `values()` no está especificado.
Como en `Array<T>`, las copias comparten el almacenamiento y hay que llamar a
`destroy` una sola vez.

`hash(valor)` es un builtin que devuelve un `u64` para enteros, `char`, `bool`,
enums, punteros y strings (por contenido).

## Lambdas sin captura

```c
var square = (int x) => x * x;
println(square(7));
```

Una lambda sin captura puede bajar esencialmente a una función C y un puntero.

## Closures

Una closure prestada puede usar un entorno local mientras no escape.

Para devolver una closure con entorno:

```c
closure<int(int)> multiplier(int n)
{
    return owned((int value) => value * n);
}

int main()
{
    var timesSix = multiplier(6);
    defer timesSix.destroy();

    println(timesSix(7));
    return 0;
}
```

El heap es explícito a través de `owned(...)`.

## Streams

Ejemplo representativo:

```c
import std.collections;

int main()
{
    int[6] values = {1, 4, 8, 12, 20, 30};

    values.stream()
          .filter(x => x > 10)
          .map(x => x * 2)
          .forEach(x => println(x));

    var sum = values.stream()
                    .filter(x => x > 10)
                    .reduce(0, (a, x) => a + x);

    assert(sum == 62);
    return 0;
}
```

La intención del lowering es **stream fusion**:

```text
stream → filter → map → reduce
           ↓
      un bucle C
```

No se construye necesariamente una colección intermedia por cada operación.

Terminales disponibles incluyen:

```text
forEach
count
first
any
all
reduce
collect
```

`first()` devuelve `(T, bool)`.

`collect(&array)` agrega a un `Array<T>` existente; la propiedad del array
sigue siendo explícita.

Una cadena stream debe terminar en una operación terminal dentro de la misma
expresión; no se almacena como pipeline runtime genérico.
