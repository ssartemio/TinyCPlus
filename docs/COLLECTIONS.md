# Colecciones

Importe `std.collections`. Además de `Array<T>` y `Map<K,V>`, el módulo ofrece
`Set<T>`, `Deque<T>`, `PriorityQueue<T>`, `BitSet` y `OrderedMap<K,V>`.
Las firmas completas están en [la referencia generada](api/collections.md).

## Contratos comunes

Las colecciones tienen propiedad manual del almacenamiento. Sus copias por valor
comparten punteros: no copie una colección para obtener una colección independiente.
Llame `destroy()` una sola vez sobre el propietario después de su último uso;
`defer collection.destroy()` es la forma habitual. Ni `clear`, ni `remove`, ni
`destroy` ejecutan destructores de los elementos, claves o valores. Libere esos
recursos por separado si los posee. Un `pop` devuelve una copia del elemento.

Los campos de almacenamiento son detalles de implementación aunque el lenguaje
permita acceder a ellos. Use los métodos para mutar; `length` es de consulta.
No modifique las tablas internas, los índices, el comparador ni los contadores.
Las colecciones no tienen sincronización interna.

Las operaciones inválidas y los fallos de reserva terminan mediante `assert`
(código 101), siguiendo el contrato existente de `Array`. No son errores
recuperables. En una búsqueda ausente, los mapas devuelven `(valor cero, false)`;
compruebe siempre el booleano o use `getOr`.

`Set` y `OrderedMap` usan las mismas claves que `Map`: enteros, caracteres,
booleanos, enums, punteros y strings, con `hash()` y `==`. No aceptan callbacks de
hash/igualdad. Las claves string son vistas prestadas: sus bytes deben permanecer
vivos y sin cambios mientras exista la entrada. Nunca cambie una clave en sitio.

## Elegir una estructura

| Tipo | Operaciones principales | Coste |
| --- | --- | --- |
| `Set<T>` | `add`, `contains`, `remove`, `values` | Hash: O(1) esperado amortizado, O(n) peor caso; snapshot O(capacidad). |
| `Deque<T>` | `pushFront`, `pushBack`, `popFront`, `popBack`, `get`, `set` | Extremos O(1) amortizado, acceso O(1); crecimiento O(n). |
| `PriorityQueue<T>` | `push`, `peek`, `pop` | Insertar/extraer O(log n), consultar O(1); una inserción con crecimiento puede costar O(n). |
| `BitSet` | `set`, `reset`, `contains`, `count`, operaciones de conjuntos | Acceso O(1), operaciones por palabras O(ceil(bits/64)); count recorre palabras y bits activos. |
| `OrderedMap<K,V>` | `put`, `get`, `remove`, `keyAt`, `valueAt` | Hash O(1) esperado amortizado; eliminar O(n) esperado; acceso por índice O(1). |

Los costes de hash suponen claves de tamaño acotado; calcular hash o comparar un
string depende de su longitud. La degradación por colisiones también afecta a
`OrderedMap.remove`, que actualiza los índices de las entradas desplazadas.

### Set

`add(value)` devuelve true sólo si el elemento era nuevo. `remove` devuelve true
sólo si estaba presente. `values(&array)` añade un snapshot sin orden garantizado,
sin vaciar el destino. Las reservas invalidan punteros a las tablas internas.

### Deque

Es un buffer circular que se vuelve contiguo cuando crece. Sus índices lógicos
van de cero a `length - 1`, independientemente de dónde esté la cabeza física.
`reserve(required)` conserva el orden. Los pops sobre una cola vacía y los índices
fuera de rango fallan. Crecer invalida punteros al almacenamiento; mover los
extremos cambia la relación entre índices lógicos y posiciones físicas.

### PriorityQueue

Construya la cola con `create(compare)`. El comparador es una función sin capturas
`func<bool(T,T)>`: devuelve true cuando el primer argumento debe salir antes que
el segundo. Debe establecer un orden débil estricto y mantenerse consistente
mientras haya elementos. Use `<` para mínimos y `>` para máximos, o compare un
campo de un tipo propio. No modifique prioridades almacenadas a través de punteros.
Los empates no conservan orden de inserción; añada un desempate explícito si lo necesita.

`clear()` conserva el comparador y la reserva. `destroy()` invalida la cola para
nuevas inserciones; vuelva a construirla con `create`. `peek` y `pop` requieren una
cola no vacía. El heap mueve elementos: no hay referencias estables a ellos.

