# Memoria y recursos

La memoria manual no es una opción secundaria en TinyC+: es parte del contrato
del lenguaje.

## Stack y heap

Un objeto local vive por valor:

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

int main()
{
    Box box(42);
    println(box.get());
    return 0;
}
```

Una reserva explícita usa `new` y `delete`:

```c
int main()
{
    int* value = new int(42);
    println(*value);
    delete value;
    return 0;
}
```

No hay GC detrás.

## Defer

```c
var values = Array<int>.create();
defer values.destroy();
```

Los `defer` se ejecutan en orden LIFO al abandonar el bloque mediante la ruta
normal, `return`, `break` o `continue`.

Una distinción importante: el `defer` observa los valores al momento de salir;
no congela una copia de todos los argumentos cuando se declara.

## Destructores

```c
class Resource {
    int id;

    Resource(int id) {
        this.id = id;
    }

    ~Resource() {
        println(id);
    }
}
```

Los destructores de locales se ejecutan al salir del scope. `delete` llama al
destructor antes de liberar el objeto heap.

Las clases estándar propietarias suelen usar `destroy()` explícito en vez de
destructor del lenguaje.

## La regla del propietario único

TinyC+ no genera copias profundas automáticamente.

Si un tipo contiene un puntero propietario:

```text
A ----> heap
```

una copia simple puede producir:

```text
A ----      +----> mismo heap
B ----/
```

Si A y B intentan liberar ese recurso habrá double free. Si ninguno lo libera,
habrá leak.

Por tanto:

> Decida quién es el único propietario de cada recurso y haga visible esa decisión
> en el código.

## Strings

`string` es una vista: puntero + longitud. Un literal vive todo el programa.

`OwnedString` sí posee almacenamiento.

```c
var text = String.copy("hola");
defer text.destroy();

println(text.view());
```

No conserve `text.view()` después de `destroy()`.

## Slices

Una `Slice<T>` no posee los elementos. Su vida depende del storage subyacente.

```c
int[4] data = {1, 2, 3, 4};
var view = data[1:3];
```

Si el storage es dinámico y se libera o reubica, la slice deja de ser válida.

## Bounds checking

Los bounds se comprueban por defecto. Para una build donde el programador acepta
el riesgo:

```bash
tiny run programa.tc --no-bounds-check
```

Desactivar checks no arregla errores de vida útil.

## Closures propietarias

Una closure con entorno que escapa debe poder hacer explícita su propiedad:

```c
closure<int(int)> multiplier(int n)
{
    return owned((int value) => value * n);
}

int main()
{
    var f = multiplier(6);
    defer f.destroy();
    println(f(7));
    return 0;
}
```

`destroy()` libera el entorno de la closure. No destruye mágicamente recursos
propietarios capturados por puntero.

## Checklist práctico

Antes de cerrar una función que manipula recursos, revise:

```text
¿Qué valores son vistas?
¿Qué valores son propietarios?
¿Qué objeto destruye cada recurso?
¿Existe una salida temprana sin cleanup?
¿Estoy copiando accidentalmente un propietario?
¿Hay un Task/Future que contiene un resultado propietario aún no consumido?
```

Éste es el punto donde TinyC+ exige disciplina a cambio de no imponer GC/ARC.
