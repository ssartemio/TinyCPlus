# Capítulo 5 — Objetos y modelos

**Objetivo:** representar TinyStatus con clases ligeras y interfaces
estructurales.

```c
class Measurement {
    int temperature;
    int load;

    Measurement(int temperature, int load)
    {
        this.temperature = temperature;
        this.load = load;
    }

    int score()
    {
        return temperature + load;
    }

    int severity()
    {
        var value = score();
        if (value < 60) return 0;
        if (value < 90) return 1;
        return 2;
    }
}
```

Uso:

```c
Measurement sample(24, 37);
println(sample.score());
```

Mentalmente:

```text
class ≈ struct C + funciones + sintaxis
```

## Interface estructural

```c
interface StatusSource {
    int current();
}

class LocalSource {
    int value;

    LocalSource(int value) { this.value = value; }

    int current() { return value; }
}

int main()
{
    LocalSource source(42);
    StatusSource reader = &source;
    println(reader.current());
    return 0;
}
```

No hay `implements` obligatorio. La compatibilidad depende de la firma.

Conceptualmente una interface contiene object pointer + vtable.

## Genérico mínimo

```c
class Sample<T> {
    T value;
    Sample(T value) { this.value = value; }
}
```

Se monomorfiza por tipos utilizados.

## Ejercicios

1. Añada una propiedad calculada `healthy`.
2. Cree una interface `Scorable`.
3. Haga que Measurement la satisfaga estructuralmente.
4. Inspeccione el C de clase e interface.
5. Explique por qué composición es preferible a añadir herencia aquí.

[← Capítulo 4](Curso-04-Memoria-y-Recursos) ·
[Siguiente → Capítulo 6](Curso-06-Colecciones-Closures-y-Streams)


## Solución ejecutable

[`examples/tutorial/05_objects_models.tc`](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/05_objects_models.tc)

La CI ejecuta esta solución como parte de `tests/test_tutorial.py`.
