"""Embed a UTF-8 process manifest without requiring windres on end-user machines."""
import struct
from pathlib import Path

MANIFEST = b'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="1.0.0.0" processorArchitecture="amd64" name="TinyCPlus" type="win32"/>
  <application xmlns="urn:schemas-microsoft-com:asm.v3"><windowsSettings>
    <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
  </windowsSettings></application>
</assembly>
'''


def write_resource(folder, compiler):
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    directory = lambda: struct.pack('<IIHHHH', 0, 0, 0, 0, 0, 1)
    section = (directory() + struct.pack('<II', 24, 0x80000018) +
               directory() + struct.pack('<II', 1, 0x80000030) +
               directory() + struct.pack('<II', 0x409, 72) +
               struct.pack('<IIII', 88, len(MANIFEST), 65001, 0) + MANIFEST)
    section += bytes(-len(section) % 4)
    symbols = 60 + len(section) + 10
    header = struct.pack('<HHIIIHH', 0x8664, 1, 0, symbols, 1, 0, 0)
    header += struct.pack('<8sIIIIIIHHI', b'.rsrc', 0, 0, len(section), 60,
                          60 + len(section), 0, 1, 0, 0x40000040)
    relocation = struct.pack('<IIH', 72, 0, 3)  # IMAGE_REL_AMD64_ADDR32NB
    symbol = struct.pack('<8sIhHBB', b'.rsrc', 0, 1, 0, 3, 0)
    path = folder / 'utf8-manifest.o'
    path.write_bytes(header + section + relocation + symbol + struct.pack('<I', 4))
    return path
