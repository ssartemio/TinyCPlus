"""Native package policy, shared by packaging and host-independent tests."""
from pathlib import Path
import struct

SOURCE_DIRS = {'apps', 'compiler', 'docs', 'examples', 'runtime', 'std', 'tests', 'tools', 'third_party'}
ROOT_FILES = {'README.md', 'THIRD_PARTY.md', 'WORKLOG.md', 'build.py', '.clang-format', '.gitattributes', '.gitignore'}


def platform_name(system):
    try:
        return {'Windows': 'windows', 'Darwin': 'macos', 'Linux': 'linux'}[system]
    except KeyError:
        raise ValueError('Unsupported package platform: ' + system)


def architecture_name(machine):
    aliases = {'amd64': 'x86_64', 'x64': 'x86_64', 'x86_64': 'x86_64',
               'aarch64': 'arm64', 'arm64': 'arm64'}
    try:
        return aliases[machine.lower()]
    except KeyError:
        raise ValueError('Unsupported package architecture: ' + machine)


def binary_target(path):
    """Read native headers, independent of the host and executable suffix."""
    with Path(path).open('rb') as stream:
        header = stream.read(64)
        if header[:2] == b'MZ' and len(header) == 64:
            stream.seek(struct.unpack_from('<I', header, 60)[0])
            pe = stream.read(6)
            if pe[:4] == b'PE\0\0':
                return 'windows', {0x8664: 'x86_64', 0xAA64: 'arm64'}.get(struct.unpack_from('<H', pe, 4)[0])
        if header[:4] == b'\x7fELF' and len(header) >= 20:
            endian = '<' if header[5] == 1 else '>'
            return 'linux', {62: 'x86_64', 183: 'arm64'}.get(struct.unpack_from(endian + 'H', header, 18)[0])
        if header[:4] in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf'):
            endian = '<' if header[0] == 0xcf else '>'
            return 'macos', {0x1000007: 'x86_64', 0x100000C: 'arm64'}.get(struct.unpack_from(endian + 'I', header, 4)[0])
    raise ValueError('Unrecognized native binary: ' + str(path))


def included_source(relative, host):
    """Exclude scratch/private files, while always retaining dependency sources/licenses."""
    relative = Path(relative)
    parts = relative.parts
    if any(part in {'build', '__pycache__', '.git', '.DS_Store'} for part in parts) or relative.suffix == '.pyc':
        return False
    if len(parts) == 1:
        return parts[0] in ROOT_FILES
    if parts[0] not in SOURCE_DIRS:
        return False
    if parts[:2] == ('third_party', 'tcc'):
        if len(parts) == 3 and parts[2] in {'tinycc-source.zip', 'COPYING', 'VERSION', 'libtcc.h', 'macos-codesign.patch'}:
            return True
        return (parts[2] != 'posix') if host == 'windows' else parts[2] == 'posix'
    return not (parts[:2] == ('third_party', 'nghttp2') and relative.name in {'nghttp2.dll', 'libnghttp2.dll.a'})


def archive_mode(relative):
    relative = Path(relative)
    return 0o755 if relative.parts[0] == 'bin' or relative.name in {'tcc', 'tcc.exe'} else 0o644
