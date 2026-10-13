# Contribuir a TinyC+

TinyC+ valora más una implementación pequeña y demostrable que una feature
ambiciosa difícil de razonar.

## Licencia de las contribuciones

El código y la documentación propios del proyecto usan
[Apache-2.0](../LICENSE). Salvo indicación expresa en contrario, las contribuciones
enviadas intencionalmente para su inclusión se ofrecen bajo esa misma licencia,
según su sección 5. Aporte únicamente material que tenga derecho a contribuir.

Conserve los avisos de copyright y licencia existentes. En nuevos archivos de
código propio, añada `SPDX-License-Identifier: Apache-2.0` en un comentario.
Los componentes de `third_party/` conservan sus licencias: no les añada la cabecera
Apache del proyecto. Documente cualquier nueva dependencia y sus avisos en
[THIRD_PARTY.md](../THIRD_PARTY.md).

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
