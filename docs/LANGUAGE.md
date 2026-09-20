# Lenguaje y memoria

TinyC+ usa archivos `.tc`, comentarios `//` y `/* */`, y un punto de entrada
`int main()`. Un `main` sin retorno explícito retorna cero. Otras funciones no
void deben retornar en todas las rutas. Las variables sin inicializador se
inicializan a cero; `var` exige inicializador.

## Tipos, expresiones y control

Tipos: `bool`, `i8/u8`, `i16/u16`, `i32/u32`, `i64/u64`, `float`, `double`,
`char`, `string`, punteros `T*`, arrays `T[N]` y `Slice<T>`.
`int`/`Error` son i32, `uint` u32, `byte` u8, `long` i64 y `size_t` u64.
Esta entrega está orientada a hosts de 64 bits. `char` representa un byte;
el TUI decodifica UTF-8 explícitamente.

```c
int scale(int value, int factor = 2) { return value * factor; }
int main() {
    var result = scale(factor: 3, value: 14);
    int[3] items = {1, 2, 3};
    for (index, item in items) println(index + item);
    println(result);
    return 0;
}
```

Hay `if/else`, `while`, `for` tradicional, `for(value in collection)` y
`for(index,value in collection)`, `break`, `continue`, `return`.
`switch` compara un entero, `char`, `bool`, enum o string contra etiquetas
constantes (literales o miembros de enum):

```c
enum Kind { Fn, Var, Type }
string describe(Kind kind) {
    switch (kind) {
        case Kind.Fn:
            return "función";
        case Kind.Var, Kind.Type:
            return "dato";
    }
}
```

Los casos **no caen** al siguiente: un `break` directo dentro de un caso es un
error; `continue` y un `break` dentro de un bucle anidado funcionan como siempre.
Cada caso tiene su propio scope, por lo que sus `defer` se ejecutan al salir del
caso. Se rechazan etiquetas duplicadas (también `16` y `0x10`) y más de un
`default`. Sobre un enum sin `default`, el switch debe cubrir todos sus valores;
un valor fuera de rango obtenido con `cast` termina el programa con un mensaje.
Un switch con `default` o exhaustivo cuenta como retorno en todas las rutas
cuando todos sus casos retornan. `case` y `default` no son palabras reservadas
fuera de un switch.

Se evalúan operandos y argumentos de izquierda a derecha; `&&`/`||` cortocircuitan.
Los argumentos nombrados se evalúan en orden escrito y después se reordenan
para la ABI. Los valores por defecto deben ser literales.
`cast<T>(value)` realiza una conversión explícita; `sizeof(T)` da bytes.
No hay exceptions, overload de operadores, herencia ni metaprogramación general.

Las promociones numéricas siguen el subconjunto C implementado: bool, char e
integers menores que i32 se promocionan a i32. El desbordamiento signed y los
desplazamientos inválidos conservan los riesgos de C; no hay aritmética checked.
`const` restringe la mutación directa del valor y de sus campos/elementos.
La API no impone un sistema transitivo de inmutabilidad sobre los recursos
apuntados ni métodos `const`.

## Memoria, defer y valores

La forma de defer implementada es una expresión, por ejemplo
`defer resource.destroy();` o `defer free(pointer);`. `delete` es una sentencia:

```c
int main() {
    int* item = new int(42);
    println(*item);
    delete item;
}
```

`new` reserva memoria, `delete` llama al destructor del objeto si existe y libera
la reserva. `malloc`, `calloc`, `realloc` y `free` están disponibles mediante
el prelude y `std.memory`. No hay recolector, borrow checker ni conteo automático.
`defer expresion;` se ejecuta en orden inverso al salir del bloque, incluyendo
return, break y continue. La expresión diferida observa los valores al salir,
no una captura de los argumentos al declarar el defer. Un panic termina el proceso;
no desenrolla los defer, aunque restaura la terminal activa.

```c
class Box {
    int value;
    Box(int value) { this.value = value; }
    ~Box() { println(value); }
    int get() { return value; }
}
Box make() { Box value(42); return value; }
int main() { var box = make(); println(box.get()); }
```

Las clases son valores C por composición. `Box b(42)` y `var b=Box(42)` crean
valores. El destructor de un local se llama al salir de su scope. Devolver
directamente ese local transfiere su valor y evita destruirlo en el productor.
Los parámetros y las copias son copias de campos, no copias profundas.
Copiar un propietario duplica sus punteros: **debe existir un solo responsable
de liberar cada recurso**. Para composición con recursos, escriba la limpieza
de los miembros en el destructor exterior. No hay destructores/copias implícitas
de elementos de `Array<T>` ni destrucción automática de temporales descartados.

Las clases estándar propietarias usan `destroy()` explícito, no `~Tipo()`.
Ejemplo: `var a=Array<int>.create(); defer a.destroy();`.
No combine destrucción explícita y automática del mismo objeto.
Guarde en variables los resultados propietarios y libérelos exactamente una vez.

## Strings, slices y resultados múltiples

`string`/`StringView` es `{const char* data, size_t length}`: una vista UTF-8 o de
bytes que puede contener NUL. Los literales viven todo el programa.
`s[lo:hi]` toma una vista, sin reservar memoria; `hi` es exclusivo.
`len(s)` y `.length` están disponibles. `==` compara contenidos de strings.
Los escapes incluyen `\0`, `\n`, `\t` y `\xNN`, con exactamente dos dígitos hex.

