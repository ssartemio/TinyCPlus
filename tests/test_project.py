"""Native project workflows, relative paths, FFI, overrides and invalid manifests."""
import argparse
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc')
args = parser.parse_args()
tiny = Path(os.environ.get('TINY_TEST_COMPILER', str(ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')))).resolve()
folder = ROOT / 'build' / 'tests' / ('project space ' + uuid.uuid4().hex)
for name in ('src', 'modules', 'native', 'headers', 'outside'):
    (folder / name).mkdir(parents=True, exist_ok=True)
(folder / 'modules/shared.tc').write_text('module shared;int answer(){return 2;}')
(folder / 'headers/value.h').write_text('#define VALUE 40\n')
(folder / 'native/value.c').write_text('#include "value.h"\nint native_value(void){return VALUE;}')
(folder / 'src/main.tc').write_text('import shared;extern C{int native_value();}int main(){println(native_value()+shared.answer());return 0;}')
(folder / 'src/tests.tc').write_text('import shared;extern C{int native_value();}@test void sum(){assert(native_value()+shared.answer()==42);}')
(folder / 'src/other.tc').write_text('@test int other(){return 0;}')
manifest = folder / 'tiny.project'
output = 'out/app.exe' if os.name == 'nt' else 'out/app'
contents = f'''# A minimal project; quoted paths and repeated keys need no shell.
entry = "src/main.tc"
output = "{output}"
module_path = modules
c_source = "native/value.c"
include = headers
test = src/tests.tc
test = src/other.tc
'''
manifest.write_text(contents)


def run(*command, cwd=folder, status=0, text=None):
    result = subprocess.run([str(tiny), *(['--cc', args.cc] if args.cc else []), *map(str, command)],
                            cwd=cwd, capture_output=True, text=True, timeout=90)
    assert result.returncode == status, (command, result.returncode, result.stdout, result.stderr)
    if text:
        assert text in result.stdout + result.stderr, result.stdout + result.stderr
    return result


run('build')
exe = folder / output
assert subprocess.check_output([str(exe)], text=True).strip() == '42'
run('run', text='42\n')
run('check')
result = run('test')
assert 'PASS sum' in result.stdout and 'PASS other' in result.stdout, result.stdout
run('build', '--project', manifest, cwd=folder / 'outside')
run('test', '--project', manifest, cwd=folder / 'outside', text='PASS sum')
# Inspecting generated C must not overwrite the configured executable.
binary = exe.read_bytes()
run('--emit-c', '--project', manifest, cwd=folder / 'outside', text='tiny_runtime.h')
assert exe.read_bytes() == binary
# CLI output paths are relative to the invocation directory, and override the manifest.
run('build', '--project', manifest, '-o', 'override.exe' if os.name == 'nt' else 'override', cwd=folder / 'outside')
assert (folder / 'outside' / ('override.exe' if os.name == 'nt' else 'override')).exists()
# Module search paths also work with the existing explicit-source interface.
run('run', folder / 'src/main.tc', '--module-path', folder / 'modules', '--c-source', folder / 'native/value.c', '-I', folder / 'headers', cwd=folder / 'outside', text='42\n')
# The importer's own directory wins over configured module roots.
(folder / 'src/shared.tc').write_text('module shared;int answer(){return 3;}')
run('run', text='43\n')
(folder / 'src/shared.tc').unlink()
# CLI compiler takes precedence over a deliberately unusable manifest compiler.
manifest.write_text(contents + 'cc = tiny-no-such-compiler\n')
run('check')
run('build', '--cc', args.cc or 'cc')
manifest.write_text(contents)
# Arguments reach the program after -- without project-option parsing.
(folder / 'src/main.tc').write_text('import std.process;int main(){println(Arguments.get(1));return 0;}')
run('run', '--', 'hello project', text='hello project\n')
# A failed test file does not hide later files or turn the result into success.
(folder / 'src/tests.tc').write_text('@test int failure(){return 1;}')
run('test', status=1, text='PASS other')
# Default output path is created if no output key was supplied.
manifest.write_text('entry = src/main.tc\n')
run('build')
assert (folder / 'build' / ('app.exe' if os.name == 'nt' else 'app')).exists()
invalid = [
    ('unknown = value\n', "unknown project key"),
    ('entry = a\nentry = b\n', "duplicate project key"),
    ('entry = "unterminated\n', 'unterminated project value'),
    ('entry = "a" trailing\n', 'unexpected text'),
    ('entry =\n', 'must not be empty'),
    ('entry src/main.tc\n', 'expected key = value'),
    ('entry = "bad\\q"\n', 'unsupported escape'),
    ('test = src/tests.tc\n', 'requires an entry'),
    ('module_path = modules\n', 'requires an entry'),
]
for source, diagnostic in invalid:
    manifest.write_text(source)
    run('build', status=2, text=diagnostic)
manifest.write_text('entry = src/main.tc\n')
run('test', status=2, text='no test entries')
run('build', '--project', manifest, 'extra.tc', status=2, text='cannot be combined')
run('build', '--project', folder / 'absent.project', status=2, text='cannot read')
print('Projects verified: native build/run/check/test, modules/FFI, spaced paths, CLI overrides, diagnostics and failure propagation')
