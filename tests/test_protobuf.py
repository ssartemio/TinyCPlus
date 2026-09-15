"""Cross-check generated TinyC+ against Google's protobuf implementation."""
from pathlib import Path
import argparse
import importlib
import os
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc')
args = parser.parse_args()
folder = ROOT / 'build' / 'tests' / ('protobuf-' + uuid.uuid4().hex)
folder.mkdir(parents=True)
os.environ['TMP'] = os.environ['TEMP'] = str(folder)
schema = ROOT / 'tests' / 'wire.proto'
subprocess.run([sys.executable, str(ROOT / 'tools' / 'tiny-protoc.py'), str(schema), '-o', str(folder / 'wire.tc')], check=True)
subprocess.run([sys.executable, '-m', 'grpc_tools.protoc', '-I', str(schema.parent), '--python_out=' + str(folder), str(schema)], check=True)
sys.path.insert(0, str(folder))
pb = importlib.import_module('wire_pb2')
message = pb.Wire(d=3.141592653589793, f=-1.25, i=-2147483648, l=-9223372036854775808,
    u=4294967295, ul=18446744073709551615, si=-2000, sl=-9223372036854775808,
    fx=4294967295, fxl=18446744073709551615, sfx=-2147483648, sfxl=-9223372036854775808,
    flag=True, text='á日本語', binary=b'\x00\xff\x80', state=-1, numbers=[-1, 0, 1, 2**62],
    decimals=[-0.25, 2.5], present=0)
message.child.name = 'child'
message.child.count = 2
message.children.add(name='one', count=1)
message.children.add(name='two', count=2)
message.entries['first'] = -10
message.entries['second'] = 20
message.tree.add(text='recursive').tree.add(i=3)
message.nested.value = 'nested'
input_path, output_path = folder / 'input.pb', folder / 'output.pb'
# Unknown field, followed by a duplicate nested message that must merge.
serialized = message.SerializeToString() + b'\xf8\x07\x01' + b'\x9a\x01\x02\x10\x05'
input_path.write_bytes(serialized)
message.child.count = 5
source = '''
import wire;
import std.fs;
int main() {
    var input, fileError = File.readAll("INPUT"); assert(fileError == 0); defer input.destroy();
    var message, error = Wire.decode(input.view()); defer message.destroy(); assert(error == 0);
    assert(message.present == 0); assert(message.has_present); assert(message.numbers.length == 4);
    assert(message.child.count == 5); assert(message.children.length == 2);
    assert(message.tree[0].tree[0].i == 3); assert(message.nested.value == "nested");
    var encoded, writeError = message.encode(); defer encoded.destroy(); assert(writeError == 0);
    assert(File.writeAll("OUTPUT", encoded.view()) == 0);
    var broken, brokenError = Wire.decode("\\x80"); defer broken.destroy(); assert(brokenError != 0);
    var overflow, overflowError = Wire.decode("\\x18\\xff\\xff\\xff\\xff\\xff\\xff\\xff\\xff\\xff\\x02"); defer overflow.destroy(); assert(overflowError != 0);
    var truncated, truncatedError = Wire.decode("\\x72\\x05ab"); defer truncated.destroy(); assert(truncatedError != 0);
    var invalidUtf8, utfError = Wire.decode("\\x72\\x02\\xc0\\xaf"); defer invalidUtf8.destroy(); assert(utfError != 0);
    Wire invalidText; invalidText.text = "\\xff";
    var invalidEncoding, invalidError = invalidText.encode(); defer invalidEncoding.destroy(); assert(invalidError != 0);
    Wire deep; defer deep.destroy(); Wire* cursor = &deep;
    for (int i=0; i<70; i++) { Wire child; cursor.tree.push(child); cursor = &cursor.tree[0]; }
    var tooDeep, depthError = deep.encode(); defer tooDeep.destroy(); assert(depthError != 0);
    println("protobuf verified"); return 0;
}
'''.replace('INPUT', input_path.as_posix()).replace('OUTPUT', output_path.as_posix())
program = folder / 'main.tc'
program.write_text(source, encoding='utf-8')
command = [str(ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')), 'run', str(program)]
if args.cc:
    command += ['--cc', args.cc]
result = subprocess.run(command, capture_output=True, text=True, timeout=90)
assert result.returncode == 0 and result.stdout == 'protobuf verified\n', (result.stdout, result.stderr)
actual = pb.Wire.FromString(output_path.read_bytes())
assert actual == message, (actual, message)

# Unsupported or invalid schemas must fail without creating misleading bindings.
for invalid in ('syntax = "proto2";', 'syntax="proto3"; message X {int32 a=0;}',
                'syntax="proto3"; message X {oneof x {int32 a=1;}}',
                'syntax="proto3"; message X {int32 a=1;int32 b=1;}'):
    bad = folder / 'bad.proto'; bad.write_text(invalid)
    p = subprocess.run([sys.executable, str(ROOT / 'tools' / 'tiny-protoc.py'), str(bad), '-o', str(folder / 'bad.tc')], capture_output=True, text=True)
    assert p.returncode == 1 and 'tiny-protoc:' in p.stderr
print('Protobuf interop passed: all scalar encodings, enums, presence, packed/unpacked, maps, nested/recursive messages, unknown/duplicate fields, malformed input')