### BitSet

`create(bits)` establece el tamaño fijo del universo, inicialmente vacío. `length`
es ese tamaño, no el número de bits activos; `count()` calcula los activos.
El tamaño cero es válido, pero no admite acceso a índices. `unionWith`,
`intersectWith` y `differenceWith` modifican el receptor y exigen tamaños iguales.
Se permite operar con el propio conjunto. No se reserva al modificar bits.

### OrderedMap

La iteración se hace por índice: `keyAt(i)` y `valueAt(i)` recorren el orden de
inserción. Actualizar una clave existente cambia su valor y conserva la posición;
eliminar y reinsertar la coloca al final. `remove` desplaza las entradas posteriores
y actualiza sus índices: esta implementación prioriza recorrido denso y simple.

`keys(&array)` y `values(&array)` añaden snapshots correspondientes, en orden de
inserción. El destino no debe compartir almacenamiento con la colección. No mute
el mapa entre ambos snapshots si necesita que correspondan. El crecimiento
invalida punteros; eliminar cambia los índices posteriores. `clear` conserva reservas.

## Copias independientes con clone()

Las siete colecciones ofrecen `clone()`. Devuelve un propietario nuevo con
almacenamiento independiente y los mismos elementos lógicos:

```c
var original = Array<int>.create();
defer original.destroy();
original.push(42);
var copy = original.clone();
defer copy.destroy();
copy.set(0, 7);
assert(original.get(0) == 42);
```

Puede modificar, vaciar o destruir cada propietario sin afectar al otro.
La copia no modifica ni invalida los iteradores del original. Una colección vacía
también puede clonarse. La capacidad reservada, las posiciones hash y los tombstones
no se conservan como parte del contrato; `Deque` conserva el orden lógico,
`OrderedMap` el orden de inserción y `PriorityQueue` el heap y su comparador.

La copia es profunda **respecto al almacenamiento de la colección**, pero los
elementos, claves y valores se copian por valor. Un string prestado sigue apuntando
a los mismos bytes; un puntero, handle u objeto con recursos sigue refiriéndose a
sus recursos originales. No ejecute dos veces su destructor por haberlo clonado
en un contenedor. Para copiar esos recursos, implemente su copia explícita y
construya el nuevo contenedor con ella.

Los costes son O(n) para Array, Deque, PriorityQueue y OrderedMap (esperado para
sus índices hash), O(capacidad + n) esperado para Map y Set, y O(palabras) para
BitSet. El resultado debe destruirse incluso si el original ya se destruyó.
`tests/clones.tc` comprueba independencia, limpieza, vacíos, tumbas hash, buffers
circulares, orden, comparadores y la compartición intencional de punteros.

## Iteradores y `for`

Las siete colecciones ofrecen `iterator()`. El cursor es un valor pequeño sin
reserva propia ni `destroy`; toma prestado el propietario. Copiar un **cursor**
produce una posición independiente sobre el mismo propietario, a diferencia de
copiar una colección propietaria. El propietario debe permanecer vivo en la misma
dirección durante todo el recorrido; los elementos devueltos son copias por valor,
sin copia profunda ni transferencia automática de recursos.

```c
var queue = Deque<int>.create();
defer queue.destroy();
queue.pushBack(10);
queue.pushBack(20);
for (index, value in queue) println(value);

var cursor = queue.iterator();
while (true) {
    var index, value, available = cursor.next();
    if (!available) break;
    println(value);
}
```

`next()` devuelve `(clave o índice, valor, bool)`. Si el booleano es false,
el recorrido terminó y los otros dos resultados son cero; llamadas posteriores
siguen devolviendo false mientras no se modifique el propietario.

| Colección | Primer resultado | Segundo resultado y orden |
| --- | --- | --- |
| `Array<T>` | Índice desde cero | Elemento, en orden del array. |
| `Deque<T>` | Índice lógico desde cero | Elemento, del frente al final. |
| `Map<K,V>` | Clave | Valor; orden de la tabla, sin garantía de orden de inserción. |
| `Set<T>` | Ordinal consecutivo desde cero | Elemento; orden no especificado. |
| `OrderedMap<K,V>` | Clave | Valor, en orden de inserción. |
| `PriorityQueue<T>` | Índice interno desde cero | Elemento en orden del heap, **sin ordenar por prioridad ni extraerlo**. |
| `BitSet` | Ordinal consecutivo desde cero | Índice de bit activo, en orden ascendente. |

