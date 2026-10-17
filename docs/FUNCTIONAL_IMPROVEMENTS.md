# Mejoras funcionales de la revisión transversal

Esta implementación sigue `build/zen-review/RECOMMENDATIONS.md`. Mantiene C11,
memoria manual, tuplas de resultado y generación de C inspeccionable. No añade
excepciones, destrucción recursiva de elementos ni reintentos de red implícitos.

## Strings y colecciones

`StringBuilder.append` admite una vista del propio buffer, incluidas subvistas,
con y sin crecimiento: calcula el offset antes de `realloc` y copia con `memmove`.
Una entrada inválida o un fallo de reserva sigue siendo una precondición exigida
por `append`; las vistas prestadas anteriores siguen invalidándose al modificar.

`Array`: `tryPop`, `tryPeek`, `tryGet`; `Deque`: `tryPopFront`, `tryPopBack`,
`tryPeek` (frente), `tryGet`; `PriorityQueue`: `tryPop`, `tryPeek`;
`Map` y `OrderedMap`: `tryGet` además del `get` recuperable existente.
Devuelven `(valor, encontrado)`; el valor ausente está inicializado a cero.
Un intento vacío no muta el contenedor ni invalida sus iteradores.

Los siete contenedores, `OwnedString` y `StringBuilder` tienen `take()`: transfiere
almacenamiento y deja vacío al origen. En contenedores invalida los iteradores del
origen. `clone()` conserva su contrato de copia de almacenamiento, con elementos
copiados por valor; préstamos mediante punteros/vistas no transfieren propiedad.

`tiny check fuente.tc --ownership-check` añade errores para copias directas de
propietarios, doble cierre/destrucción y usos posteriores en un bloque lineal.
Es optativo para mantener compatible el código manual existente. No analiza
aliases, todos los caminos de control, contratos de funciones ni propiedad de
los elementos. No sustituye a ASan. Los retornos, `take` y préstamos siguen
siendo explícitos; el objeto vacío tras `take` puede reutilizarse.

Los sockets creados con las APIs públicas y los objetos socket vacíos tienen
cierre idempotente; un socket vacío no cierra el descriptor 0. Para adoptar un
handle externo se usa el constructor `TcpSocket(handle)` / `UdpSocket(handle)`.
`Task.destroy()` deja a cero el handle de una variable; `File.close()` admite
un archivo vacío. `retain()` añade una referencia, `destroy()` la libera;
una copia de bits nunca añade una referencia automáticamente.

## Tipos y módulos

Clases, enums, plantillas de clase y funciones genéricas conservan identidad de
archivo/módulo. `left.Item`, `right.Item`, `left.Box<int>` y `right.Box<int>` pueden
coexistir. La calificación funciona en declaraciones y expresiones genéricas.
Los tipos de módulos distintos son incompatibles aunque sus campos coincidan.
Los nombres importados ambiguos producen error; los locales y miembros tienen
precedencia sobre los símbolos globales importados. El ABI de tipos `extern C`
conserva su nombre C: declaraciones equivalentes de dos módulos comparten identidad
C, incluidas referencias recursivas; layouts incompatibles producen diagnóstico.

## I/O, errores y concurrencia

TCP ofrece `readSome`, `readExactly`, `writeSome`, `writeAll` y las cuatro variantes
`Async`. Las versiones síncronas devuelven `(bytes, error nativo)`; EOF limpio es
error 0 y puede dejar una lectura exacta incompleta. Las versiones asíncronas
conservan `(bytes, error)` mediante `Task.result()`, incluso al cancelar o agotar
el plazo después de una transferencia parcial. `read` y `write` conservan su
contrato histórico de una operación parcial. `writeAsync` conserva escritura total.

Archivos ofrecen `readSome`, `readExactly`, `writeSome` y `writeBytes` (todos los
bytes), con `(bytes, error nativo)`. `File.open` conserva errno cuando falla.
`Errors.system(code, contexto)` y `Errors.task(code, task.nativeError(), contexto)`
construyen `ErrorInfo`: categoría estable, código nativo y contexto prestado.
`ErrorCategory` y `TaskError` evitan interpretar números mágicos. Las APIs antiguas
siguen devolviendo sus errores originales: la adaptación es explícita, sin cambiar
su ABI. `File.writeAtomicResult` adapta errno en POSIX y GetLastError en Windows al mismo
ErrorInfo; conserva el path como contexto prestado. Los resultados se comprueban
antes de usar el recurso. Un error sin código nativo disponible conserva la
categoría de runtime; no se inventa un errno.

