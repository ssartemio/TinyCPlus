#!/usr/bin/env python3
"""Regenerate the API reference from the shipped standard library."""
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
tiny=ROOT/'bin'/('tiny.exe' if os.name=='nt' else 'tiny')
dest=ROOT/'docs/api'
dest.mkdir(parents=True,exist_ok=True)
index=['# API estándar','', 'Referencia generada con `tiny doc` desde las fuentes `.tc`.', '',
       'Las reglas de propiedad y vida útil se explican en LANGUAGE.md y CONCURRENCY.md.', '']
for source in sorted((ROOT/'std').glob('*.tc')):
    target=dest/(source.stem+'.md')
    subprocess.run([str(tiny),'doc',str(source),'-o',str(target)],check=True)
    text=target.read_text().replace(str(source),source.relative_to(ROOT).as_posix())
    target.write_text(text,encoding='utf-8')
    index.append(f'- [std.{source.stem}](api/{source.stem}.md)')
(ROOT/'docs/API.md').write_text('\n'.join(index)+'\n',encoding='utf-8')
print('Generated',len(list(dest.glob('*.md'))),'API modules')
