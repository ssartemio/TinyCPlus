#!/bin/sh
# Copyright (c) 2026 TinyC+ contributors
# SPDX-License-Identifier: Apache-2.0
# Run only in a fresh ubuntu:24.04 or fedora:42 container. Mount the package
# directory and this script, not the checkout or a compiler installation.
# Usage: sh /test_linux_install.sh deb /packages/tinycplus_VERSION_amd64.deb
#        sh /test_linux_install.sh rpm /packages/tinycplus-VERSION.x86_64.rpm
set -eu

fail() { printf '%s\n' "$*" >&2; exit 1; }

[ "$#" -eq 2 ] || fail 'Expected package format and absolute package path'
[ -e /.dockerenv ] || [ -e /run/.containerenv ] || fail 'Use a disposable clean container'
[ "$(id -u)" -eq 0 ] || fail 'Package installation requires root inside the container'
format=$1
package=$2
case "$package" in /*) ;; *) fail 'Package path must be absolute' ;; esac
[ -f "$package" ] || fail "Package does not exist: $package"

unset TINY_HOME LIBTCC_PATH CC CFLAGS CPPFLAGS LDFLAGS LD_LIBRARY_PATH
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH

no_external_compiler() {
    for compiler in gcc cc clang; do
        if command -v "$compiler" >/dev/null 2>&1; then
            fail "Clean-install test must not have an external C compiler: $compiler"
        fi
    done
}
no_external_compiler

case "$format" in
    deb)
        if dpkg-query -W -f='${db:Status-Status}' libc6-dev 2>/dev/null | grep -qx installed; then
            fail 'libc6-dev is already present; this is not a clean installation'
        fi
        dpkg-deb -f "$package" Depends
        dpkg-deb -f "$package" Depends | grep -Eq '(^|, )libc6-dev([ ,(]|$)'
        apt-get update
        # Only request TinyC+; its declared dependencies must supply the headers.
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "$package"
        dpkg-query -W -f='${db:Status-Status}' libc6-dev | grep -qx installed
        ;;
    rpm)
        if rpm -q glibc-devel >/dev/null 2>&1; then
            fail 'glibc-devel is already present; this is not a clean installation'
        fi
        rpm -qp --requires "$package"
        rpm -qp --requires "$package" | grep -Eq '^glibc-devel(\(|[[:space:]]|$)'
        dnf install -y --setopt=install_weak_deps=False "$package"
        rpm -q glibc-devel
        ;;
    *) fail 'Format must be deb or rpm' ;;
esac
no_external_compiler

work=$(mktemp -d '/tmp/tinycplus user program.XXXXXX')
trap 'rm -rf -- "$work"' EXIT HUP INT TERM
# Package installation needs root; running and compiling must work for an
# ordinary account, including access to all installed headers and libraries.
cat > "$work/smoke.sh" <<'SMOKE'
set -eu
fail() { printf '%s\n' "$*" >&2; exit 1; }
[ "$(id -u)" -ne 0 ] || fail 'Runtime smoke tests must run as a normal user'
cd "$TINY_SMOKE_DIR"
installed=/usr/lib/tinycplus
for file in LICENSE NOTICE THIRD_PARTY.md third_party/tcc/COPYING third_party/nghttp2/COPYING; do
    [ -s "$installed/$file" ] || fail "Missing installed license notice: $file"
done
for executable in tiny tinyc tinyedit; do
    [ "$(command -v "$executable")" = "/usr/bin/$executable" ] || fail "Missing command: $executable"
    [ -x "/usr/bin/$executable" ] || fail "Command is not executable: $executable"
done
for binary in "$installed"/bin/tiny "$installed"/bin/tinyc "$installed"/bin/tinyedit \
              "$installed"/third_party/tcc/posix/tcc "$installed"/third_party/tcc/posix/libtcc.so; do
    libraries=$(ldd "$binary")
    printf '%s\n%s\n' "$binary" "$libraries"
    case "$libraries" in *'not found'*) fail "Unresolved installed library: $binary" ;; esac
done

tiny --version
tiny check "$installed/examples/hello.tc"
tinyc check "$installed/examples/hello.tc"
[ "$(tiny run "$installed/examples/hello.tc")" = '30' ] || fail 'Installed example failed'

# A newly authored file outside the installation, in a directory with spaces.
cat > hola.tc <<'PROGRAM'
int main() {
    println("Hola mundo");
    return 0;
}
PROGRAM
tiny check hola.tc
tinyc check hola.tc
[ "$(tiny run hola.tc)" = 'Hola mundo' ] || fail 'New user program did not run'
tiny build hola.tc -o hola
[ -x hola ] || fail 'Build did not create an executable'
[ "$(./hola)" = 'Hola mundo' ] || fail 'New native executable did not run'

repl_output=$(printf 'println("REPL_OK");\n:quit\n' | tiny repl 2>repl.err)
printf '%s\n' "$repl_output"
case "$repl_output" in *REPL_OK*) ;; *) fail 'Installed REPL failed' ;; esac
[ ! -s repl.err ] || { cat repl.err >&2; fail 'Installed REPL reported an error'; }
SMOKE
chown nobody "$work"
TINY_SMOKE_DIR=$work
export TINY_SMOKE_DIR
su -s /bin/sh nobody -c 'exec sh "$TINY_SMOKE_DIR/smoke.sh"'

case "$format" in
    deb) dpkg -r tinycplus ;;
    rpm) dnf remove -y tinycplus ;;
esac
for executable in tiny tinyc tinyedit; do
    [ ! -e "/usr/bin/$executable" ] && [ ! -L "/usr/bin/$executable" ] || fail "Command remains after uninstall: $executable"
done
[ ! -e /usr/lib/tinycplus ] || fail 'Installation directory remains after uninstall'
printf '%s\n' 'Clean Linux installation verified: automatic dependencies, no C compiler, normal user, examples, own program, native build, REPL and uninstall'
