"""Exercise a distribution moved outside the checkout, with spaces and Unicode paths."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
import tarfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--archive', type=Path, help='Validate an already built shipping archive')
args = parser.parse_args()
(ROOT / 'build').mkdir(exist_ok=True)
archive = args.archive
if archive is None:
    archive = ROOT / 'build' / ('distribution-test.tar.gz' if sys.platform == 'darwin' else 'distribution-test.zip')
    subprocess.run([sys.executable, str(ROOT / 'tools/package.py'), '--output', str(archive)], check=True)
archive = archive.resolve()
with tempfile.TemporaryDirectory(prefix='TinyC+ distribución ñ 日 ') as temporary:
    destination = Path(temporary) / 'unpacked'
    def unpack(name, content, mode):
        path = destination / name
        assert path.resolve().is_relative_to(destination.resolve()), name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        path.chmod(mode & 0o777)
    if archive.name.endswith('.zip'):
        with zipfile.ZipFile(archive) as package:
            for entry in package.infolist():
                if entry.is_dir():
                    continue
                if entry.filename.startswith('tinycplus/bin/'):
                    assert entry.external_attr >> 16 & 0o111
                unpack(entry.filename, package.read(entry), entry.external_attr >> 16)
    else:
        with tarfile.open(archive) as package:
            for entry in package:
                assert entry.isfile(), entry.name
                if entry.name.startswith('tinycplus/bin/'):
                    assert entry.mode & 0o111
                unpack(entry.name, package.extractfile(entry).read(), entry.mode)
    root = Path(temporary) / 'instalación final'
    (destination / 'tinycplus').rename(root)
    for line in (root / 'SHA256SUMS.txt').read_text().splitlines():
        digest, name = line.split('  ', 1)
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
    capabilities = json.loads((root / 'CAPABILITIES.json').read_text())
    assert capabilities['packagePlatform'] == ('windows' if os.name == 'nt' else 'macos' if sys.platform == 'darwin' else 'linux')
    assert capabilities['architecture'] in ('x86_64', 'arm64') and capabilities['editorIncluded']
    assert (root / 'third_party/tcc/tinycc-source.zip').is_file()
    env = os.environ.copy()
    for key in ('TINY_HOME', 'LIBTCC_PATH', 'CC', 'SDKROOT', 'LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH'):
        env.pop(key, None)
    if os.name == 'nt':
        env['PATH'] = str(Path(env['SystemRoot']) / 'System32') + os.pathsep + env['SystemRoot']
    else:
        env['PATH'] = '/usr/bin:/bin:/usr/sbin:/sbin'
    ext = '.exe' if os.name == 'nt' else ''
    tiny = root / 'bin' / ('tiny' + ext)
    def run(command, input=None):
        result = subprocess.run(list(map(str, command)), input=input, env=env, cwd=root,
                                capture_output=True, text=True, encoding='utf-8', timeout=40)
        assert result.returncode == 0, (command, result.returncode, result.stdout, result.stderr)
        return result
    result = run([tiny, 'doctor'])
    assert str(root).replace('\\', '/') in result.stderr.replace('\\', '/')
    tcc = root / ('third_party/tcc/tcc.exe' if os.name == 'nt' else 'third_party/tcc/posix/tcc')
    run([tcc, '-v'])
    run([tiny, 'repl'], 'println(17);\n:quit\n')
    source = root / 'aplicación ñ.tc'
    source.write_text('import std.process; int main(){assert(Arguments.get(1)=="niño 日"); println("ok"); return 0;}', encoding='utf-8')
    output = root / ('programa ñ' + ext)
    run([tiny, 'run', source, '--', 'niño 日'])
    run([tiny, 'build', source, '-o', output])
    assert run([output, 'niño 日']).stdout.strip() == 'ok'
    # Freshly built packaged editor must support history and UTF-8 file paths/arguments.
    document, replay = root / 'notas ñ 日.txt', root / 'teclas ñ.txt'
    document.write_text('', encoding='utf-8')
    replay.write_bytes('ñ日\x1a\x19\x13\x11'.encode())
    run([root / 'bin' / ('tinyedit' + ext), document, '--replay', replay])
    assert document.read_text(encoding='utf-8') == 'ñ日'
print('Distribution verified: hashes, sources, relocated loader, UTF-8 paths/argv, native build/run, REPL and fresh editor')
