# Capítulo 13 — solución experimental

`13_gui_dashboard.tc` implementa el proyecto final con `std.gui`.

La base de `std.gui` y los widgets portables están integrados de forma experimental.
Puede ejecutarse con `tiny run examples/tutorial/experimental/13_gui_dashboard.tc`.
No implica que la GUI sea estable ni que se hayan validado ventanas nativas en
cada plataforma; consulte `docs/GUI.md` y `docs/FUNCTIONAL_IMPROVEMENTS.md`.

El ejemplo usa `headless:true` para que pueda verificarse sin una ventana real;
cambiarlo a `false` ejercita el backend nativo disponible en la plataforma.
