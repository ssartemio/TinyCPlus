# Fuente de la Wiki de TinyC+

Este directorio contiene la fuente versionada de la Wiki del proyecto.

Los nombres de archivo siguen la convención de GitHub Wiki:

- `Home.md`: portada.
- `_Sidebar.md`: navegación lateral.
- `_Footer.md`: pie común.
- El resto de los archivos son páginas enlazadas desde la portada y el sidebar.

La documentación distingue explícitamente entre:

1. **TinyC+ 1.0.0-rc.1**, disponible en `main` y validado por CI.
2. **Trabajo post-1.0 experimental**, especialmente la GUI gráfica desarrollada
   en ramas/PR separados.

## Publicar en la Wiki nativa de GitHub

La Wiki de GitHub es un repositorio Git separado. Desde un equipo con acceso de
escritura se puede sincronizar este directorio así:

```bash
git clone git@github.com:ssartemio/TinyCPlus.wiki.git build/TinyCPlus.wiki
rsync -av --delete wiki/ build/TinyCPlus.wiki/ --exclude README.md
cd build/TinyCPlus.wiki
git add -A
git commit -m "docs: update project wiki"
git push
```

En PowerShell puede copiar el contenido de `wiki\` al clon de
`TinyCPlus.wiki.git`, excluyendo este `README.md`.

La fuente permanece en el repositorio principal para que los cambios de
documentación puedan revisarse junto con el código.


## Publicador multiplataforma

También se incluye:

```bash
python tools/publish-wiki.py --dry-run
python tools/publish-wiki.py
```

El script clona el repositorio Wiki, sincroniza estas páginas, hace commit y
push usando las credenciales Git configuradas en el equipo.


## Curso integrado

La Wiki incluye el curso progresivo `TinyCPlus-Desde-Cero.md` y 13 capítulos
`Curso-01-...` a `Curso-13-...`. Estos archivos también se sincronizan con
la Wiki nativa mediante `tools/publish-wiki.py`.
