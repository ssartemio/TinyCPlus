# Capítulo 6 — Colecciones, closures y streams

**Objetivo:** convertir el conjunto fijo de mediciones en datos dinámicos y
analizarlos con operaciones de alto nivel cuyo coste siga siendo visible.

## Array<T>

```c
import std.collections;

int main()
{
    var scores = Array<int>.create();
    defer scores.destroy();

    scores.push(42);
    scores.push(61);
    scores.push(95);

    for (score in scores)
        println(score);

    return 0;
}
```

`Array<T>` posee su buffer. `destroy()` libera ese buffer; no destruye
automáticamente recursos propietarios contenidos en cada elemento.

## Lambdas

```c
var doubled = (int value) => value * 2;
println(doubled(21));
```

Sin captura, una lambda puede bajar esencialmente a una función C.

## Captura

```c
int threshold = 60;
var isWarning = (int value) => value >= threshold;
```

Las capturas prestadas deben respetar la vida del scope. Si una closure debe
escapar, use ownership explícito.

```c
closure<int(int)> multiplier(int factor)
{
    return owned((int value) => value * factor);
}
```

## Streams

```c
import std.collections;

int main()
{
    int[6] scores = {42, 61, 95, 55, 88, 73};

    scores.stream()
          .filter(x => x >= 60)
          .map(x => x * 2)
          .forEach(x => println(x));

    var total = scores.stream()
                      .filter(x => x >= 60)
                      .reduce(0, (sum, x) => sum + x);

    println(total);
    return 0;
}
```

La idea clave es **fusionar** la cadena en un bucle en lugar de crear una
colección intermedia por cada operación.

## TinyStatus: resumen de muestras

```c
import std.collections;

class History {
    Array<int> scores;

    static History create()
    {
        History history;
        history.scores = Array<int>.create();
        return history;
    }

    void add(int value)
    {
        scores.push(value);
    }

    int warnings()
    {
        return scores.stream().filter(x => x >= 60).count();
    }

    void destroy()
    {
        scores.destroy();
    }
}
```

Uso:

```c
var history = History.create();
defer history.destroy();

history.add(42);
history.add(61);
history.add(95);

println(history.warnings());
```

## Ejercicios

1. Calcule el máximo mediante `reduce`.
2. Use `first()` para encontrar la primera alarma.
3. Use `collect` para crear un Array con sólo valores críticos.
4. Genere el C y localice el loop fusionado.
5. Convierta una lambda que captura en closure `owned` y libérela.

[← Capítulo 5](Curso-05-Objetos-y-Modelos) ·
[Siguiente → Capítulo 7](Curso-07-Modulos-y-FFI)


## Solución ejecutable

[`examples/tutorial/06_collections_streams.tc`](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/06_collections_streams.tc)

La CI ejecuta esta solución como parte de `tests/test_tutorial.py`.
