# Capítulo 13 — solución experimental

`13_gui_dashboard.tc` implementa el proyecto final con `std.gui`.

No forma parte de la suite estable porque `main` 1.0 RC todavía no contiene
`std.gui`.

Para ejecutarlo use una rama que incluya la fundación GUI y widgets portables,
por ejemplo después de integrar PR #4 y PR #10.

El ejemplo usa `headless:true` para que pueda verificarse sin una ventana real;
cambiarlo a `false` ejercita el backend nativo disponible en la plataforma.
