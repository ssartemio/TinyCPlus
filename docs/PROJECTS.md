# Proyectos con tiny.project

El frontend C11 lee `tiny.project` directamente: no requiere Python ni ejecuta
un shell para interpretar sus valores. Desde la raíz de un proyecto:

```sh
tiny build
tiny run
tiny check
tiny test
```

Sin un archivo fuente explícito, estos comandos buscan `tiny.project` en el
directorio actual. No buscan automáticamente en carpetas superiores. Para trabajar
desde otro directorio, use `tiny build --project ruta/tiny.project`. Los comandos
con un archivo explícito siguen funcionando igual; no cargan un manifiesto implícito.
`--project` no se combina con un archivo fuente, `--batch`, `fmt`, `doc` ni `repl`.
`tiny` sin argumentos sigue mostrando la ayuda.

## Formato

Es un formato propio de pares `clave = valor`, uno por línea; no es TOML ni JSON.
Las claves distinguen mayúsculas. Las listas se expresan repitiendo la clave:

```text
entry = src/main.tc
output = build/demo
module_path = modules
module_path = vendor
c_source = native/support.c
include = "native/include files"
library_path = native/lib
library = example
cc = cc
test = tests/unit.tc
test = tests/integration.tc
```

Las líneas vacías y los comentarios `#` se ignoran. Se admiten comentarios después
del valor. Entre comillas dobles, `#` forma parte del valor y los únicos escapes son
`\\` y `\"`. Use `/` en las rutas también en Windows para evitar escapes. Los
valores sin comillas abarcan hasta `#`, quitando espacios al principio y al final.
No se admiten valores vacíos, expansión de variables, comandos, globbing ni arrays.
Las claves desconocidas, escalares duplicados y sintaxis incorrecta son errores.

| Clave | Uso |
| --- | --- |
| `entry` | Archivo `.tc` de entrada; obligatorio para build/run/check. |
| `output` | Ejecutable de salida. Si se omite, build usa `build/app` o `build/app.exe` según el host. |
| `cc` | Nombre del compilador C en PATH, o ruta a su ejecutable. Si se omite se conserva el backend habitual de libtcc. |
| `module_path` | Directorio adicional de módulos TinyC+; repetible, hasta 32. |
| `c_source` | Fuente C u objeto para enlazar; repetible, hasta 32. |
| `include` | Directorio de headers C, equivalente a `-I`; repetible, hasta 32. |
| `library_path` | Directorio de bibliotecas C, equivalente a `-L`; repetible, hasta 32. |
| `library` | Nombre de biblioteca para `-l`, sin prefijo ni extensión; repetible, hasta 32. |
| `test` | Archivo con declaraciones `@test`; repetible, hasta 32. No es un patrón de búsqueda. |

Las rutas son relativas a la carpeta del manifiesto, con independencia del lugar
desde el que se invoque `tiny`. Las rutas absolutas se conservan. En `cc`, un nombre
sin separadores se resuelve mediante PATH; una ruta se resuelve desde el manifiesto.
Si se especifica `output` en Windows, incluya `.exe` cuando corresponda.

## Imports y opciones

Un import de usuario busca primero junto a la fuente que lo declara, luego en los
`module_path` en orden. Por ejemplo, `import util.math;` busca `util/math.tc`.
Los imports `std.*` siguen usando la instalación indicada por `--home`/`TINY_HOME`;
los directorios de usuario no sustituyen la biblioteca estándar.

`--module-path DIR` también funciona con el comando tradicional y puede repetirse:

```sh
tiny run src/main.tc --module-path modules --c-source native/support.c -I include
```

Los argumentos `--cc` y `-o` de la CLI prevalecen sobre el manifiesto. Las rutas
suministradas por CLI son relativas al directorio actual, como en los comandos
existentes. Las opciones de lista de la CLI se añaden antes que las del manifiesto;
los límites de 32 incluyen ambos orígenes. No escriba flags dentro de `cc`: es una
ruta de ejecutable, no un fragmento de shell.

`build` crea los directorios padres de la salida. `run` no necesita producir un
artefacto persistente. `tiny run -- argumento` pasa los argumentos a la aplicación.
Los directorios de trabajo de la aplicación y de las pruebas siguen siendo el
directorio de invocación: las rutas de datos que usa el programa no se reinterpretan.

## Pruebas y resultados

`tiny test` compila y ejecuta cada archivo configurado en `test` como unidad
independiente con los mismos imports y opciones de enlace. No ejecuta `main` como
sustituto de las declaraciones `@test`. El manifiesto sin entradas `test` produce un
error; configure archivos que contengan pruebas reales.

Una compilación o prueba fallida conserva un resultado distinto de cero y no oculta
los archivos posteriores que puedan ejecutarse. Una terminación del proceso, como
un panic de runtime en el backend en memoria, puede interrumpir la invocación antes
de continuar. Los códigos habituales son 0 para éxito, 1 para errores de compilación
o pruebas, 2 para manifiesto/uso inválido y 101 para asserts del runtime. No se añade
un gestor de paquetes ni una resolución de dependencias remotas.

## Proyecto de ejemplo

[examples/project/tiny.project](../examples/project/tiny.project) integra un módulo,
una fuente C con su header, una copia de Array y dos pruebas:

```sh
# Desde la raíz del repositorio
./bin/tiny build --project examples/project/tiny.project
./bin/tiny run --project examples/project/tiny.project
./bin/tiny test --project examples/project/tiny.project
./bin/tiny test --project examples/project/tiny.project --cc cc
```

La aplicación imprime `42`; las pruebas deben informar `2 tests executed; passed`.
También puede entrar en `examples/project` e invocar `../../bin/tiny build` y
`../../bin/tiny test` sin argumentos adicionales. Los outputs por defecto quedan
en el directorio `build` ignorado de ese proyecto.

`tests/test_project.py` verifica ambos backends, rutas con espacios, uso desde otra
carpeta, precedencia de opciones, imports, FFI, creación de salida y fallos de
configuración y pruebas.
