# Diagnósticos del frontend

Los errores conservan su primera línea `archivo:línea:columna: error: mensaje` y
ahora añaden la línea de fuente y un subrayado. Esto permite seguir usando las
ubicaciones desde editores y scripts existentes. El fragmento proviene del texto
leído por la compilación, incluidos módulos importados; no se vuelve a abrir el
archivo para construir el diagnóstico.

El marcador `^` señala la ubicación del error y `~` extiende el token cuando puede
identificarse un nombre o literal en esa ubicación. Los errores de tipo en un
retorno señalan su expresión. Las líneas largas se muestran mediante una ventana
acotada con `...`. Los tabs se expanden y los controles se representan con escapes.
Los números de columna del lexer siguen contando bytes UTF-8; el dibujo evita
contar bytes de continuación como caracteres adicionales. No calcula el ancho
visual de todos los glifos Unicode, como caracteres de ancho doble o combinaciones.

## Contexto de genéricos

Una declaración instanciada conserva el lugar que solicitó el tipo o función
concretos. Si falla su análisis, el diagnóstico añade notas desde la instancia
más cercana hasta sus solicitantes, por ejemplo:

```text
error: expected i32, found string
...
note: while instantiating 'Inner<i32>'
...
note: while instantiating 'Outer<i32>'
```

Las notas incluyen ubicaciones y fragmentos, de modo que se puede localizar tanto
la definición que falla como la solicitud concreta. El contexto cubre clases,
funciones y tipos dependientes sustituidos durante la monomorfización. Las
instancias se comparten internamente: cuando hay varias solicitudes del mismo tipo,
se conserva su primer origen registrado, no todas sus llamadas. La salida se limita
a 16 niveles y avisa si omite contexto adicional.

Este contexto pertenece al frontend. Los errores de un compilador C externo, el
linker o el runtime conservan sus propios formatos; la emisión C mantiene los
`#line` existentes. El modo silencioso usado por herramientas internas también
suprime fragmentos y notas.

## Verificar

```sh
python3 tests/test_diagnostics.py
```

Las pruebas comprueban ubicaciones y alineación, tabs, CRLF, BOM, texto UTF-8,
controles, líneas largas, imports, fallos de protocolos normales sin notas espurias
y cadenas de genéricos anidados y funciones genéricas. Los diagnósticos también se
comprueban dentro de la suite general del compilador.
