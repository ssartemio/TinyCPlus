# Instaladores nativos de TinyC+

Este proyecto genera **tres paquetes nativos** desde builds realizados en cada sistema:

| Plataforma | Archivo | Ubicación |
|---|---|---|
| Windows x64 | Instalador Inno Setup .exe | Carpeta del usuario: LocalAppData/Programs/TinyCPlus |
| Debian/Ubuntu amd64 | .deb | /usr/lib/tinycplus |
| Fedora x86_64 | .rpm | /usr/lib/tinycplus |

El instalador Windows ofrece añadir la carpeta bin al PATH del usuario (opcional). Los paquetes Linux crean enlaces en /usr/bin para los comandos tiny, tinyc y tinyedit.

## Obtener los instaladores

En GitHub vaya a **Actions → TinyC+ native installers → ejecución exitosa → Artifacts**. Aparecerán los artefactos:

- tinycplus-windows-x64-installer
- tinycplus-debian-amd64
- tinycplus-fedora-x86_64

Cada artefacto de Actions contiene el instalador nativo correspondiente.

## Instalar

### Windows

Ejecute el .exe, opcionalmente active **Agregar al PATH**, abra una terminal nueva y pruebe:

~~~powershell
tiny --version
tiny run "$env:LOCALAPPDATA\Programs\TinyCPlus\examples\hello.tc"
~~~

El instalador es por usuario y no pide elevación. La desinstalación elimina la entrada PATH únicamente si fue añadida por el instalador.

### Debian y Ubuntu

~~~bash
sudo apt install ./tinycplus_1.0.0~rc.1_amd64.deb
tiny --version
tiny run /usr/lib/tinycplus/examples/hello.tc
sudo apt remove tinycplus
~~~

### Fedora

~~~bash
sudo dnf install ./tinycplus-*.x86_64.rpm
tiny --version
tiny run /usr/lib/tinycplus/examples/hello.tc
sudo dnf remove tinycplus
~~~

Los archivos Fedora incluyen el sufijo de distribución, por ejemplo .fc42.

## Construcción local desde el código fuente

Primero instale GCC, make, Python 3.12 y, según el sistema:
- Windows: Inno Setup 6, con ISCC.exe disponible;
- Debian/Ubuntu: dpkg-deb;
- Fedora: rpm-build.

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

Los paquetes incluyen avisos de terceros, fuentes redistribuibles y THIRD_PARTY.md. La distribución no depende de Python, aunque las herramientas de desarrollo sí lo utilizan.

## Estado y licencias

**El repositorio principal todavía no declara una licencia general para su propio código.** Los metadatos RPM lo señalan con LicenseRef-TinyCPlus-Undeclared en lugar de atribuir una licencia que no ha sido concedida. Antes de ofrecer estos binarios como release oficial, el propietario del proyecto debe escoger una licencia, revisar los avisos por archivo de TinyCC, definir la política de distribución y, de ser posible, firmar Windows con Authenticode y RPM con GPG.

Estos instaladores son artefactos de prueba de GitHub Actions, no paquetes aceptados por los repositorios oficiales de Debian o Fedora.

## Diagnóstico

Si tiny --version funciona pero tiny run falla, revise std/, runtime/ y third_party/ dentro de la carpeta instalada. Para usar repl en Linux debe existir libtcc.so en third_party/tcc/posix. Si un paquete no puede instalarse en una distribución más antigua, reconstruya en la distribución más antigua compatible; el glibc mínimo no se garantiza entre sistemas.
