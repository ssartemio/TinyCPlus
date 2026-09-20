# Capítulo 2 — Funciones y control

**Objetivo:** sacar lógica de `main` y usar defaults/named arguments.

```c
int healthScore(int temperature, int load, int factor = 1)
{
    return temperature + load * factor;
}

int classify(int score)
{
    if (score < 60) return 0;
    if (score < 90) return 1;
    return 2;
}

int main()
{
    var score = healthScore(
        load: 37,
        temperature: 24,
        factor: 1
    );

    println(score);
    println(classify(score));
    return 0;
}
```

Los argumentos nombrados se evalúan en el orden escrito y después se reordenan
para la llamada C. No implican diccionarios ni reflexión runtime.

## Loops

```c
for (int sample = 0; sample < 10; sample = sample + 1) {
    if (sample == 2)
        continue;
    if (sample == 8)
        break;
    println(sample);
}
```

## Ejercicios

1. Añada un cuarto nivel de severidad.
2. Implemente `clampScore`.
3. Llame a `healthScore` con named args en otro orden.
4. Inspeccione el C generado y confirme que los labels desaparecen.

[← Capítulo 1](Curso-01-Primer-Programa) ·
[Siguiente → Capítulo 3](Curso-03-Arrays-Slices-y-Errores)
