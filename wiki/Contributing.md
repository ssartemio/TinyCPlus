# Contribuir a TinyC+

TinyC+ valora más una implementación pequeña y demostrable que una feature
ambiciosa difícil de razonar.

## Antes de escribir código

Defina:

```text
problema
sintaxis/API mínima
semántica
lowering o runtime
ownership
coste
errores
tests
```

## Dónde debería vivir una feature

Prefiera, en este orden conceptual:

```text
stdlib
runtime pequeño
lowering/frontend
backend TCC
```

No modifique TinyCC si la feature puede expresarse como C generado.

## Reglas de implementación

Mantenga el frontend C11.

Mantenga el C generado legible.

Evite dependencias nuevas en el core salvo una necesidad clara.

No introduzca ownership implícito.

Añada tests positivos y negativos.

Si cambia el lowering, inspeccione el C generado.

Si toca memoria, ejecute sanitizers.

Si toca plataforma, ejecute la matriz que corresponda.

## Verificación local

```bash
python tools/bootstrap.py --cc cc
python build.py --cc cc
python -m pip install -r tests/requirements.txt
python tests/run_all.py --cc cc --fuzz 1000
```

## Commits

Prefiera commits con una intención clara:

```text
feat(parser): ...
fix(async): ...
test(gui): ...
docs(wiki): ...
refactor(input): ...
```

## Pull requests

Un buen PR explica:

- qué problema resuelve;
- qué cambia;
- qué no cambia;
- ownership/costes relevantes;
- pruebas ejecutadas;
- plataformas verificadas;
- límites conocidos.

## Compatibilidad

No trate `main` como un experimento permanente. Las features post-1.0 pueden
desarrollarse en ramas separadas hasta que su contrato y pruebas estén claros.

La GUI gráfica actual es un ejemplo de este patrón.
