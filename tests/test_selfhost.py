"""Differential tests between stage 0 (compiler/, C) and stage 1 (selfhost/, TinyC+).

Stage 1 is built with stage 0 and must behave exactly like it: same stdout, same stderr and
the same exit status. Today that covers the lexer (`--emit-tokens`); each later phase adds a
row to PHASES. The typed syntax tree that stage 0 prints (`--emit-typed-ast`) is checked here
too, because it is the reference the semantic phase will be compared against.
"""
from pathlib import Path
import argparse
import os
import random
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc', help='External C compiler instead of the bundled libtcc')
parser.add_argument('--fuzz', type=int, default=300, help='deterministic random lexer inputs')
opts = parser.parse_args()

ext = '.exe' if os.name == 'nt' else ''
tiny = ROOT / 'bin' / ('tiny' + ext)
folder = ROOT / 'build' / 'tests' / ('selfhost-' + uuid.uuid4().hex)
folder.mkdir(parents=True)
stage1 = folder / ('tinyc1' + ext)


def run(command, **extra):
    p = subprocess.run([str(part) for part in command], cwd=ROOT, capture_output=True, timeout=300, **extra)
    return p.returncode, p.stdout, p.stderr


# ---- build stage 1 with stage 0 ----------------------------------------------------------
build = [tiny, 'build', 'selfhost/main.tc', '-o', stage1, '--home', ROOT]
if opts.cc:
    build += ['--cc', opts.cc]
code, out, err = run(build)
assert code == 0, err.decode(errors='replace')

failures = []


def compare(label, path, phase_flag):
    """Runs both compilers on path (relative to ROOT when possible) and records any difference."""
    a = run([tiny, phase_flag, path])
    b = run([stage1, phase_flag, path])
    if a != b:
        failures.append((label, a, b))
    return a


# ---- 1. every TinyC+ source in the repository ---------------------------------------------
skipped = ('third_party', 'build', '.git')
corpus = sorted(p for p in ROOT.rglob('*.tc') if not any(part in skipped for part in p.relative_to(ROOT).parts))
assert len(corpus) > 50, len(corpus)
for path in corpus:
    compare('repo file ' + path.relative_to(ROOT).as_posix(), path.relative_to(ROOT).as_posix(), '--emit-tokens')

# ---- 2. hand-written edge cases: valid, tricky and malformed ------------------------------
CASES = [
    ('empty', b''),
    ('spaces_only', b'  \t\n\n  '),
    ('bom_only', b'\xef\xbb\xbf'),
    ('bom_code', b'\xef\xbb\xbfint main(){return 0;}'),
    ('partial_bom', b'\xef\xbb'),
    ('crlf', b'int a;\r\nint b;\r\n'),
    ('vt_ff', b'a\x0bb\x0cc'),
    ('no_final_newline', b'int a;'),
    ('line_comment_eof', b'a // no newline at the end'),
    ('block_comment', b'a /* x */ b'),
    ('nested_comment', b'/* a /* b */ c */ d'),
    ('empty_comment', b'/**/x'),
    ('comment_star_slash', b'/*/ x */ y'),
    ('multiline_comment_positions', b'/* a\nb\n c */ x'),
    ('unterminated_comment', b'x /* never closed'),
    ('unterminated_nested', b'/* a /* b */ c'),
    ('identifiers', b'_ _1 a_b9 A9_ __x'),
    ('integers', b'0 007 42 0x0 0XFF 0xdeadBEEF'),
    ('floats', b'1.5 0.25 1e5 1E-3 2e+8 1. 3.e2'),
    ('range_not_float', b'1..2 0..n'),
    ('bad_hex_empty', b'0x'),
    ('bad_hex_digit', b'0xg'),
    ('bad_hex_suffix', b'0x1fz'),
    ('bad_exponent', b'1e'),
    ('bad_exponent_sign', b'1e+'),
    ('bad_suffix', b'12abc'),
    ('bad_float_suffix', b'1.0f'),
    ('bad_underscore_suffix', b'5_000'),
    ('dot_before_digit', b'.5'),
    ('strings', b'"" "a" "hello world" "tab\\t" "q\\"" "s\\\\" "z\\0"'),
    ('hex_escape', b'"\\x41\\x4a" \'\\x41\''),
    ('hex_escape_short', b'"\\x4"'),
    ('hex_escape_bad', b'"\\xZZ"'),
    ('bad_escape', b'"\\q"'),
    ('bad_escape_char', b"'\\q'"),
    ('unterminated_string', b'"abc'),
    ('unterminated_escape', b'"abc\\'),
    ('newline_in_string', b'"a\nb"'),
    ('cr_in_string', b'"a\rb"'),
    ('chars', b"'a' '\\n' '\\'' '\"' ' '"),
    ('empty_char', b"''"),
    ('long_char', b"'ab'"),
    ('utf8_char', b"'\xc3\xa9'"),
    ('utf8_string', b'"h\xc3\xa9llo" x'),
    ('utf8_positions', b'"\xc3\xa9\xc3\xa9" x'),
    ('operators_three', b'>>= <<= ...'),
    ('operators_two', b'== != <= >= && || ++ -- += -= *= /= %= &= |= ^= << >> -> => ::'),
    ('operators_one', b'+ - * / % = < > ! & | ^ ~ ( ) { } [ ] ; , . : ? @'),
    ('operators_glued', b'a>>=b<<=c...d..e=>f->g::h'),
    ('operators_triple_gt', b'>>> <<< ==='),
    ('shifts_and_compare', b'a<<b>>c<=d>=e'),
    ('hash', b'a # b'),
    ('dollar', b'$'),
    ('backtick', b'`'),
    ('backslash_outside', b'\\'),
    ('high_byte', b'a \x80'),
    ('high_byte_ff', b'\xff'),
    ('control_byte', b'\x01'),
    ('nul_in_middle', b'a b\x00c d'),
    ('nul_first', b'\x00abc'),
    ('positions_after_tabs', b'\ta\t\tb'),
    ('token_limit_ok', b';' * 1048575),
    ('token_limit_at_eof', b';' * 1048576),
    ('token_limit_over', b';' * 1048577),
]
for name, data in CASES:
    source = folder / (name + '.tc')
    source.write_bytes(data)
    compare('case ' + name, source.relative_to(ROOT).as_posix(), '--emit-tokens')