`connectAsync` conserva la lista de direcciones para continuar tras una conexión
asíncrona rechazada. UDP selecciona destinos de la familia del socket local.
Las duraciones están en milisegundos; timeout 0 significa sin plazo, salvo
`Timer.after(0)`, que programa la siguiente oportunidad del reactor.
Los buffers y tokens prestados deben vivir hasta completar la operación.

Los pools limitan por defecto 1.024 trabajos y 64 MiB pendientes, excluyendo los
trabajos que ya están ejecutándose. `WorkerPool.limits(jobs, bytes)` permite
ajustarlos; el runtime contabiliza bytes de frames y trabajos de I/O copiados.
El callback genérico `submit` no conoce el tamaño del argumento prestado y sólo
contabiliza el trabajo. Saturación devuelve error 8; las tareas generadas y el
I/O asíncrono completan su futuro con error, sin abortar por rechazo de cola.
`destroy()` del pool drena trabajos: un callback arbitrario bloqueado requiere
su propia cancelación cooperativa.

`TaskGroup.waitAll()` / `wait()` esperan todos los hijos; `cancelOnError()` observa
los completados independientemente del orden, cancela su token al primer error y
une a todos antes de devolver. No termina por fuerza una función que ignora el
token. `TaskWait.any` acepta futuros prestados y devuelve `(índice, error)`, con
timeout/token opcionales; puede competir con el futuro de `Timer.after`.
Los perdedores permanecen vivos y deben cancelarse/unirse antes de liberar buffers.

Al finalizar el programa se rechaza nuevo I/O, se cancela el reactor, se esperan
las tareas y se liberan los recursos del reactor. El apagado directo de I/O cancela
peticiones pendientes, incluido `accept` sin plazo. El resolver del sistema y
operaciones de archivo bloqueantes no pueden interrumpirse portablemente: siguen
siendo una limitación expresa, no una garantía de apagado acotado universal.

## Herramientas

- `tiny fmt archivo.tc --check`: devuelve 1 si cambia el formato, sin escribir.
- `tiny check archivo.tc --json`: documentos JSON por unidad (NDJSON), esquema 1,
  `ok` y `diagnostics`; errores incluyen posición y contexto de instanciación.
  Los errores de manifiesto también usan JSON. El compilador conserva el criterio
  de detener la unidad en el primer error.
- `tiny --emit-typed-ast archivo.tc [-o archivo.json]`: esquema 1, nodos con kind
  numérico de `NodeKind`, nombre, texto, tipo inferido, archivo, línea, columna e
  hijos separados por rol. Incluye módulos cargados e instancias monomorfizadas.
- `tiny test archivo.tc --list`; `--filter texto` selecciona una subcadena del
  nombre calificado. Cero coincidencias devuelve error.
- `tiny test archivo.tc --isolate`: compila una vez y ejecuta cada test en un hijo;
  un panic/exit no impide ejecutar los siguientes. Muestra estado de salida del
  hijo. Los límites temporales e informes JSON/JUnit pertenecen al banco existente.
- `tiny doctor [--cc compilador]`: compila y ejecuta un programa nativo de prueba.
  `tiny capabilities` exporta capacidades/disponibilidad de libtcc, sin afirmar
  que las bibliotecas o ventanas opcionales hayan sido probadas.
- `tiny build --explain`: muestra backend y opciones efectivas, incluidas rutas
  de runtime, GUI y opciones de enlace.

Estos modos aceptan los manifiestos de proyecto allí donde se acepta compilación.
Los bancos existentes mantienen JSON/JUnit, hashes, timeouts y estados de fallo;
no se introduce un segundo formato de resultados de tests.

## Procesos, archivos y paquetes

`Process.runShell` hace explícito el shell; `run` queda como alias compatible.
`Process.spawn` sigue siendo el camino argv síncrono. `ProcessHandle.start(&args)`
devuelve `(handle, error)`; `wait(timeout)` no consume el handle, timeout 0 espera
sin plazo, y timeout devuelve `(-1, 4)` manteniendo vivo al hijo. `cancel` termina
al hijo directo; `destroy` cancela si sigue vivo, espera y libera el handle. No
promete cancelar un árbol de descendientes ni admite llamadas concurrentes.

