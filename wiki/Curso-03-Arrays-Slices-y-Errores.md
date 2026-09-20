# Capítulo 3 — Arrays, slices y errores

**Objetivo:** procesar varias mediciones sin heap y adoptar errores explícitos.

```c
(int, Error) average(Slice<int> values)
{
    if (values.length == 0)
        return (0, 1);

    int total = 0;
    for (value in values)
        total += value;

    return (total / values.length, 0);
}

int main()
{
    int[6] samples = {45, 48, 50, 62, 70, 57};

    var middle = samples[1:5];
    var value, error = average(middle);

    if (error != 0)
        return error;

    println(value);
    return 0;
}
```

Una `Slice<T>` es una **vista** sobre almacenamiento existente; no copia los
elementos.

```text
array  [45][48][50][62][70][57]
            ↑--------------↑
                 slice
```

Los bounds se verifican por defecto.

## Error como parte de la firma

```c
(int, Error) average(...)
```

obliga al caller a ver que la operación puede fallar. TinyC+ no usa exceptions
como mecanismo estándar.

## Ejercicios

1. Escriba `maximum(Slice<int>)`.
2. Cuente muestras críticas.
3. Pruebe una slice vacía.
4. Fuerce un índice fuera de rango.
5. Explique por qué una slice no debe sobrevivir a su storage.

[← Capítulo 2](Curso-02-Funciones-y-Control) ·
[Siguiente → Capítulo 4](Curso-04-Memoria-y-Recursos)
