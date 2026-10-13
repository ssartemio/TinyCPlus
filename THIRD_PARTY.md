# Third-party components

TinyC+ original code and documentation are licensed under [Apache-2.0](LICENSE),
unless a file explicitly states otherwise. See [NOTICE](NOTICE) for project
attribution. This license does not replace the licenses of the components under
`third_party/`; their original license texts and per-file notices remain in force.

| Component | Version | Location | License reference |
|---|---|---|---|
| TinyCC / libtcc | 0.9.28rc, commit 0fb54300b56512754221d80adda85ddb9815bceb | third_party/tcc | [COPYING](third_party/tcc/COPYING), per-file notices in source archive |
| nghttp2 | 1.70.0 | third_party/nghttp2 | [COPYING](third_party/nghttp2/COPYING), MIT notice |

Original source archives are included. Their checksums, upstream URLs and exact
revisions are in [dependencies.json](third_party/dependencies.json).
`python tools/bootstrap.py --verify-only` verifies both archives.

On Linux and macOS, `tools/bootstrap.py` builds TinyCC from the same archive into
the untracked directory `third_party/tcc/posix/`; the committed Windows package is
left untouched.

TinyCC's compiler core is unmodified. The Windows package includes its runtime
support objects, SDK headers and import definitions. The original archive keeps
the notices associated with those files. libtcc is dynamically loaded and can
be replaced by rebuilding the included source; the frontend can also use an
external C compiler via `--cc`.

nghttp2 is compiled from source into applications that import std.grpc. Its only
local source adaptation substitutes portable inline byte swaps under TinyCC on
Windows. The exact diff is [tinycc-byteswap.patch](third_party/nghttp2/tinycc-byteswap.patch).
Configured headers are reproduced by tools/bootstrap.py.

Windows installers built with MSYS2 GCC also contain MinGW-w64 CRT and GCC
runtime code linked into the executables. Their notices are copied from the
same compiler installation into `third_party/mingw-runtime/` in the installer:
`COPYING.MinGW-w64-runtime.txt`, `COPYING3`, and `COPYING.RUNTIME`. The CRT
retains the per-component terms in its notice; GCC runtime code is covered by
GPLv3 with the GCC Runtime Library Exception 3.1. The exception permits the
eligible combined executable to retain its own license. The installer does
not distribute separate GCC or winpthreads runtime DLLs; CI checks the binary
imports and exercises the installation without the build toolchain on PATH.
When packaging a GCC build locally, run the staging command in the same MSYS2
environment used to compile it, so the matching runtime notices are included.
See [MinGW-w64 runtime notices](https://github.com/mingw-w64/mingw-w64/blob/master/COPYING.MinGW-w64/COPYING.MinGW-w64-runtime.txt)
and the [GCC Runtime Library Exception](https://www.gnu.org/licenses/gcc-exception-3.1.html).

The test-only Python packages grpcio, grpcio-tools and protobuf are specified in
tests/requirements.txt and are not bundled in the application package. Their
installed distributions supply their license notices. Python and clang-format
are development tools, not compiler/runtime dependencies.

## Package license reference

RPM metadata uses `Apache-2.0 AND LicenseRef-TinyCPlus-Bundled-Third-Party`.
Apache-2.0 applies to TinyC+'s own code. The custom reference denotes the
original licenses and per-file notices of the bundled dependencies, including
their binaries, headers, runtime objects and complete source archives. It is
a reference to those existing terms, not a new license or a relicensing grant.

The authoritative notices are preserved in `third_party/tcc/COPYING`, the
files inside `third_party/tcc/tinycc-source.zip`, `third_party/nghttp2/COPYING`,
and the files inside `third_party/nghttp2/nghttp2-1.70.0.zip`. Exact revisions
and archive checksums are recorded in `third_party/dependencies.json` in the
source repository. TinyCC includes LGPL/GPL code, file-specific exceptions,
MIT/BSD notices and public-domain material. The nghttp2 source archive also
contains ancillary files under terms other than its core MIT license.
Keep these upstream notices and source archives when redistributing the
bundled toolchain. This reference does not assert Fedora repository approval.

Upstream: [TinyCC](https://bellard.org/tcc/),
[nghttp2 1.70.0](https://github.com/nghttp2/nghttp2/releases/tag/v1.70.0).
