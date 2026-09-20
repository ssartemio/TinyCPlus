# Objetos, interfaces y genéricos

TinyC+ ofrece abstracciones orientadas a objetos sin construir una jerarquía de
runtime pesada.

## Clases como valores

```c
class Point {
    int x;
    int y;

    Point(int x, int y) {
        this.x = x;
        this.y = y;
    }

    int sum() {
        return x + y;
    }
}

int main()
{
    Point p(20, 22);
    println(p.sum());
    return 0;
}
```

Mentalmente, piense en:

```text
class ≈ struct + funciones + sintaxis
```

## Propiedades

Propiedad almacenada:

```c
class Counter {
    property int value;
}
```

Propiedad calculada:

```c
class Rectangle {
    int width;
    int height;

    property int area {
        get => width * height;
    }
}
```

## Extensiones

Una extension añade métodos sin modificar el layout del tipo.

```c
extension Point {
    int doubled() {
        return (x + y) * 2;
    }
}
```

## Interfaces estructurales

```c
interface Reader {
    int read();
}

class Source {
    int value;

    Source(int value) {
        this.value = value;
    }

    int read() {
        return value;
    }
}

int main()
{
    Source source(42);
    Reader reader = &source;
    println(reader.read());
    return 0;
}
```

No se necesita una declaración `implements Reader`. La conformidad depende de
la firma.

La representación conceptual es:

```text
interface value
 ├── object pointer
 └── vtable pointer
```

No hay boxing heap obligatorio.

La vida del objeto concreto debe superar la vida de la interfaz.

## Genéricos

```c
class Box<T> {
    T value;
}

T identity<T>(T value)
{
    return value;
}
```

Uso:

```c
var a = identity<int>(42);
var b = identity<double>(3.14);
```

TinyC+ **monomorfiza**:

```text
identity<int>     → versión concreta int
identity<double>  → versión concreta double
```

No hay type erasure universal ni maquinaria de templates C++.

## Array<T>

```c
import std.collections;

int main()
{
    var values = Array<int>.create();
    defer values.destroy();

    values.push(1);
    values.push(2);
    values.push(3);

    println(values.get(1));
    println(values.pop());
    return 0;
}
```

`Array<T>` gestiona su storage, pero no destruye automáticamente los recursos
propietarios que puedan existir dentro de cada elemento. El destructor del
elemento sigue siendo responsabilidad del diseño del programa.