`String.copy`, `concat`, `fromInt`, `fromDouble` y `File.readAll` producen
`OwnedString`. `fromDouble` escribe el texto decimal más corto que se relee como
el mismo `double`. Para construir texto incrementalmente use `StringBuilder`
(`append`, `appendChar`, `appendInt`, `appendUnsigned`, `appendHex`,
`appendDouble`, `view`, `toOwned`, `clear`, `destroy`): crece por duplicación,
sin copias cuadráticas; su `view()` queda inválida tras el siguiente cambio.
Su `.view()` queda inválida después de `.destroy()`. Las slices de arrays
quedan inválidas al liberar/reubicar su almacenamiento.
Los bounds se comprueban por defecto; `--no-bounds-check` los desactiva.

```c
(int, Error) divide(int a, int b) {
    if (b == 0) return (0, 1);
    return (a / b, 0);
}
int main() { var value, error = divide(84, 2); assert(error == 0); println(value); }
```

`Error` es un código explícito: cero significa éxito. No existe propagación por
exceptions. Un tuple se representa como un struct C.

## Objetos, interfaces y genéricos

Propiedades almacenadas: `property int value;`; calculadas:
`property int doubled { get => value * 2; }`.
Las extensiones agregan métodos a un tipo conocido: `extension Box { ... }`.
Los métodos ordinarios se resuelven estáticamente.

```c
interface Reader { int read(); }
class Source { int value; int read() { return value; } }
int main() { Source source(42); Reader reader = &source; println(reader.read()); }
```

La conformidad de interfaces es estructural y exige firmas compatibles. La
conversión parte de un puntero y produce `{objeto,vtable}`; no reserva memoria.
El objeto debe vivir más que la interfaz. No hay boxing automático.

`class Box<T> { T value; }` y `T identity<T>(T value){return value;}` se
monomorfizan. Especifique los argumentos de funciones genéricas, por ejemplo
`identity<int>(42)`. El límite es 512 instancias por unidad. No hay constraints
genéricos; los errores se detectan al instanciar. `Array<T>` ofrece reserve,
push, pop, get, set, clear, destroy e indexación con bounds.

`hash(value)` devuelve un `u64` para enteros, `char`, `bool`, enums, punteros y
strings (por contenido). Una función de usuario llamada `hash` tiene prioridad.
`Map<K,V>` de `std.collections` es una tabla hash propietaria con esas mismas
claves; se comparan con `==`:

```c
import std.collections;
int main() {
    var ages = Map<string, int>.create();
    defer ages.destroy();
    ages.put("Ana", 30);
    var age, found = ages.get("Ana");
    if (found) println(age);
    println(ages.getOr("Luis", -1));
}
```

Ofrece put, get, getOr, contains, remove, keys, values, clear y destroy.
Las claves string son vistas: el mapa no copia el texto, que debe vivir más que
la entrada. El orden de `keys()`/`values()` no está especificado. Las claves de
tipo clase se rechazan al instanciar.

`enum Color { Red=-1, Green=2 }` usa almacenamiento i32; acceso `Color.Red`.

## Funciones, closures y streams

`func<int(int)>` es un puntero a función C. `var f=(int x)=>x*x;` no reserva
memoria si no captura. Lambdas con captura usan un entorno por valor; las
capturas anidadas se propagan. Cambiar la variable original no cambia la copia.
Las closures prestadas deben terminar de usarse antes de salir de su scope.

```c
closure<int(int)> multiplier(int n) { return owned((int value)=>value*n); }
int main() { var f=multiplier(6); defer f.destroy(); println(f(7)); }
```

`owned(...)` crea explícitamente un entorno en heap. Su destrucción libera el
entorno, no los recursos apuntados dentro de las capturas.

```c
int main() {
    int[4] data={1,2,3,4};
    println(data.stream().filter(x=>x>1).map(x=>x*2).reduce(0,(sum,x)=>sum+x));
}
```

Streams: filter/map y terminales forEach, count, first, any, all, reduce y
collect. `first()` retorna `(T,bool)`. `collect(&destination)` agrega elementos
a un `Array<T>` existente. Las cadenas se fusionan en un bucle; no hay arrays
intermedios. Los callbacks se evalúan una vez. La cadena requiere un terminal
en la misma expresión: no puede almacenarse un pipeline pendiente.

## Módulos y FFI

`module package.name;` declara el nombre; `import std.net;` o `import sibling;`
resuelve archivos `.tc`. Los imports son transitivos; se detectan archivos
repetidos por su ruta canónica y se admiten ciclos de importación.
Las funciones se pueden calificar (`package.name.function()`). La búsqueda
sin calificador prefiere el módulo propio y rechaza ambigüedades. Las clases y
enums de distintos módulos todavía deben tener nombres globalmente únicos.
Hay tipos calificados; use aliases importados sin prefijo en llamadas genéricas.

`extern C { ... }` declara funciones, structs y enums con ABI C. Las funciones
pueden usar `cdecl`/`stdcall`; Windows x64 usa la ABI unificada del host.
No se ha validado x86 de 32 bits. No existe ABI C++ ni importación automática
de headers. Los tipos y layouts deben coincidir exactamente con la biblioteca.

```powershell
.\bin\tiny.exe run examples\ffi.tc --c-source examples\ffi_math.c
.\bin\tiny.exe build programa.tc --c-source biblioteca.o -I include -L lib -l biblioteca
```

Las opciones se pasan como argumentos separados, sin construir comandos shell.
`--emit-asm` requiere GCC/Clang y una sola unidad C; para gRPC emita C y compile
nghttp2 por separado.
