# Distribuciones nativas de Windows y macOS

Los paquetes son kits de desarrollo nativos: compilador `tiny`/`tinyc`, TinyEdit
recién compilado, runtime, módulos, ejemplos, pruebas y fuentes/licencias de sus
dependencias. No se mezclan ejecutables de otra plataforma ni se reutiliza un
TinyEdit antiguo de `bin`. El empaquetador comprueba los headers PE/ELF/Mach-O del
compilador, editor, TinyCC y libtcc antes de nombrar la arquitectura del paquete.

## Crear y comprobar

Desde un checkout en la plataforma/arquitectura de destino:

```sh
python tools/bootstrap.py --cc gcc          # Windows x64; requiere MinGW-w64 GCC
python build.py --cc gcc
python tools/package.py --output-dir build/artifacts
```

En macOS Intel o Apple Silicon se usa `--cc clang` y se requiere instalar las
Command Line Tools con `xcode-select --install`. En Linux se usa GCC/Clang.
No es un proceso de compilación cruzada: Windows x64, macOS x86_64 y macOS arm64
se construyen por separado. Los nombres normalizan AMD64/x64 a x86_64 y aarch64 a
arm64. CI tiene runners macOS Intel y ARM64, Windows x64 y Linux.

macOS genera `.tar.gz`, que conserva permisos ejecutables; Windows genera `.zip`.
También se puede pedir explícitamente cualquiera de los dos formatos:

```sh
python tools/package.py --output build/artifacts/tinycplus.zip
python tests/test_distribution.py --archive build/artifacts/tinycplus.zip
```

Cada archivo contiene `CAPABILITIES.json` y `SHA256SUMS.txt`; a su lado se genera
un hash SHA256 del archivo completo. El manifiesto declara dependencias y alcance
de firma. El paquete completo exige libtcc nativa disponible; se rechaza un kit
incompleto que luego no podría ofrecer el REPL.

## Instalar

Windows 10/11 x64: extraer con el Explorador o `Expand-Archive` y ejecutar
`tinycplus\bin\tiny.exe doctor`. Puede agregarse `tinycplus\bin` a PATH. El uso
habitual con TinyCC incluido no necesita GCC instalado. Los DLL del sistema se
obtienen de Windows; no se distribuyen copias de DLL del sistema. El frontend
incluye un manifiesto UTF-8 y convierte rutas/argv mediante APIs Unicode. Los
programas generados también convierten sus argumentos antes de exponerlos como
vistas UTF-8. Un compilador externo elegido mediante `--cc` conserva sus propias
limitaciones de plataforma y codificación.

macOS: extraer con `tar -xzf archivo.tar.gz`, instalar Command Line Tools y ejecutar
`./tinycplus/bin/tiny doctor`. TinyCC carga su biblioteca desde una ruta relativa
al ejecutable, y TinyC+ consulta `SDKROOT` o `xcrun --show-sdk-path` para usar los
headers/librerías del SDK instalado en el equipo, sin depender únicamente del SDK
que existía en el runner. El renderer Cocoa usa los frameworks del sistema.

Mover la carpeta instalada no exige establecer `TINY_HOME` ni `LIBTCC_PATH`.
El paquete incluye las fuentes TinyCC verificadas y la adaptación de codesign,
así como las fuentes nghttp2 y scripts de reconstrucción.

## Firma y publicación

Son compilaciones para desarrollo. La firma ad hoc de macOS permite ejecutar
binarios locales y no equivale a un Developer ID ni a notarización. Windows no
incluye un certificado de editor. No se afirma que estas compilaciones eviten
Gatekeeper o SmartScreen al descargarse desde Internet. Una distribución pública
firmada requiere las credenciales del editor y, para macOS, su flujo de
notarización; esos secretos no forman parte del repositorio.

CI crea y valida los archivos y los conserva como artifacts; no publica ni
fusiona automáticamente una release. Las pruebas de distribución se ejecutan
fuera del checkout, después de cambiar de nombre la carpeta, en rutas con espacios,
`ñ` y `日`, con PATH reducido y sin overrides del SDK/cargador. Comprueban hashes,
fuentes, cargador TinyCC, compilación/ejecución nativa, argv Unicode, REPL y el
historial/guardado de TinyEdit. La compilación cruzada PE desde Linux comprueba
compilación/enlace, pero no sustituye a ejecutar las pruebas en Windows/macOS.
