# TinyC+ bootstrap 0.0.5 — archival checkpoint

This directory preserves the ChatGPT-assisted bootstrap checkpoint `0.0.5` created during the incremental TinyC+ design sessions.

It is **historical** and intentionally separate from `main`, which is already at `1.0.0-rc.1` and is substantially more advanced.

Contents:

- `tinycplus-bootstrap-0.0.5.zip` — complete source snapshot for the 0.0.5 checkpoint.
- The archive includes compiler sources, runtime, examples, tests, implementation notes, lowering notes, and the Codex specification used for that bootstrap line.

Checkpoint scope includes the procedural core from 0.0.1–0.0.4 plus the first lightweight object model: classes by value, fields, methods, `this`, constructors, named constructor arguments, and classes imported across modules. It deliberately predates destructor/heap-object lifecycle, properties, extensions, structural interfaces, async, gRPC, and UI work.

Do not merge this snapshot over `main` as a replacement implementation. Keep it for history, regression comparison, and design archaeology.
