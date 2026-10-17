"""Source excerpts and concrete generic provenance, including imported sources."""
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
tiny = Path(os.environ.get('TINY_TEST_COMPILER', str(ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')))).resolve()
folder = ROOT / 'build' / 'tests' / ('diagnostics-' + uuid.uuid4().hex)
folder.mkdir(parents=True)


def check(name, source, message, marked=None):
    path = folder / (name + '.tc')
    path.write_bytes(source.encode('utf-8'))
    result = subprocess.run([str(tiny), 'check', str(path)], cwd=ROOT, capture_output=True, text=True, timeout=20)
    assert result.returncode == 1, (name, result.returncode, result.stdout, result.stderr)
    assert message in result.stderr, result.stderr
    assert '^' in result.stderr, result.stderr
    assert '\x1b' not in result.stderr, repr(result.stderr)
    if marked:
        lines = result.stderr.splitlines()
        for i, line in enumerate(lines):
            if '| ' in line and marked in line:
                text = line.split('| ', 1)[1]
                underline = lines[i + 1].split('| ', 1)[1]
                assert text.index(marked) == underline.index('^'), (text, underline)
                break
        else:
            raise AssertionError(result.stderr)
    return result.stderr


check('basic', 'int main(){return unknown;}', "undefined identifier 'unknown'", 'unknown')
check('tabs', 'int main(){\n\treturn unknown;\n}', 'undefined identifier', 'unknown')
check('crlf', 'int main(){\r\n    return unknown;\r\n}\r\n', 'undefined identifier', 'unknown')
check('utf8', 'int main(){println("café");return unknown;}', 'undefined identifier', 'unknown')
check('bom', '\ufeffint main(){return unknown;}', 'undefined identifier', 'unknown')
check('controls', 'int main(){/*\x1b[31m*/return unknown;}', 'undefined identifier', 'unknown')
check('multidigit', '\n' * 12 + 'int main(){return unknown;}', 'undefined identifier', 'unknown')
check('eof', 'int main(){\n', 'unterminated block')
long = check('long', 'int main(){' + ' ' * 10000 + 'return unknown;}', 'undefined identifier', 'unknown')
assert '...' in long and len(long) < 1000, len(long)
nested = check('nested', '''class Inner<T> {
    T invalid(){return "wrong";}
}
class Outer<T>{Inner<T> item;}
int main(){var x=Outer<int>();return 0;}
''', 'expected i32, found string')
assert "while instantiating 'Inner<i32>'" in nested, nested
assert "while instantiating 'Outer<i32>'" in nested, nested
assert nested.index("'Inner<i32>'") < nested.index("'Outer<i32>'"), nested
function = check('function', 'T invalid<T>(T x){return "wrong";}int main(){return invalid<int>(0);}', 'expected i32, found string')
assert "while instantiating 'invalid<i32>'" in function, function
(folder / 'broken.tc').write_text('module broken;int bad(){return unknown;}')
imported = check('imports', 'import broken;int main(){return broken.bad();}', 'undefined identifier', 'unknown')
assert 'broken.tc:1:' in imported and '| module broken;' in imported, imported
missing = check('missing_import', 'import absent;int main(){return 0;}', "cannot find module 'absent'", 'import')
literal = check('literal', 'int main(){return "bad";}', 'expected i32, found string', '"bad"')
assert '^~~~~' in literal, literal
plain = check('plain', 'int main(){return unknown;}', 'undefined identifier')
assert 'while instantiating' not in plain, plain
print('Diagnostics verified: source excerpts/carets, tabs/UTF-8/BOM/CRLF, bounded/control-safe output, imports and nested class/function instantiation context')
