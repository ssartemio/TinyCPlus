"""Cross-platform package contracts that can be checked on every host."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from distribution import architecture_name, platform_name, binary_target, included_source
from windows_manifest import write_resource, MANIFEST
from tcc_portability import patch_codesign


class DistributionPolicy(unittest.TestCase):
    def test_native_targets(self):
        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder) / 'binary'
            for machine, expected in [(0x8664, 'x86_64'), (0xAA64, 'arm64')]:
                pe = bytearray(70)
                pe[:2] = b'MZ'; struct.pack_into('<I', pe, 60, 64)
                pe[64:68] = b'PE\0\0'; struct.pack_into('<H', pe, 68, machine)
                target.write_bytes(pe)
                self.assertEqual(binary_target(target), ('windows', expected))
            for cpu, expected in [(0x1000007, 'x86_64'), (0x100000C, 'arm64')]:
                target.write_bytes(struct.pack('<II', 0xfeedfacf, cpu) + bytes(56))
                self.assertEqual(binary_target(target), ('macos', expected))
            target.write_bytes(b'not a native binary')
            with self.assertRaises(ValueError): binary_target(target)
        self.assertEqual(architecture_name('AMD64'), 'x86_64')
        self.assertEqual(architecture_name('aarch64'), 'arm64')
        with self.assertRaises(ValueError): platform_name('UnknownOS')

    def test_sources_and_foreign_binaries(self):
        for host in ('windows', 'macos', 'linux'):
            self.assertTrue(included_source('third_party/tcc/tinycc-source.zip', host))
            self.assertTrue(included_source('third_party/tcc/macos-codesign.patch', host))
            self.assertTrue(included_source('third_party/tcc/COPYING', host))
            self.assertFalse(included_source('.env', host))
            self.assertFalse(included_source('build/private.zip', host))
        self.assertFalse(included_source('third_party/tcc/libtcc.dll', 'macos'))
        self.assertFalse(included_source('third_party/tcc/posix/libtcc.dylib', 'windows'))
        self.assertTrue(included_source('third_party/tcc/posix/libtcc.dylib', 'macos'))

    def test_windows_resource(self):
        with tempfile.TemporaryDirectory() as folder:
            resource = write_resource(folder, 'gcc').read_bytes()
        self.assertEqual(struct.unpack_from('<H', resource)[0], 0x8664)
        self.assertIn(MANIFEST, resource)
        self.assertEqual(struct.unpack_from('<I', resource, 60 + 16)[0], 24)  # RT_MANIFEST
        self.assertEqual(struct.unpack_from('<I', resource, 60 + 72)[0], 88)  # relocated XML RVA
        self.assertIn(b'<activeCodePage', MANIFEST)

    def test_macos_codesign_patch_against_verified_source(self):
        with zipfile.ZipFile(ROOT / 'third_party/tcc/tinycc-source.zip') as archive:
            original = archive.read('tinycc-mob/tccmacho.c').decode()
        modified = patch_codesign(original)
        self.assertNotIn('system (command)', modified)
        self.assertIn('"-", filename, (char *)NULL', modified)
        with self.assertRaises(ValueError): patch_codesign(modified)


if __name__ == '__main__':
    unittest.main()
