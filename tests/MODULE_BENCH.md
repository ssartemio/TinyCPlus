# Banco de pruebas de los módulos desarrollados

Ejecutor: [run_modules.py](run_modules.py). Usa únicamente la biblioteca estándar de
Python y las suites existentes. No necesita grpcio ni Protobuf Python para este banco.
Requiere Python 3.9+, un compilador C y la instalación de TinyCC/libtcc para el backend
por defecto. La preparación de dependencias se describe en el README del proyecto.

## Ejecutar

Desde la raíz del repositorio:

```sh
# Reconstruye el frontend y prueba libtcc y el compilador C del host.
python3 tests/run_modules.py --cc cc

# Usa el frontend existente y añade ASan/UBSan (POSIX, GCC/Clang).
python3 tests/run_modules.py --no-build --cc cc --sanitize

# Selección de módulos, repetición y rutas de informes.
python3 tests/run_modules.py --module clones iterators --backend both --cc cc \
  --repeat 3 --report build/selected-modules.json --junit build/selected-modules.xml

# Sólo backend por defecto.
python3 tests/run_modules.py --backend default --no-build
```

En Windows use `python` y el compilador externo disponible, por ejemplo `--cc gcc`.
`--sanitize` se limita actualmente a POSIX con GCC/Clang. El compilador externo se
detecta entre `cc`, `gcc` y `clang`; use `--cc RUTA` para elegirlo. Para el backend
`external` o `both`, la ausencia de ese compilador es un requisito pendiente, no
una omisión silenciosa. Cada etapa tiene un límite de 600 segundos, configurable
con `--timeout`.

## Cobertura y oráculos

| Módulo (`--module`) | Qué se comprueba | Fuentes principales |
| --- | --- | --- |
| `collections` | Set, Deque, PriorityQueue, BitSet y OrderedMap; operaciones aleatorias deterministas, orden, crecimiento y contratos inválidos. | `collections.tc`, `test_collections.py --only collections` |
| `clones` | Las siete colecciones: independencia del almacenamiento, vacíos, orden y comparador, destrucción del original y propiedad superficial de punteros. | `clones.tc`, `test_collections.py --only clones` |
| `iterators` | Las siete colecciones, cursores independientes, agotamiento, mapas clave–valor, bucles anidados, defer/continue/break/await, 50 invalidaciones y siete protocolos inválidos. | `iterators.tc`, `test_iterators.py` |
| `projects` | Manifiestos, módulos y FFI, rutas relativas/con espacios, overrides de CLI, build/run/check/test, fallos y conservación del ejecutable al emitir C. | `test_project.py` |
| `diagnostics` | Ubicación y subrayado, tabs/CRLF/BOM/UTF-8, líneas largas, imports y contexto concreto de genéricos. | `test_diagnostics.py` |

Los modelos de referencia usan arrays contiguos, pertenencia booleana y búsqueda
lineal para comparar resultados con las estructuras implementadas. Los errores
intencionales sólo pasan cuando su código y diagnóstico coinciden con lo esperado;
no se ocultan como fallos esperados del ejecutor. Los diagnósticos son del frontend
por lo que se ejecutan una vez por repetición, sin duplicarlos por backend C.

## ASan/UBSan

Con `--sanitize`, además de las pruebas funcionales seleccionadas:

- Emite C para `collections`, `clones` e `iterators`, lo compila con sanitizers y
  ejecuta cada programa seleccionado con detección de fugas.
- Construye un frontend instrumentado separado para `diagnostics` y `projects`.
  Ejecuta las suites correspondientes con ese frontend; para proyectos usa el
  compilador C externo al construir las aplicaciones de prueba.

Los ejecutables instrumentados se guardan como artifacts y no reemplazan `bin/tiny`.
Los casos negativos de runtime se verifican en las suites funcionales; los programas
C instrumentados ejercitan las operaciones y limpiezas válidas. No se desactiva la
verificación de asserts de Python: el ejecutor quita `PYTHONOPTIMIZE` del entorno de
sus procesos de pruebas.

## Informes y fallos

Por defecto se escriben:

- `build/module-bench/report.json`: selección, plataforma, Python, comandos, códigos
  reales de salida, duración, stdout/stderr, estados y resumen.
- `build/module-bench/report.xml`: JUnit para consumidores de CI.
- `build/module-bench/<run_id>/`: logs numerados y, cuando se solicitan sanitizers,
  fuentes C emitidas y ejecutables de esa ejecución.

Cada testcase de JUnit representa una **suite o etapa**, no cada assert interno.
La ejecución completa de ambos backends contiene diez etapas; con todos los
sanitizers son 22. `--repeat` repite suites funcionales, no la preparación ni los
sanitizers. Las duraciones sirven para diagnóstico local; no son un benchmark
estadístico de rendimiento.

El JSON incluye SHA-256 de fuentes relevantes y del frontend utilizado para asociar
la evidencia con el código probado aunque existan cambios locales. Los informes se
actualizan después de cada etapa y se reemplazan de forma atómica. `status=running`
indica una ejecución aún no terminada o interrumpida; sólo `passed` confirma una
finalización correcta. Los artifacts de ejecuciones anteriores permanecen en sus
directorios individuales, sin reutilizar sus resultados.

Los estados distinguen `passed`, `failed`, `error`, `timeout` y `skipped`. Un fallo
de compilación bloquea las suites dependientes en vez de probar un binario antiguo.
Un timeout mata el grupo de procesos de la etapa. Cualquier fallo, timeout o etapa
bloqueada devuelve 1; el uso incorrecto devuelve 2. Se continúa con las etapas
independientes para obtener un informe útil.

`tests/run_all.py` invoca este banco, por lo que el CI existente también lo ejecuta.
Sin `--cc`, la suite general selecciona sólo el backend por defecto; con `--cc`,
ambos. Los resultados detallados permanecen en los informes propios del banco.
Para publicar JUnit o logs en otro sistema, configure su recogida desde las rutas
anteriores; el ejecutor no publica ni envía resultados a servicios externos.

## Verificar el propio ejecutor

```sh
python3 tests/test_module_bench.py
```

Comprueba ejecuciones reales exitosas y fallidas, compilador inexistente, bloqueo
tras fallo de build, códigos de salida, timeout, repetición, logs separados y
consistencia entre JSON y JUnit. Los fallos intencionales se guardan en artifacts
separados; la prueba del ejecutor sólo pasa si los detecta correctamente.
