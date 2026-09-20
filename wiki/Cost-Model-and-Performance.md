# Modelo de costes y rendimiento

TinyC+ no promete “zero cost” como eslogan genérico. Prefiere documentar el
coste mecánico de cada abstracción.

## Tabla mental

| Construcción | Representación | Coste principal |
|---|---|---|
| primitiva/puntero | tipo C | stack/registro |
| clase por valor | struct C | tamaño de campos |
| array fijo | almacenamiento contiguo | por valor |
| slice/string | puntero + longitud | vista, sin reserva |
| tuple | struct | por valor |
| interface | objeto + vtable | llamada indirecta |
| lambda sin captura | function pointer | sin entorno |
| closure prestada | env local + invoke | copia capturas |
| closure owned | env heap + invoke | una reserva |
| Array<T> | data/length/capacity | realloc al crecer |
| stream | loop fusionado | sin arrays intermedios |
| spawn | frame + Future | reserva por tarea |
| async | state frame + Future | reserva por invocación |
| TUI | dos buffers de celdas | O(ancho × alto) |

## Cómo comprobar el coste

Use `--emit-c`:

```bash
tiny --emit-c examples/streams.tc -o build/streams.c
```

Después inspeccione si un pipeline stream se convirtió en un único loop.

Use assembly cuando sea necesario:

```bash
tiny --emit-asm programa.tc --cc clang -o build/programa.s
```

## Benchmarking

El repositorio incluye:

```bash
python tools/benchmark.py --iterations 7
```

Mide, según plataforma:

- tiempo de compilación;
- tiempo de ejecución;
- tamaño del ejecutable;
- memoria máxima cuando el host lo permite.

No compare resultados de máquinas distintas como si fueran una propiedad
intrínseca del lenguaje.

## Preguntas para una optimización

Antes de optimizar, determine:

```text
¿el coste está en frontend?
¿en C generado?
¿en runtime?
¿en alloc/free?
¿en syscalls?
¿en el backend C?
¿en una dependencia externa?
```

La salida C es especialmente útil para evitar “optimizar” la capa equivocada.
