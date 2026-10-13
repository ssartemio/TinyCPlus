# Instaladores nativos de TinyC+

TinyC+ 1.0.0 genera **tres paquetes nativos** desde builds realizados en cada sistema:

| Plataforma | Archivo | Ubicación |
|---|---|---|
| Windows x64 | Instalador Inno Setup .exe | Carpeta del usuario: LocalAppData/Programs/TinyCPlus |
| Ubuntu 24.04 amd64 | .deb | /usr/lib/tinycplus |
| Fedora 42 x86_64 | .rpm | /usr/lib/tinycplus |

El instalador Windows ofrece añadir la carpeta bin al PATH del usuario (opcional). Los paquetes Linux crean enlaces en /usr/bin para los comandos tiny, tinyc y tinyedit.

Se requiere un sistema de 64 bits y una terminal con soporte ANSI para TinyEdit.
Los paquetes Linux contienen TinyCC/libtcc, pero necesitan las cabeceras,
bibliotecas y objetos de inicio C del sistema: `libc6-dev` en Ubuntu y
`glibc-devel` en Fedora. El gestor de paquetes instala esas dependencias.

El `.deb` se construye para Ubuntu 24.04; sus dependencias expresan el mínimo
de glibc requerido por los binarios. No se garantiza que funcione en versiones
anteriores de Ubuntu o en cualquier Debian solo por usar el mismo formato.
El RPM de Fedora 42 tampoco se anuncia para otras distribuciones RPM. Para
otros sistemas o arquitecturas, use las fuentes y compile en el sistema destino.
Ubuntu ARM64 y macOS ARM64 tienen pruebas desde fuentes, pero no instaladores
nativos en esta entrega.

## Obtener los instaladores