Windows convierte paths UTF-8 a UTF-16 para fopen, existencia, borrado y reemplazo
atómico, y usa `CreateProcessW` para argv. El reemplazo atómico vacía el archivo,
pero en POSIX no sincroniza el directorio padre: no promete persistencia de su
entrada ante pérdida de energía. La validación de Windows requiere su runner.

`tools/package.py` nombra el paquete por plataforma/arquitectura del host,
selecciona binarios nativos e incluye TinyCC POSIX cuando corresponde. Añade
`CAPABILITIES.json`, dependencias, SHA256 y metadatos de permisos. La prueba de
distribución extrae en otra ruta y comprueba hashes, `doctor` y el REPL. Requiere
el runtime/headers del host declarados; no es un paquete universal ni estático.

## GUI y editor

Se integra selectivamente la base GUI de `feature/gui-widgets` (f863662), conservando
las mejoras locales del compilador y las rutas TinyCC POSIX. Renderer común,
headless, Win32/GDI, X11 y Cocoa optativos; `std.input` comparte códigos de teclas
con TUI sin cambiar los kinds de sus eventos. Shift+Tab retrocede el foco y
GuiProgressBar usa i64 en los productos intermedios. TextBox decodifica directamente
las dos regiones del gap buffer: no reserva una copia completa durante el dibujo.

TinyEdit habilita historial explícito: Ctrl-Z undo, Ctrl-Y redo; una edición nueva
invalida redo. Conserva posición y estado guardado, y cubre inserción, borrado,
corte y pegado. GapBuffer permite habilitarlo con `enableHistory`; está limitado
a 64 snapshots / 8 MiB de texto en la pila undo y está desactivado por defecto.
Las ediciones grandes pueden descartar historial; no es un log persistente.

Las ventanas y superficies se usan en el hilo de UI. Las superficies prestadas
no se destruyen; su dueño debe seguir vivo. La cola acotada puede devolver error
al llenarse. Las pruebas deterministas cubren renderer, entrada, cierre y resize;
`tests/test_gui_native.py` exige un escritorio real o Xvfb. CI añade pruebas nativas
por plataforma; configurarlas no equivale a haber ejecutado esos runners aquí.

## Protocolos condicionados por el informe

Se conserva la interoperabilidad unary h2c de gRPC y Protobuf existente.
No se anuncia TLS, metadata nueva, reutilización de conexiones, retry ni oneof.
El informe condiciona esas extensiones a necesidades/aplicaciones, a una política
de idempotencia y a un modelo explícito de representación/ownership; esos contratos
no estaban definidos. Tampoco se añaden streaming, frameworks, gestor remoto de
paquetes ni autohospedaje como nuevas funcionalidades dentro de este lote.

## Comprobaciones reproducibles

```sh
python build.py --cc cc
python tests/run_all.py --cc cc
python tests/test_improvements.py
python tests/test_distribution.py
cc -std=c11 -D_POSIX_C_SOURCE=200809L -fsanitize=address,undefined -g tests/runtime_review.c -pthread -lm -o build/review-asan
./build/review-asan
xvfb-run -a python tests/test_gui_native.py --cc cc
```

La ejecución local documentada es Linux x86_64. Headless, compilación o pruebas
de interoperabilidad no sustituyen validación nativa de Windows/macOS/X11.

### Resultado de esta ejecución

Linux x86_64, TinyCC/libtcc y GCC: **37/37 etapas** de la suite completa,
con **1.200 comprobaciones de lenguaje** y gRPC/Protobuf interoperables en ambos
backends. Banco de módulos con ASan/UBSan: **22/22 etapas**. También pasan las
regresiones sanitizadas de aliasing/GUI/I/O, historial UTF-8 y timeout/cancelación
/recolección de procesos. El dashboard GUI experimental ejecuta en headless.

Informes locales: `build/test-report.json`, `build/review-sanitizers/report.json`
y `build/review-sanitizers/report.xml`. No se ejecutaron aquí ventanas nativas
X11, Windows o Cocoa; sus pruebas y dependencias quedaron configuradas en CI.
