# Glosario

**AST** — árbol sintáctico producido por el parser.

**Backend** — capa que convierte el C generado en código nativo mediante libtcc
o un compilador externo.

**Borrowed / prestado** — valor que referencia storage cuyo propietario está en
otro lugar. No debe destruirse como si fuera dueño.

**Closure** — función junto con un entorno de capturas.

**Damage rectangle** — región mínima de una superficie gráfica que necesita
repintarse. Usado por la GUI experimental.

**defer** — registra una expresión de cleanup para la salida normal del scope.

**FFI** — Foreign Function Interface; frontera para llamar ABI C.

**Future<T>** — resultado pendiente que se completa una sola vez.

**Headless** — ejecución sin terminal/ventana real, útil para tests.

**h2c** — HTTP/2 sin TLS. Transporte del gRPC 1.0 RC.

**Interface estructural** — interfaz satisfecha por tener las firmas requeridas,
sin `implements` obligatorio.

**Lowering** — traducción de una construcción TinyC+ a formas más simples,
normalmente C11.

**Monomorfización** — creación de una implementación concreta por combinación de
tipos genéricos utilizada.

**Owned / propietario** — valor responsable de liberar un recurso.

**OwnedString** — string con almacenamiento propio que requiere `destroy()`.

**REPL** — sesión interactiva compilada mediante libtcc.

**Slice<T>** — vista pointer+length sobre una secuencia; no posee el storage.

**State machine** — representación usada para reanudar funciones async después
de un `await`.

**Stream fusion** — lowering de cadenas filter/map/terminal a loops sin
colecciones intermedias.

**Task<T>** — unidad lógica de trabajo, distinta de un hilo del OS.

**TCC / TinyCC** — compilador C ligero usado como backend central.

**TUI** — interfaz de usuario basada en terminal/celdas.

**Vtable** — tabla de funciones usada por valores de interface.