`for (value in collection)` toma sólo el segundo resultado. En mapas, use
`for (key, value in map)` para obtener ambos. El propietario puede ser una
variable, un campo o un puntero; la expresión se evalúa una vez al entrar.
Para una colección devuelta por valor desde una función, asígnela primero a una
variable y gestione su `destroy`; no se admite un propietario temporal.
Los arrays fijos, slices y strings conservan su recorrido existente.

### Modificaciones durante el recorrido

Los iteradores comprueban la versión del **propietario original** antes de leer.
Una llamada a un método mutador después de crear el cursor lo invalida: el siguiente
`next()` termina con `assert` (101). Esto incluye `set`, reemplazar un valor de
mapa, `clear`, `destroy`, reservas sin crecimiento, eliminar una clave ausente o
añadir un elemento repetido al Set. Las consultas y crear otros cursores no lo
invalidan. Las llamadas internas pueden incrementar la versión varias veces;
su valor concreto no forma parte de la API.

`for` comprueba esa condición al pedir el siguiente elemento, incluso después del
último elemento válido. Si modifica y sale inmediatamente mediante `break` o
`return`, no hay otra petición y no se emite un error. Para modificar una colección
mientras selecciona elementos, recorra primero un snapshot independiente o acumule
los cambios y aplíquelos al terminar.

Esta detección no reemplaza la sincronización ni vuelve válido usar un cursor tras
salir del ámbito de su propietario. Tampoco cubre escrituras directas a campos,
punteros de elementos o copias superficiales de la colección: esas operaciones
eluden el contrato de la API. Destruir el almacenamiento con el propietario todavía
vivo sí se detecta; acceder a un objeto propietario cuya vida ya terminó es inválido.

### Protocolo para tipos propios

El compilador reconoce una clase con un método de instancia síncrono sin argumentos
`iterator()`, que devuelve por valor otra clase con un método de instancia síncrono
`next()` sin argumentos. Su retorno debe ser exactamente `(K, V, bool)`.
Las variables del `for` reciben `K` y `V`, o sólo `V` si se indica una variable.
El cursor debe ser prestado, sin recursos que requieran destructor: `for` no añade
una llamada a `destroy` del cursor. Cada implementación define su orden, agotamiento
y política de invalidación; las comprobaciones de versión pertenecen a los
iteradores de `std.collections`, no se insertan en cualquier tipo automáticamente.

`break`, `continue`, bucles anidados y `defer` conservan su comportamiento. En
funciones async, el cursor se conserva en el frame entre suspensiones; el propietario
y sus recursos deben seguir vivos. No mute la colección desde otra tarea sin una
sincronización apropiada.

## Ejemplo ejecutable

[examples/collections.tc](../examples/collections.tc) muestra las cinco estructuras:

```sh
./bin/tiny run examples/collections.tc
./bin/tiny run examples/collections.tc --cc cc
```

## Verificación

`tests/collections.tc` compara operaciones deterministas con modelos sencillos:
un array contiguo para la deque, pertenencia booleana para Set/BitSet, extracción
mínima mediante búsqueda lineal para el heap y arrays ordenados para OrderedMap.
Incluye crecimiento con la cabeza desplazada, prioridades duplicadas, comparadores
mínimo/máximo y un tipo propio, límites de palabras y eliminación/reinserción.

`tests/test_collections.py` ejecuta ese programa y casos negativos que deben
terminar con 101. `tests/run_all.py` incluye estas pruebas con libtcc y, al usar
`--cc`, con el compilador externo. El frontend también tiene regresiones para
sustituir parámetros genéricos en argumentos y retornos de callbacks y closures.

```sh
python3 tests/test_collections.py
python3 tests/test_collections.py --cc cc
```

Para comprobar accesos y liberaciones del C generado en Linux con GCC/Clang:

```sh
mkdir -p build
./bin/tiny --emit-c tests/collections.tc -o build/collections-check.c
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Iruntime \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  build/collections-check.c -lm -o build/collections-sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/collections-sanitize
```

Las pruebas de recorrido y protocolo están en `tests/iterators.tc` y
`tests/test_iterators.py`. Comprueban las siete colecciones, orden, cursores
independientes y agotamiento, tombstones, buffers circulares, control de flujo,
suspensión async, invalidación y diagnósticos de protocolos incorrectos.

```sh
python3 tests/test_iterators.py
python3 tests/test_iterators.py --cc cc
```
