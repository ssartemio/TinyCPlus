# Mediciones locales

Wall time includes startup and shutdown; Windows peak working set includes compiler process, not external compiler children. No baseline comparison or allocation instrumentation.

Host: Windows-10-10.0.19044-SP0; backend: bundled libtcc; repeticiones: 7.

| Programa | Compilar (mediana ms) | Ejecutar (mediana ms) | Ejecutable bytes | RAM máxima compilador bytes |
|---|---:|---:|---:|---:|
| arithmetic | 12.523 | 11.209 | 5120 | 4538368 |
| fused_stream | 13.115 | 9.2 | 8192 | 4730880 |
| interface | 12.0 | 14.444 | 5632 | 4562944 |
| owned_closure | 12.424 | 14.215 | 5632 | 4546560 |
| allocation | 12.138 | 12.051 | 5120 | 4550656 |
| async_tasks | 29.676 | 194.639 | 22528 | 11329536 |
