# Troubleshooting

## "tiny" no encuentra runtime/std/third_party

No mueva sólo el ejecutable. Mantenga el layout del proyecto o configure:

```text
TINY_HOME
```

También puede usar `--home RUTA`.

## El REPL dice que necesita libtcc

Es esperado: el REPL compila celdas en memoria y requiere libtcc.

Los comandos build/run pueden usar un compilador externo.

## Un programa compila con GCC pero falla con TinyCC

Genere el C:

```bash
tiny --emit-c programa.tc -o build/programa.c
```

Compile el mismo C con GCC/Clang y TinyCC. Si el comportamiento difiere, reduzca
el caso y reporte el backend afectado.

## Bounds failure

Los bounds checks están activados por defecto.

No use `--no-bounds-check` como forma de “arreglar” el programa. Primero
compruebe índices, longitud y vida del storage.

## Double free / use-after-free

Revise:

```text
¿copié un objeto propietario?
¿llamé destroy() y además se ejecuta un destructor?
¿una slice/view sobrevivió al storage?
¿destruí un Future/Task antes de consumir un resultado propietario?
```

Compile con sanitizers cuando sea posible.

## Deadlock con Task/WorkerPool

Si todos los workers bloquean con `.get()` esperando trabajo que necesita esos
mismos workers, el pool puede quedar sin capacidad para progresar.

Prefiera `await` dentro de flujos async.

## gRPC no funciona contra un endpoint TLS

La implementación 1.0 RC es h2c. TLS no forma parte todavía del transporte
entregado.

## TinyEdit deja la terminal extraña después de matar el proceso

La salida normal y panic restauran la terminal activa. Una terminación forzada
del proceso puede impedir esa limpieza.

## FFI produce datos corruptos

Confirme que:

- tipos y tamaños coinciden;
- el struct tiene el mismo layout;
- calling convention es correcta;
- signedness coincide;
- no está pasando una vista a storage ya liberado.

## GUI experimental no abre ventana

Primero confirme la rama/PR que está usando. `main` 1.0 RC no incluye todavía
la GUI gráfica.

Win32, X11 y Cocoa se desarrollan en líneas post-1.0 separadas.
