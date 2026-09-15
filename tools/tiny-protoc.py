#!/usr/bin/env python3
"""Dependency-free proto3 -> TinyC+ messages and unary gRPC bindings.

Supported: all scalar wire types, enums, nested messages, repeated/packed,
optional presence, maps as arrays of entries, imports and unary services.
Unknown fields are skipped. Unsupported syntax is diagnosed, never ignored.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass, field
import json
from pathlib import Path
import re
import sys

SCALARS = {
    'double': ('double', 1), 'float': ('float', 5),
    'int32': ('int', 0), 'int64': ('i64', 0), 'uint32': ('uint', 0), 'uint64': ('u64', 0),
    'sint32': ('int', 0), 'sint64': ('i64', 0), 'fixed32': ('uint', 5), 'fixed64': ('u64', 1),
    'sfixed32': ('int', 5), 'sfixed64': ('i64', 1), 'bool': ('bool', 0),
    'string': ('string', 2), 'bytes': ('string', 2),
}
RESERVED = {'encode', 'decode', 'decodeView', 'mergeView', 'destroy', 'wireStorage', 'this', 'new', 'delete', 'var', 'return'}
TOKEN = re.compile(r'\s+|//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[A-Za-z_][A-Za-z_0-9]*|-?\d+|[{}()[\];=.,<>]')

@dataclass
class Field:
    type: str
    name: str
    number: int
    repeated: bool = False
    optional: bool = False
    packed: bool = True
    resolved: str = ''

@dataclass
class Message:
    full: str
    name: str
    fields: list[Field] = field(default_factory=list)

@dataclass
class Enum:
    full: str
    name: str
    values: list[tuple[str, int]] = field(default_factory=list)

@dataclass
class Service:
    full: str
    name: str
    methods: list[tuple[str, str, str]] = field(default_factory=list)

class Schema:
    def __init__(self, include):
        self.include = include
        self.files = set()
        self.types = {}
        self.services = []
    def read(self, path):
        path = path.resolve()
        if path in self.files:
            return
        self.files.add(path)
        Parser(self, path).parse()
    def add(self, value):
        if value.full in self.types or any(t.name == value.name for t in self.types.values()):
            raise ValueError(f'duplicate or ambiguous generated type name: {value.full}')
        self.types[value.full] = value
    def resolve(self, name, scope):
        if name in SCALARS:
            return name
        if name.startswith('.'):
            choices = [name[1:]]
        else:
            parts = scope.split('.')
            choices = ['.'.join(parts[:i] + [name]) for i in range(len(parts), -1, -1)]
        for choice in choices:
            if choice in self.types:
                return choice
        raise ValueError(f'unknown type {name!r} in {scope}')
    def validate(self):
        for value in self.types.values():
            if isinstance(value, Message):
                for f in value.fields:
                    f.resolved = self.resolve(f.type, value.full)
        for service in self.services:
            service.methods = [(name, self.resolve(a, service.full), self.resolve(b, service.full)) for name, a, b in service.methods]
            for _, a, b in service.methods:
                if not isinstance(self.types.get(a), Message) or not isinstance(self.types.get(b), Message):
                    raise ValueError('RPC request/response must be messages')

class Parser:
    def __init__(self, schema, path):
        self.schema, self.path, self.package = schema, path, ''
        source = path.read_text(encoding='utf-8-sig')
        if len(source) > 4 * 1024 * 1024:
            raise ValueError(f'{path}: schema exceeds 4 MiB')
        self.tokens = []
        position = 0
        while position < len(source):
            match = TOKEN.match(source, position)
            if not match:
                raise ValueError(f'{path}:{source.count(chr(10), 0, position) + 1}: invalid token')
            token = match.group()
            if not token.isspace() and not token.startswith(('//', '/*')):
                self.tokens.append(token)
            position = match.end()
        self.tokens.append('<eof>')
        self.pos = 0
    def peek(self):
        return self.tokens[self.pos]
    def pop(self):
        value = self.peek()
        if value == '<eof>':
            self.fail('unexpected end of file')
        self.pos += 1
        return value
    def eat(self, value):
        if self.peek() == value:
            self.pos += 1
            return True
        return False
    def need(self, value):
        if not self.eat(value):
            self.fail(f'expected {value!r}, found {self.peek()!r}')
    def fail(self, message):
        raise ValueError(f'{self.path}: token {self.pos + 1}: {message}')
    def ident(self):
        value = self.pop()
        if not re.fullmatch('[A-Za-z_][A-Za-z_0-9]*', value) or value.startswith(('tc_', '__')):
            self.fail(f'invalid or reserved identifier {value!r}')
        return value
    def qualified(self):
        value = '.' if self.eat('.') else ''
        value += self.ident()
        while self.eat('.'):
            value += '.' + self.ident()
        return value
    def ignored_option(self):
        # File/language options do not change the wire format.
        while self.pop() != ';':
            pass
    def parse(self):
        syntax = False
        while self.peek() != '<eof>':
            token = self.pop()
            if token == 'syntax':
                self.need('='); self.need('"proto3"'); self.need(';'); syntax = True
            elif token == 'package':
                self.package = self.qualified(); self.need(';')
            elif token == 'import':
                self.eat('public')
                name = json.loads(self.pop()); self.need(';')
                candidates = [self.path.parent / name] + [p / name for p in self.schema.include]
                found = next((p for p in candidates if p.is_file()), None)
                if found is None:
                    self.fail(f'import not found: {name}')
                self.schema.read(found)
            elif token == 'message':
                self.message(self.package, '')
            elif token == 'enum':
                self.enum(self.package, '')
            elif token == 'service':
                self.service()
            elif token == 'option':
                self.ignored_option()
            elif token != ';':
                self.fail(f'unsupported declaration {token!r}')
        if not syntax:
            self.fail('explicit syntax = "proto3" is required')
    def message(self, scope, prefix):
        name = self.ident(); full = '.'.join(filter(None, (scope, name)))
        msg = Message(full, prefix + name); self.schema.add(msg); self.need('{')
        if len(full.split('.')) > 64:
            self.fail('message nesting exceeds 64')
        while not self.eat('}'):
            if self.eat('message'):
                self.message(full, msg.name + '_'); continue
            if self.eat('enum'):
                self.enum(full, msg.name + '_'); continue
            if self.eat('option') or self.eat('reserved'):
                self.ignored_option(); continue
            if self.eat(';'):
                continue
            repeated = self.eat('repeated'); optional = False if repeated else self.eat('optional')
            if self.eat('map'):
                if repeated or optional:
                    self.fail('map cannot have a label')
                self.need('<'); key = self.qualified(); self.need(','); value = self.qualified(); self.need('>')
                if key not in SCALARS or key in ('float', 'double', 'bytes'):
                    self.fail('invalid map key type')
                field_name = self.ident(); entry_name = field_name[:1].upper() + field_name[1:] + 'Entry'
                entry = Message(full + '.' + entry_name, msg.name + '_' + entry_name,
                                [Field(key, 'key', 1), Field(value, 'value', 2)])
                self.schema.add(entry); type_name = entry_name; repeated = True
            else:
                type_name = self.qualified(); field_name = self.ident()
            self.need('='); number = int(self.pop())
            if not 1 <= number <= 536870911 or 19000 <= number <= 19999:
                self.fail('field number outside legal range')
            if field_name in RESERVED or field_name.startswith('has_') or any(f.name == field_name or f.number == number for f in msg.fields):
                self.fail(f'duplicate or reserved field: {field_name}')
            packed = True
            if self.eat('['):
                while True:
                    option = self.ident(); self.need('='); val = self.pop()
                    if option == 'packed' and val in ('true', 'false'):
                        packed = val == 'true'
                    elif option != 'deprecated':
                        self.fail(f'unsupported field option: {option}')
                    if not self.eat(','):
                        break
                self.need(']')
            self.need(';'); msg.fields.append(Field(type_name, field_name, number, repeated, optional, packed))
    def enum(self, scope, prefix):
        name = self.ident(); value = Enum('.'.join(filter(None, (scope, name))), prefix + name)
        self.schema.add(value); self.need('{')
        while not self.eat('}'):
            if self.eat('option') or self.eat('reserved'):
                self.ignored_option(); continue
            key = self.ident(); self.need('='); number = int(self.pop()); self.need(';')
            if not -2147483648 <= number <= 2147483647 or any(k == key for k, _ in value.values):
                self.fail('invalid enum value')
            value.values.append((key, number))
        if not value.values or value.values[0][1] != 0:
            self.fail('proto3 enum must start with zero')
    def service(self):
        name = self.ident(); service = Service('.'.join(filter(None, (self.package, name))), name)
        self.schema.services.append(service); self.need('{')
        while not self.eat('}'):
            self.need('rpc'); method = self.ident(); self.need('('); request = self.qualified(); self.need(')')
            self.need('returns'); self.need('('); response = self.qualified(); self.need(')')
            if self.eat('{'):
                while not self.eat('}'):
                    self.need('option'); self.ignored_option()
            else:
                self.need(';')
            if any(m[0] == method for m in service.methods):
                self.fail('duplicate RPC method')
            service.methods.append((method, request, response))

class Generator:
    def __init__(self, schema):
        self.s = schema
        self.lines = ['// Generated by tiny-protoc. Do not edit.', 'import std.protobuf;']
        if schema.services:
            self.lines.append('import std.grpc;')
    def emit(self, text=''):
        self.lines.append(text)
    def info(self, f):
        if f.resolved in SCALARS:
            return (*SCALARS[f.resolved], False)
        value = self.s.types[f.resolved]
        return (value.name, 2, True) if isinstance(value, Message) else ('int', 0, False)
    def read_number(self, f, reader):
        typ, wire, _ = self.info(f)
        read = f'{reader}.' + {0: 'number()', 1: 'fixed64()', 5: 'fixed32()'}[wire]
        if f.resolved in ('sint32', 'sint64'):
            read = f'tc_pb_unzigzag({read})'
        elif f.resolved == 'double':
            read = f'tc_pb_bits_double({read})'
        elif f.resolved == 'float':
            read = f'tc_pb_bits_float(cast<uint>({read}))'
        return f'cast<{typ}>({read})'
    def write_value(self, f, value, writer, tagged=True):
        _, wire, message = self.info(f)
        if tagged:
            self.emit(f'        {writer}.tag({f.number}, {wire});')
        if message:
            self.emit(f'        var pbEncoded, pbError = {value}.encode(depth + 1); defer pbEncoded.destroy();')
            self.emit('        if (pbError != 0) { OwnedString pbEmpty; return (pbEmpty, pbError); }')
            self.emit(f'        {writer}.bytes(pbEncoded.view());')
        elif wire == 2:
            if f.resolved == 'string':
                self.emit(f'        if (!tc_pb_valid_utf8({value})) {{ OwnedString pbEmpty; return (pbEmpty, 3); }}')
            self.emit(f'        {writer}.bytes({value});')
        else:
            if f.resolved in ('sint32', 'sint64'):
                value = f'tc_pb_zigzag({value})'
            elif f.resolved == 'double':
                value = f'tc_pb_double_bits({value})'
            elif f.resolved == 'float':
                value = f'tc_pb_float_bits({value})'
            self.emit(f'        {writer}.' + {0: 'number', 1: 'fixed64', 5: 'fixed32'}[wire] + f'(cast<u64>({value}));')
    def message(self, msg):
        self.emit(f'\n/// decode owns a copied input; decodeView borrows input. destroy releases arrays/nested objects.\nclass {msg.name} {{')
        self.emit('    OwnedString wireStorage;')
        for f in msg.fields:
            typ, _, message = self.info(f)
            stored = f'Array<{typ}>' if f.repeated else typ + ('*' if message else '')
            self.emit(f'    {stored} {f.name};')
            if f.optional and not message:
                self.emit(f'    bool has_{f.name};')
        self.emit('    void destroy() {')
        for f in msg.fields:
            _, _, message = self.info(f)
            if f.repeated:
                if message:
                    self.emit(f'        for (u64 pbIndex = 0; pbIndex < {f.name}.length; pbIndex++) {f.name}.data[pbIndex].destroy();')
                self.emit(f'        {f.name}.destroy();')
            elif message:
                self.emit(f'        if ({f.name} != null) {{ {f.name}.destroy(); delete {f.name}; {f.name} = null; }}')
        self.emit('        wireStorage.destroy();\n    }')
        self.emit('    (OwnedString, Error) encode(int depth = 0) {\n        if (depth > 64) { OwnedString pbEmpty; return (pbEmpty, 3); }\n        var pbWriter = ProtoWriter.create(); defer pbWriter.destroy();')
        for f in msg.fields:
            _, wire, message = self.info(f)
            value = f'this.{f.name}'
            if f.repeated:
                if wire != 2 and f.packed:
                    self.emit(f'        if ({value}.length > 0) {{\n        var pbPacked = ProtoWriter.create(); defer pbPacked.destroy();')
                    self.emit(f'        for (pbValue in {value}) {{')
                    self.write_value(f, 'pbValue', 'pbPacked', False)
                    self.emit('        }\n        if (pbPacked.error() != 0) { OwnedString pbEmpty; return (pbEmpty, pbPacked.error()); }')
                    self.emit(f'        var pbBytes = pbPacked.take(); defer pbBytes.destroy(); pbWriter.tag({f.number}, 2); pbWriter.bytes(pbBytes.view());\n        }}')
                else:
                    self.emit(f'        for (pbValue in {value}) {{'); self.write_value(f, 'pbValue', 'pbWriter'); self.emit('        }')
            else:
                test = f'{value} != null' if message else f'this.has_{f.name}' if f.optional else f'len({value}) > 0' if wire == 2 else f'{value} != 0'
                self.emit(f'        if ({test}) {{'); self.write_value(f, value, 'pbWriter'); self.emit('        }')
        self.emit('        var pbResult = pbWriter.take(); return (pbResult, pbWriter.error());\n    }')
        self.emit(f'    static ({msg.name}, Error) decode(string input) {{\n        var pbStorage = String.copy(input);\n        var pbResult, pbError = {msg.name}.decodeView(pbStorage.view()); pbResult.wireStorage = pbStorage; return (pbResult, pbError);\n    }}')
        self.emit(f'    static ({msg.name}, Error) decodeView(string input, int depth = 0) {{\n        {msg.name} pbResult; var pbError = pbResult.mergeView(input, depth); return (pbResult, pbError);\n    }}')
        self.emit('    Error mergeView(string input, int depth = 0) {\n        if (depth > 64) return 3;\n        var pbReader = ProtoReader.create(input); defer pbReader.destroy();\n        while (!pbReader.done()) {\n        var pbField = pbReader.next(); if (pbField == 0) break;\n        var pbWire = pbReader.wire();')
        for f in msg.fields:
            typ, wire, message = self.info(f)
            target = f'this.{f.name}'
            self.emit(f'        if (pbField == {f.number}) {{')
            if f.repeated and wire != 2:
                self.emit('        if (pbWire == 2) {\n        var pbPacked = ProtoReader.create(pbReader.bytes()); defer pbPacked.destroy();\n        while (!pbPacked.done()) {')
                self.emit(f'        {target}.push({self.read_number(f, "pbPacked")});\n        }}\n        if (pbPacked.error() != 0) return pbPacked.error();\n        continue;\n        }}')
            self.emit(f'        if (pbWire != {wire}) {{ pbReader.skip(pbField); continue; }}')
            if message:
                if f.repeated:
                    self.emit(f'        var pbChild, pbError = {typ}.decodeView(pbReader.bytes(), depth + 1);\n        {target}.push(pbChild); if (pbError != 0) return pbError;')
                else:
                    self.emit(f'        if ({target} == null) {target} = new {typ}();\n        var pbError = {target}.mergeView(pbReader.bytes(), depth + 1); if (pbError != 0) return pbError;')
            else:
                read = 'pbReader.bytes()' if wire == 2 else self.read_number(f, 'pbReader')
                if f.resolved == 'string':
                    self.emit('        var pbText = pbReader.bytes(); if (!tc_pb_valid_utf8(pbText)) return 3;')
                    read = 'pbText'
                self.emit(f'        {target}.push({read});' if f.repeated else f'        {target} = {read};')
                if f.optional:
                    self.emit(f'        this.has_{f.name} = true;')
            self.emit('        continue;\n        }')
        self.emit('        pbReader.skip(pbField);\n        }\n        return pbReader.error();\n    }\n}')
    def service(self, service):
        name = service.name
        self.emit(f'\ninterface {name}Service {{')
        for method, a, b in service.methods:
            self.emit(f'    async ({self.s.types[b].name}, Error) {method}({self.s.types[a].name} request);')
        self.emit(f'}}\nclass {name}Client {{\n    GrpcClient transport;')
        self.emit(f'    static {name}Client connect(string host, int port) {{ {name}Client client; client.transport.host = host; client.transport.port = port; return client; }}')
        for method, a, b in service.methods:
            request, response = self.s.types[a].name, self.s.types[b].name
            self.emit(f'    async ({response}, Error) {method}({request} request, u64 timeout = 30000) {{\n        {response} empty;\n        var bytes, error = request.encode(); defer bytes.destroy(); if (error != 0) return (empty, error);')
            self.emit(f'        var reply = await transport.callAsync("/{service.full}/{method}", bytes.view(), timeout); defer reply.destroy();\n        if (reply.status() != 0) return (empty, reply.status());\n        return {response}.decode(reply.data());\n    }}')
        self.emit('}')
        self.emit(f'class {name}Binding {{\n    {name}Service service;\n    Error addTo(GrpcServer* server) {{\n        Error error;')
        for method, _, _ in service.methods:
            self.emit(f'        error = server.add("/{service.full}/{method}", {name}_{method}_dispatch, this); if (error != 0) return error;')
        self.emit('        return 0;\n    }\n}')
        for method, a, b in service.methods:
            request = self.s.types[a].name
            self.emit(f'void* {name}_{method}_dispatch(string data, void* context) {{\n    var binding = cast<{name}Binding*>(context);\n    var request, error = {request}.decodeView(data); defer request.destroy();\n    if (error != 0) return tc_grpc_response("", 3);')
            self.emit(f'    var task = binding.service.{method}(request); defer task.destroy();\n    var pair, taskError = task.result(); if (taskError != 0) return tc_grpc_response("", 13);\n    var response, status = pair; defer response.destroy();\n    if (status != 0) return tc_grpc_response("", status);\n    var encoded, encodingError = response.encode(); defer encoded.destroy();\n    if (encodingError != 0) return tc_grpc_response("", 13);\n    return tc_grpc_response(encoded.view(), 0);\n}}')
    def generate(self):
        for value in self.s.types.values():
            if isinstance(value, Message):
                self.message(value)
            else:
                self.emit(f'\nclass {value.name} {{')
                for name, number in value.values:
                    self.emit(f'    static int {name}() {{ return {number}; }}')
                self.emit('}')
        for service in self.s.services:
            self.service(service)
        return '\n'.join(self.lines) + '\n'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('-I', '--proto-path', action='append', type=Path, default=[])
    parser.add_argument('-o', '--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        schema = Schema(args.proto_path); schema.read(args.input); schema.validate()
        output = Generator(schema).generate()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding='utf-8')
    except (ValueError, OSError, RecursionError) as error:
        print(f'tiny-protoc: {error}', file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
