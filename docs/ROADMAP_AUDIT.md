# TinyC+ 1.0.0-rc.1 — Roadmap audit

Date: 2026-09-18
Baseline: `main` at `7b3fd2e9954f5cf657839f96e1ca57686e3e0629`

## Purpose

This document compares the implementation in `main` against the original TinyC+ roadmap and separates:

- implemented and locally exercised;
- implemented but not yet release-qualified on all intended platforms;
- intentionally deferred;
- future work after 1.0.

The goal is to avoid continuing from the historical 0.0.x bootstrap line when `main` is already substantially more advanced.

## Result

The functional roadmap through 0.10 is present in the repository. The project should now prioritize **release qualification and hardening**, not additional language features.

| Roadmap area | Status | Evidence / notes |
|---|---|---|
| 0.0 bootstrap/compiler | Implemented | C11 frontend, AST, diagnostics, C lowering, libtcc/external backend |
| 0.1 core language | Implemented | variables, control flow, pointers, manual memory, defer, strings, arrays/slices, tuples/Error |
| 0.2 object model | Implemented | classes, constructor/destructor, properties, extensions, structural interfaces, named/default args |
| 0.3 generics/FFI | Implemented | monomorphization, collections, C ABI FFI |
| 0.4 closures/streams/tooling | Implemented | borrowed/owned closures, range, fused streams, fmt/doc/REPL/test tooling |
| 0.5 threads | Implemented | Thread, Mutex, Condition, Semaphore, Event, Atomic |
| 0.6 tasks/channels | Implemented | WorkerPool, Task/Future, spawn, Channel, cancellation |
| 0.7 async/await | Implemented | state-machine lowering, async file/network paths, TaskGroup |
| 0.8 networking | Implemented | TCP/UDP/DNS/reactor/timers, HTTP/2 via nghttp2 |
| 0.9 protobuf/gRPC | Implemented with scoped limits | proto3 subset + unary gRPC over h2c |
| 0.10 TUI/editor | Implemented | terminal UI, widgets/layout/events, gap-buffer TinyEdit |
| 1.0 release qualification | Incomplete | Windows x64 evidence exists; Linux/macOS/ARM64/sanitizers still need successful recorded runs |
| 1.x graphical GUI | Not implemented | intentionally future work |

## Release blockers for 1.0 stable

The repository itself documents the following qualification gaps:

1. Linux x86-64 validation has not been recorded.
2. Linux ARM64 validation has not been recorded.
3. macOS ARM64 validation has not been recorded.
4. Clang frontend/runtime validation has not been recorded.
5. ASan/UBSan runs have not been recorded.
6. The CI workflow exists, but no successful workflow run is attached to the current `main` commit.

Until those are closed, the correct version label remains `1.0.0-rc.1`.

## Important implementation limits that are acceptable for 1.0 if documented

These are not necessarily blockers, but they must remain explicit contracts:

- manual memory management;
- owned values can be aliased incorrectly by copying;
- no automatic destruction of elements stored in generic containers;
- no borrow checker/lifetime checker for borrowed closures;
- class/enum names remain globally unique across modules;
- no cross-compilation interface;
- no validated 32-bit ABI;
- Atomic is portable and sequentially consistent, but mutex-backed rather than lock-free;
- no channel/select construct;
- TaskGroup cancellation is cooperative;
- DNS cancellation cannot interrupt an in-progress system resolver call;
- gRPC is unary h2c only;
- no gRPC TLS, streaming, auth, metadata, reflection, compression or retry;
- TUI Unicode width is approximate and does not implement full grapheme shaping;
- TinyEdit lacks undo/redo, syntax highlighting and multiple documents;
- no graphical GUI yet.

## Post-1.0 priorities

After release qualification, recommended order:

### P1 — Graphical GUI foundation

Build the graphical UI promised for the later roadmap without changing the language core:

- `Surface`
- `Pixel`
- `Canvas`
- framebuffer/window backend abstraction
- keyboard/mouse event bridge
- `Window`
- reusable `Row` / `Column` layout concepts from TinyUI
- `Label`, `Button`, `TextBox`
- double buffering and damage tracking

The TUI widget/event model should be reused where sensible rather than replaced.

### P2 — Concurrency completion

Add a `select`-style primitive over channels/timers/futures only after API semantics are specified and tested.

### P3 — gRPC production features

Incrementally add:

1. TLS;
2. application metadata;
3. streaming RPC;
4. authentication hooks;
5. reflection;
6. compression/retry only when justified.

### P4 — Module/type namespace cleanup

Remove the global-uniqueness requirement for classes/enums across modules.

### P5 — Ownership diagnostics

Keep memory manual, but add optional compile-time diagnostics for obvious owner-copy/double-destroy patterns without introducing ARC, GC or a Rust-style borrow checker.

## Immediate next action

Run the existing CI matrix unchanged first.

Do **not** add new language features until the current workflow is green on:

- Ubuntu x86-64 + GCC;
- Ubuntu ARM64 + GCC;
- macOS ARM64 + Clang;
- Windows x64 + GCC;
- Ubuntu x86-64 + Clang ASan/UBSan.

If a platform fails, fix portability/reproducibility issues with the smallest possible change and add a regression test.

## Release gate

Promote from `1.0.0-rc.1` to `1.0.0` only when:

- all CI jobs pass on the same commit;
- validation evidence is recorded in `docs/validation`;
- `docs/STATUS.md` is updated from pending to verified for those platforms;
- no sanitizer findings remain;
- the documented scoped limitations remain accurate.

