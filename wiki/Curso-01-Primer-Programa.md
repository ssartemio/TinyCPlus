# Capítulo 1 — Primer programa

**Objetivo:** aprender editar → verificar → ejecutar → inspeccionar.

Cree `tinystatus.tc`:

```c
int main()
{
    var temperature = 24;
    var load = 37;
    var score = temperature + load;

    println(score);
    return 0;
}
```

Ejecute:

```bash
tiny check tinystatus.tc
tiny run tinystatus.tc
tiny --emit-c tinystatus.tc -o build/tinystatus.c
```

Salida: `61`.

`var` infiere un tipo **estático**; no hay tipado dinámico. Compare:

```c
var temperature = 24;
int temperature2 = 24;
i32 temperature3 = 24;
```

Añada una decisión:

```c
if (score < 80)
    println("OK");
else
    println("WARN");
```

## Modelo mental

```text
.tc → lexer → AST → tipos → C11 → código nativo
```

## Ejercicios

1. Haga aparecer `WARN`.
2. Añada `bool connected`.
3. Use `--emit-ast` y `--emit-tokens`.
4. Localice en el C generado las variables inferidas.

**Punto de control:** explique por qué `var` no necesita una representación
runtime especial.

[Siguiente → Capítulo 2](Curso-02-Funciones-y-Control)
