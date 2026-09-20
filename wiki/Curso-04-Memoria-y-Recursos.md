# Capítulo 4 — Memoria y recursos

**Objetivo:** distinguir propietario de vista y hacer visible la liberación.

## Heap explícito

```c
int main()
{
    int* value = new int(42);
    defer delete value;

    println(*value);
    return 0;
}
```

No hay GC ni ARC universal. `delete` es una sentencia, por lo que no se escribe
`defer delete value`. Para objetos `new`, use `delete` explícito o encapsule
la propiedad en un tipo con una operación de cleanup expresable como método.

## OwnedString

```c
import std.string;

int main()
{
    var number = String.fromInt(42);
    defer number.destroy();

    var message = String.concat("score=", number.view());
    defer message.destroy();

    println(message.view());
    return 0;
}
```

`OwnedString` posee bytes. `.view()` presta esos bytes.

Después de `destroy()`, la vista deja de ser válida.

## Regla de propietario único

Copiar un objeto propietario puede copiar sólo sus punteros. No asuma deep copy.

Patrón recomendado:

```c
var resource = createResource();
defer resource.destroy();
```

Adquiera el recurso y programe su cleanup cerca.

## Defer

Los defer normales se ejecutan LIFO al salir por `return`, `break` o
`continue`. La expresión observa los valores al momento de salir.

## Ejercicios

1. Cree tres OwnedString y libérelos con defer.
2. Añada un return temprano.
3. Inspeccione el lowering de defer.
4. Explique qué pasaría si dos copias liberan el mismo buffer.
5. Compile un caso con ASan usando Clang/GCC.

[← Capítulo 3](Curso-03-Arrays-Slices-y-Errores) ·
[Siguiente → Capítulo 5](Curso-05-Objetos-y-Modelos)


## Solución ejecutable

[`examples/tutorial/04_memory_resources.tc`](https://github.com/ssartemio/TinyCPlus/blob/main/examples/tutorial/04_memory_resources.tc)

La CI ejecuta esta solución como parte de `tests/test_tutorial.py`.