# ---- 3. deterministic fuzzing ---------------------------------------------------------------
rng = random.Random(0x54696e79)
FRAGMENTS = [b'int', b'main', b'x1', b'_', b'0', b'42', b'0x1F', b'1.5', b'1e9', b'"s"', b"'c'", b'"\\n"',
             b'+', b'-', b'>>=', b'<<', b'...', b'..', b'->', b'=>', b'::', b'(', b')', b'{', b'}', b';',
             b' ', b'\n', b'\t', b'\r\n', b'//c\n', b'/*c*/', b'/*', b'*/', b'"', b"'", b'\\', b'0x',
             b'1e', b'12z', b'"\\q"', b'"\\x1"', b'\xc3\xa9', b'\x80', b'#', b'$', b'\x00']
for index in range(opts.fuzz):
    parts = []
    for _ in range(rng.randint(1, 40)):
        if rng.random() < 0.08:
            parts.append(bytes(rng.randrange(256) for _ in range(rng.randint(1, 3))))
        else:
            parts.append(rng.choice(FRAGMENTS))
    source = folder / ('fuzz%d.tc' % index)
    source.write_bytes(b''.join(parts))
    compare('fuzz %d' % index, source.relative_to(ROOT).as_posix(), '--emit-tokens')

# ---- 4. the CLI contract that both stages share ---------------------------------------------
code, out, err = run([stage1])
assert code == 2 and b'usage' in err, (code, err)
missing = folder / 'does-not-exist.tc'
a = run([tiny, '--emit-tokens', missing])
b = run([stage1, '--emit-tokens', missing])
assert a[0] == b[0] == 1 and b"cannot read" in a[2] and a[2] == b[2], (a, b)

# ---- 5. stage 0 typed syntax tree: the reference for the semantic phase ---------------------
typed = 0
for path in corpus:
    relative = path.relative_to(ROOT).parts
    if relative[0] not in ('examples', 'apps') or 'experimental' in relative:
        continue
    first = run([tiny, '--emit-typed-ast', path.relative_to(ROOT).as_posix()])
    second = run([tiny, '--emit-typed-ast', path.relative_to(ROOT).as_posix()])
    assert first[0] == 0, (path, first[2])
    assert first == second, 'typed AST is not deterministic: %s' % path
    assert first[1].startswith(b'Program'), path
    assert b' : i32' in first[1] or b' : string' in first[1] or b' : func<' in first[1], path
    typed += 1
assert typed >= 10, typed
plain = run([tiny, '--emit-ast', 'examples/hello.tc'])[1]
annotated = run([tiny, '--emit-typed-ast', 'examples/hello.tc'])[1]
assert b' : ' not in plain and b' : ' in annotated
code, out, err = run([tiny, '--emit-typed-ast', ROOT / 'tests' / 'golden' / 'defer.tc'])
assert code == 0, err
bad = folder / 'type_error.tc'
bad.write_text('int main(){int x = "text"; return 0;}')
code, out, err = run([tiny, '--emit-typed-ast', bad])
assert code == 1 and out == b'' and b'expected i32, found string' in err, (code, out, err)

# ---- report ------------------------------------------------------------------------------------
if failures:
    for label, a, b in failures[:5]:
        print('MISMATCH', label)
        print('  stage 0:', a[0], a[1][:200], a[2][:200])
        print('  stage 1:', b[0], b[1][:200], b[2][:200])
    print('%d of the comparisons differ' % len(failures), file=sys.stderr)
    sys.exit(1)
print('Self-hosting verified: stage 1 lexer matches stage 0 on %d repository files, %d edge cases and %d fuzz inputs; '
      'typed AST checked on %d programs' % (len(corpus), len(CASES), opts.fuzz, typed))