Para una versión publicada, abra
[GitHub Releases](https://github.com/ssartemio/TinyCPlus/releases) y descargue el
`.exe`, `.deb` o `.rpm` correspondiente, junto con `SHA256SUMS.txt`. También se
publica un archivo de fuentes para reconstruir en otros sistemas. Consulte las
[notas de 1.0.0](releases/1.0.0.md) para conocer el alcance.

Compruebe la suma del archivo antes de instalar. En PowerShell:

~~~powershell
Get-FileHash .\TinyCPlus-1.0.0-windows-x64.exe -Algorithm SHA256
~~~

Compare el resultado con la línea del mismo nombre en `SHA256SUMS.txt`. En Linux,
para comprobar solo los archivos que descargó:

~~~bash
sha256sum --check --ignore-missing SHA256SUMS.txt
~~~

Los builds de desarrollo también están disponibles en **Actions → TinyC+ native
installers → ejecución exitosa → Artifacts**, con estos nombres:

- tinycplus-windows-x64-installer
- tinycplus-debian-amd64
- tinycplus-fedora-x86_64

Cada artefacto de Actions contiene el instalador nativo correspondiente; un
artefacto de una rama de desarrollo no equivale a una versión publicada.

## Instalar

### Windows

Ejecute el .exe, opcionalmente active **Agregar al PATH**, abra una terminal nueva y pruebe:

~~~powershell
tiny --version
tiny run "$env:LOCALAPPDATA\Programs\TinyCPlus\examples\hello.tc"
~~~

También puede abrir **TinyC+ console** desde el menú Inicio. Esa consola empieza
en la carpeta instalada y habilita los comandos aunque no haya elegido agregar
TinyC+ al PATH. Ejecute `tiny run examples\hello.tc`; el ejemplo imprime `30`.
Las rutas relativas se buscan desde la carpeta actual de la terminal. En una
terminal abierta en otra carpeta, use la ruta completa del ejemplo como arriba.

El instalador es por usuario y no pide elevación. La desinstalación elimina la entrada PATH únicamente si fue añadida por el instalador.

#### Crear su primer programa

Abra **TinyC+ console** desde Inicio. Cree una carpeta propia y abra un archivo
nuevo (estos comandos corresponden a esa consola de Windows):

~~~bat
mkdir "%USERPROFILE%\Mis programas TinyCPlus"
cd /d "%USERPROFILE%\Mis programas TinyCPlus"
tinyedit hola.tc
~~~

Si la carpeta ya existe, omita `mkdir`. También puede usar otra carpeta donde
pueda guardar archivos; conserve las comillas si la ruta contiene espacios.
Escriba este programa:

~~~c
int main() {
    println("Hola mundo");
    return 0;
}
~~~

Pulse **Ctrl-S** para guardar y **Ctrl-Q** para salir del editor. TinyEdit edita
y guarda el archivo; para ejecutarlo, escriba en la consola:

~~~bat
tiny run hola.tc
~~~

La salida será `Hola mundo`. Si utiliza otro editor, guarde el archivo como
`hola.tc`, con codificación **UTF-8** y sin añadir `.txt` al nombre.

Para crear un ejecutable y abrirlo desde esta misma consola:

~~~bat
tiny build hola.tc -o hola.exe
.\hola.exe
~~~

### Ubuntu 24.04

~~~bash
sudo apt install ./tinycplus_1.0.0_amd64.deb
tiny --version
tiny run /usr/lib/tinycplus/examples/hello.tc
sudo apt remove tinycplus
~~~

Use `apt install` con el archivo local para resolver dependencias. Un `dpkg -i`
aislado puede dejar el paquete sin configurar si faltan las cabeceras de libc.

### Fedora 42

~~~bash
sudo dnf install ./tinycplus-*.x86_64.rpm
tiny --version
tiny run /usr/lib/tinycplus/examples/hello.tc
sudo dnf remove tinycplus
~~~

Los archivos Fedora incluyen el sufijo de distribución, por ejemplo .fc42.

En ambos sistemas Linux también puede crear un archivo propio `hola.tc` con el
programa mostrado arriba y ejecutar `tiny run hola.tc`, o generar un ejecutable
con `tiny build hola.tc -o hola` y abrirlo con `./hola`.

## Construcción local desde el código fuente

Primero instale GCC, make, Python 3.12, las cabeceras y bibliotecas de desarrollo
del sistema y, según el formato:
- Windows: Inno Setup 6, con ISCC.exe disponible;
- Debian/Ubuntu: dpkg-dev y patchelf;
- Fedora: rpm-build y patchelf.

Windows:

~~~powershell
python tools/bootstrap.py --cc gcc
python build.py --cc gcc
.\bin\tiny.exe build apps\tinyedit.tc -o bin\tinyedit.exe --cc gcc
python tools/build_installers.py stage --target windows
python tools/build_installers.py windows
~~~

Debian o Fedora:

~~~bash
python3 tools/bootstrap.py --cc gcc
python3 build.py --cc gcc
./bin/tiny build apps/tinyedit.tc -o bin/tinyedit --cc gcc
python3 tools/build_installers.py stage --target linux
python3 tools/build_installers.py deb    # Debian
python3 tools/build_installers.py rpm    # Fedora
~~~

Los paquetes terminados se escriben en dist/. La salida temporal está en build/installers/ y ambos directorios son ignorados por Git.

## Estructura preservada

Los instaladores incluyen bin/, std/, runtime/, ejemplos, las dependencias TinyCC/libtcc de la plataforma y el código necesario de nghttp2 para compilar programas gRPC. El compilador calcula su raíz desde la ruta del ejecutable; en Linux los enlaces /usr/bin apuntan al bin/ real para que /proc/self/exe encuentre /usr/lib/tinycplus. No es necesario fijar TINY_HOME.

Los paquetes incluyen LICENSE (Apache-2.0 para el código propio), NOTICE, avisos de terceros, fuentes redistribuibles y THIRD_PARTY.md. La distribución no depende de Python, aunque las herramientas de desarrollo sí lo utilizan.

## Estado y licencias

El código propio de TinyC+ se distribuye bajo **Apache License 2.0**, con los
avisos de atribución de NOTICE. Las dependencias conservan sus licencias:
TinyCC/libtcc y sus componentes según COPYING y sus avisos por archivo, y
nghttp2 bajo MIT. Consulte THIRD_PARTY.md; Apache-2.0 no sustituye esos términos.
Los instaladores 1.0.0 se distribuyen **sin firma Authenticode/GPG**. Las sumas
SHA-256 permiten comprobar que la descarga coincide con el archivo publicado;
no sustituyen una firma del editor. Windows puede mostrar un aviso de editor
desconocido. Estos son paquetes del proyecto y no paquetes aceptados por los
repositorios oficiales de Debian, Ubuntu o Fedora.

## Diagnóstico

Si `tiny --version` funciona pero `tiny run` falla, revise `std/`, `runtime/` y
`third_party/` dentro de la carpeta instalada. Para usar `repl` en Linux debe
existir `libtcc.so` en `third_party/tcc/posix`. Un error sobre `stdio.h`,
`stdint.h`, `crt1.o` o `crti.o` suele indicar que faltan archivos de desarrollo
de libc: reinstale el paquete con `apt` o `dnf` y permita resolver sus
dependencias. No copie solo los ejecutables ni fuerce una instalación que
rechace la versión de glibc; reconstruya desde fuentes en el sistema destino.
