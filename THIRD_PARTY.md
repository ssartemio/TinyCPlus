# Third-party components

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

The test-only Python packages grpcio, grpcio-tools and protobuf are specified in
tests/requirements.txt and are not bundled in the application package. Their
installed distributions supply their license notices. Python and clang-format
are development tools, not compiler/runtime dependencies.

Upstream: [TinyCC](https://bellard.org/tcc/),
[nghttp2 1.70.0](https://github.com/nghttp2/nghttp2/releases/tag/v1.70.0).
