# Testing, CI y calidad

TinyC+ usa pruebas por capas para evitar que un bug de lowering se confunda con
un bug del backend.

## Niveles conceptuales

```text
lexer
parser / AST
semantic
lowering
runtime
ejecución nativa
interoperabilidad externa
fuzzing
sanitizers
multiplataforma
```

## Suite completa

```bash
python -m pip install -r tests/requirements.txt
python tests/run_all.py --cc gcc --fuzz 1000
```

El informe se escribe en:

```text
build/test-report.json
```

## Comparación con compilador externo

Una idea central del proyecto es compilar el C generado tanto con TinyCC como
con GCC/Clang.

Si GCC/Clang producen el comportamiento esperado y TinyCC no, el problema puede
estar en el backend.

Si todos producen el mismo comportamiento incorrecto, el lowering es un
sospechoso natural.

## Sanitizers

CI ejecuta builds con AddressSanitizer y UndefinedBehaviorSanitizer.

Esto es especialmente importante porque tanto el compilador como el runtime
están escritos en C y TinyC+ usa memoria manual.

## Fuzzing

El frontend recibe inputs deterministas válidos, inválidos y aleatorios.

El objetivo mínimo ante fuente incorrecta es:

```text
no crash
no hang
diagnóstico
código de salida coherente
```

## Interoperabilidad

Protobuf se compara contra la biblioteca oficial.

gRPC se prueba contra grpcio en ambas direcciones.

Esto evita el sesgo de verificar dos implementaciones que compartan el mismo
error.

## Matriz 1.0 RC

La release qualification incluye:

```text
Windows x64 / GCC
Ubuntu x86-64 / GCC
Ubuntu ARM64 / GCC
macOS ARM64 / Clang
Ubuntu / Clang + ASan/UBSan
```

Los jobs también reconstruyen el frontend con TinyCC donde corresponde y
repiten fuzzing.

## Qué significa “verde”

CI verde no significa “cero bugs”. Significa que el contrato probado sigue
cumpliéndose en la matriz conocida.

Cada nueva feature debe ampliar la evidencia, no únicamente pasar la suite vieja.
