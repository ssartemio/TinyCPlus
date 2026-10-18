#!/usr/bin/env python3
"""Build a native developer distribution and preserve its complete rebuild sources."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import platform
import subprocess
import tarfile
import tempfile
import zipfile
from distribution import platform_name, architecture_name, binary_target, included_source, archive_mode

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    destination = parser.add_mutually_exclusive_group()
    destination.add_argument('--output-dir', type=Path, help='Directory for the native platform/architecture archive')
    destination.add_argument('--output', help='Archive path (.zip or .tar.gz); macOS defaults to tar.gz')
    parser.add_argument('--cc', help='External compiler used to build the packaged editor')
    args = parser.parse_args()
    host, arch = platform_name(platform.system()), architecture_name(platform.machine())
    suffix = '.tar.gz' if host == 'macos' else '.zip'
    output = Path(args.output or (args.output_dir or ROOT.parent) / f'tinycplus-1.0.0-rc.1-{host}-{arch}{suffix}').resolve()
    if not (output.name.endswith('.zip') or output.name.endswith('.tar.gz')):
        parser.error('--output must end in .zip or .tar.gz')
    tiny = ROOT / 'bin' / ('tiny.exe' if host == 'windows' else 'tiny')
    if not tiny.is_file():
        parser.error('Build the native compiler before packaging.')
    if binary_target(tiny) != (host, arch):
        parser.error('Compiler binary does not match the package platform/architecture.')
    capabilities = json.loads(subprocess.check_output([str(tiny), 'capabilities'], text=True))
    if capabilities['platform'] != host:
        parser.error('Compiler reports an incompatible platform.')
    if not capabilities['libtccAvailable']:
        parser.error('The complete distribution requires native libtcc; run tools/bootstrap.py first.')
    output.parent.mkdir(parents=True, exist_ok=True)
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='package-', dir=ROOT / 'build') as stage:
        editor = Path(stage) / ('tinyedit.exe' if host == 'windows' else 'tinyedit')
        command = [str(tiny), 'build', str(ROOT / 'apps/tinyedit.tc'), '-o', str(editor)]
        if args.cc:
            command += ['--cc', args.cc]
        subprocess.run(command, check=True)
        if binary_target(editor) != (host, arch):
            parser.error('Editor binary does not match the package target.')
        files = {p.relative_to(ROOT).as_posix(): p for p in ROOT.rglob('*')
                 if p.is_file() and p.resolve() != output and included_source(p.relative_to(ROOT), host)}
        extension = '.exe' if host == 'windows' else ''
        files.update({'bin/tiny' + extension: tiny, 'bin/tinyc' + extension: tiny,
                      'bin/tinyedit' + extension: editor})
        required = ['third_party/tcc/tinycc-source.zip', 'third_party/tcc/COPYING',
                    'third_party/dependencies.json', 'third_party/nghttp2/nghttp2-1.70.0.zip']
        required += ['third_party/tcc/libtcc.dll', 'third_party/tcc/tcc.exe', 'third_party/tcc/lib/libtcc1.a'] if host == 'windows' else [
            'third_party/tcc/posix/tcc', 'third_party/tcc/posix/libtcc1.a',
            'third_party/tcc/posix/libtcc.dylib' if host == 'macos' else 'third_party/tcc/posix/libtcc.so']
        for name in required:
            if name not in files:
                parser.error('Required distribution component missing: ' + name)
        tcc = ROOT / ('third_party/tcc/tcc.exe' if host == 'windows' else 'third_party/tcc/posix/tcc')
        library = ROOT / ('third_party/tcc/libtcc.dll' if host == 'windows' else
                          'third_party/tcc/posix/libtcc.dylib' if host == 'macos' else
                          'third_party/tcc/posix/libtcc.so')
        for component in (tcc, library):
            if binary_target(component) != (host, arch):
                parser.error('TinyCC component has the wrong platform/architecture: ' + str(component))
        capabilities.update(packagePlatform=host, architecture=arch, editorIncluded=True,
                            dependencies=(['Windows 10/11 x64 system DLLs'] if host == 'windows' else
                                          ['host C runtime', 'C development headers and libraries'] if host == 'linux' else
                                          ['macOS', 'Xcode Command Line Tools (xcode-select --install)']),
                            optionalDependencies=({'x11': 'libX11 development/runtime files'} if host == 'linux' else
                                                  {'cocoa': 'system AppKit/CoreGraphics frameworks'} if host == 'macos' else {}),
                            signing='developer build; no notarization or publisher certificate supplied',
                            validation='Run bin/tiny doctor after extracting on the target host.')
        data = {name: path.read_bytes() for name, path in files.items()}
        data['CAPABILITIES.json'] = (json.dumps(capabilities, indent=2, sort_keys=True) + '\n').encode()
        data['SHA256SUMS.txt'] = ''.join(hashlib.sha256(value).hexdigest() + '  ' + name + '\n'
                                       for name, value in sorted(data.items())).encode()
        if output.name.endswith('.zip'):
            with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
                for name, value in sorted(data.items()):
                    entry = zipfile.ZipInfo('tinycplus/' + name, date_time=(2026, 9, 15, 0, 0, 0))
                    entry.create_system = 3
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    entry.external_attr = (0o100000 | archive_mode(name)) << 16
                    archive.writestr(entry, value)
        else:
            with output.open('wb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0) as compressed:
                with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as archive:
                    for name, value in sorted(data.items()):
                        entry = tarfile.TarInfo('tinycplus/' + name)
                        entry.size, entry.mode, entry.mtime = len(value), archive_mode(name), 0
                        archive.addfile(entry, io.BytesIO(value))
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix(output.suffix + '.sha256').write_text(digest + '  ' + output.name + '\n')
    print(f'{len(data)} files; {output.stat().st_size:,} bytes; {output}\nSHA256 {digest}')


if __name__ == '__main__':
    main()
