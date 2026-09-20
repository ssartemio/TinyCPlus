# Cookbook de ejemplos

Esta página reúne patrones representativos para copiar, ejecutar y modificar.

## Hola mundo aritmético

```c
int main()
{
    var x = 10;
    var y = 20;
    println(x + y);
    return 0;
}
```

## Error explícito + múltiples retornos

```c
(int, Error) divide(int a, int b)
{
    if (b == 0)
        return (0, 1);

    return (a / b, 0);
}

int main()
{
    var value, error = divide(90, 3);

    if (error != 0)
        return error;

    println(value);
    return 0;
}
```

## Array dinámico

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

## Stream fusionado

```c
import std.collections;

int main()
{
    int[6] values = {1, 4, 8, 12, 20, 30};

    values.stream()
          .filter(x => x > 10)
          .map(x => x * 2)
          .forEach(x => println(x));

    return 0;
}
```

## Closure owned

```c
closure<int(int)> multiplier(int n)
{
    return owned((int value) => value * n);
}

int main()
{
    var fn = multiplier(6);
    defer fn.destroy();

    println(fn(7));
    return 0;
}
```

## Spawn

```c
import std.concurrent;

int work(int value)
{
    return value * 2;
}

int main()
{
    var task = spawn work(21);
    defer task.destroy();

    println(task.get());
    return 0;
}
```

## Async + Future

```c
import std.concurrent;

async int consume(Future<int> input)
{
    var value = await input;
    return value * 2;
}
```

## FFI C

```c
extern C {
    int apply_callback(func<int(int)> callback, int value);
}

int main()
{
    println(apply_callback((int x) => x * x, 4));
    return 0;
}
```

## TUI

```c
import std.tui;

int main()
{
    var window, error = Window.create();
    if (error != 0)
        return error;

    defer window.destroy();

    var root = Ui.column();
    root.add(Ui.label("TinyC+"));

    window.setContent(root);

    while (true) {
        window.draw();
        window.refresh();

        var event = window.nextEvent();
        if (event.kind == 1 && event.key == 17)
            break;

        window.dispatch(event);
    }

    return 0;
}
```

## Cómo explorar los ejemplos reales

```bash
tiny run examples/hello.tc
tiny run examples/core.tc
tiny run examples/collections.tc
tiny run examples/streams.tc
tiny run examples/async.tc
tiny run examples/grpc_users.tc
tiny run examples/tui.tc
```

Para FFI:

```bash
tiny run examples/ffi.tc --c-source examples/ffi_math.c
```

La mejor forma de aprender TinyC+ es modificar estos ejemplos y observar
`--emit-c`.
