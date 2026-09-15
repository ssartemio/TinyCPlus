"""Wire interoperability against grpcio, independently of the TinyC+ codec."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import os
import subprocess
import uuid
import time
import grpc

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc')
options = parser.parse_args()
folder = ROOT / 'build' / 'tests' / ('grpc-' + uuid.uuid4().hex)
folder.mkdir(parents=True)
os.environ['TMP'] = os.environ['TEMP'] = str(folder)
os.environ['TINY_WORKERS'] = '1'
tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')

def compile_program(name, source):
    path = folder / (name + '.tc')
    path.write_text(source, encoding='utf-8')
    executable = folder / (name + ('.exe' if os.name == 'nt' else ''))
    command = [str(tiny), 'build', str(path), '-o', str(executable)]
    if options.cc:
        command += ['--cc', options.cc]
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    return executable

native_server = compile_program('server', '''
import std.grpc;
extern C { void tc_flush_stdout(); }
void* echo(string data, void* context) { return tc_grpc_response(data, 0); }
void* deny(string data, void* context) { return tc_grpc_response("denied", 7); }
int main() {
    var server, error = GrpcServer.listen("127.0.0.1", 0); assert(error == 0);
    defer server.destroy();
    assert(server.add("/wire.Echo/Unary", echo) == 0);
    assert(server.add("/wire.Echo/Deny", deny) == 0);
    println(server.port()); tc_flush_stdout();
    return server.serve(1);
}
''')
process = subprocess.Popen([str(native_server)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
try:
    port = int(process.stdout.readline())
    with grpc.insecure_channel(f'127.0.0.1:{port}') as channel:
        echo = channel.unary_unary('/wire.Echo/Unary')
        for payload in (b'', b'hello\x00binary\xff', bytes(range(256)) * 2048):
            assert echo(payload, timeout=10) == payload
        with ThreadPoolExecutor(max_workers=8) as pool:
            futures = [pool.submit(echo, str(i).encode(), timeout=10) for i in range(24)]
            assert [f.result() for f in futures] == [str(i).encode() for i in range(24)]
        for path, expected in [('/wire.Echo/Missing', grpc.StatusCode.UNIMPLEMENTED), ('/wire.Echo/Deny', grpc.StatusCode.PERMISSION_DENIED)]:
            try:
                channel.unary_unary(path)(b'', timeout=10)
                raise AssertionError('expected gRPC error')
            except grpc.RpcError as error:
                assert error.code() == expected, error
    stdout, stderr = process.communicate(timeout=15)
    assert process.returncode == 0, (stdout, stderr)
finally:
    if process.poll() is None:
        process.kill()
        process.communicate()

def denied(request, context):
    context.abort(grpc.StatusCode.PERMISSION_DENIED, 'expected denial')
def slow(request, context):
    time.sleep(0.15)
    return request

server = grpc.server(ThreadPoolExecutor(max_workers=4))
server.add_generic_rpc_handlers((grpc.method_handlers_generic_handler('wire.Echo', {
    'Unary': grpc.unary_unary_rpc_method_handler(lambda data, context: data),
    'Deny': grpc.unary_unary_rpc_method_handler(denied),
    'Slow': grpc.unary_unary_rpc_method_handler(slow),
}),))
port = server.add_insecure_port('127.0.0.1:0')
server.start()
payload = bytes(range(256)) * 2048
payload_path = folder / 'payload.bin'
payload_path.write_bytes(payload)
try:
    native_client = compile_program('client', '''
import std.grpc;
import std.fs;
async int request() {
    GrpcClient client("127.0.0.1", PORT);
    var data, error = File.readAll("PAYLOAD"); assert(error == 0); defer data.destroy();
    var reply = await client.callAsync("/wire.Echo/Unary", data.view()); defer reply.destroy();
    assert(reply.status() == 0); assert(reply.data() == data.view());
    var denied = client.call("/wire.Echo/Deny", ""); defer denied.destroy(); assert(denied.status() == 7);
    var unknown = client.call("/wire.Echo/Unknown", ""); defer unknown.destroy(); assert(unknown.status() == 12);
    var empty = client.call("/wire.Echo/Unary", ""); defer empty.destroy(); assert(empty.status() == 0); assert(len(empty.data()) == 0);
    var timeout = client.call("/wire.Echo/Slow", "", 20); defer timeout.destroy(); assert(timeout.status() == 4);
    println("native client interoperates"); return 0;
}
int main() { var task = request(); defer task.destroy(); return task.get(); }
'''.replace('PORT', str(port)).replace('PAYLOAD', payload_path.as_posix()))
    result = subprocess.run([str(native_client)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0 and result.stdout == 'native client interoperates\n', (result.stdout, result.stderr)
finally:
    server.stop(0).wait()

print('gRPC interop passed: both directions, binary/empty/512 KiB payloads, multiplexing, status trailers')
