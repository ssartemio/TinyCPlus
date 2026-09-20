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
