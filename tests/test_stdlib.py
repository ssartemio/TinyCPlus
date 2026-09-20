"""StringBuilder, formatting, Console.writeError and Process.spawn.

The program is built to a real executable so that Process.spawn can start the
program itself; the child checks that every argument arrived byte for byte,
which exercises Windows command-line quoting as well as POSIX execvp.
"""
from pathlib import Path
import argparse
import os
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc', help='External C compiler instead of the bundled libtcc')
opts = parser.parse_args()
tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
folder = ROOT / 'build' / 'tests' / ('stdlib-' + uuid.uuid4().hex)
folder.mkdir(parents=True)

SOURCE = r'''
import std.string;
import std.io;
import std.process;
import std.collections;

string expected(int index) {
    switch (index) {
        case 1: return "--child";
        case 2: return "two words";
        case 3: return "quote\"inside";
        case 4: return "trailing\\";
        case 5: return "";
        case 6: return "semi;colon & pipe | $HOME %PATH%";
        default: return "?";
    }
}

int child() {
    if (Arguments.count() == 3 && Arguments.get(1) == "--exit") {
        var code, error = String.parseInt(Arguments.get(2));
        return cast<int>(code);
    }
    if (Arguments.count() != 7) return 3;
    for (int i = 1; i < 7; i++)
        if (Arguments.get(i) != expected(i)) return 10 + i;
    return 0;
}

int main() {
    if (Arguments.count() > 1) return child();

    var b = StringBuilder.create();
    defer b.destroy();
    b.append("n=");
    b.appendInt(-42);
    b.appendChar(' ');
    b.appendInt(-9223372036854775807 - 1);
    b.appendChar(' ');
    b.appendUnsigned(18446744073709551615);
    b.append(" 0x");
    b.appendHex(3735928559);
    b.appendChar(' ');
    b.appendInt(0);
    b.appendHex(0);
    b.appendChar(' ');
    b.appendDouble(0.1);
    b.appendChar(' ');
    b.appendDouble(-2.5);
    b.appendChar(' ');
    b.appendDouble(1.0 / 3.0);
    println(b.view());
    for (int i = 0; i < 100000; i++) b.append("0123456789");
    assert(b.length > 1000000);
    var copy = b.toOwned();
    defer copy.destroy();
    b.clear();
    assert(b.length == 0);
    assert(b.view() == "");
    assert(len(copy.view()) > 1000000);
    var f = String.fromDouble(1e-7);
    defer f.destroy();
    println(f.view());

    Console.writeErrorLine("diagnostic line");

    var args = Array<string>.create();
    defer args.destroy();
    args.push(Arguments.get(0));
    for (int i = 1; i < 7; i++) args.push(expected(i));
    var status, error = Process.spawn(&args);
    assert(error == 0);
    println(status);

    var exits = Array<string>.create();
    defer exits.destroy();
    exits.push(Arguments.get(0));
    exits.push("--exit");
    exits.push("7");
    var code, exitError = Process.spawn(&exits);
    assert(exitError == 0);
    println(code);

    var missing = Array<string>.create();
    defer missing.destroy();
    missing.push("tinycplus-no-such-program-4096");
    var ignored, missingError = Process.spawn(&missing);
    assert(missingError != 0);

    var empty = Array<string>.create();
    var none, emptyError = Process.spawn(&empty);
    assert(emptyError != 0);
    println("done");
    return 0;
}
'''

source = folder / 'stdlib.tc'
source.write_text(SOURCE)
exe = folder / ('stdlib.exe' if os.name == 'nt' else 'stdlib')
build = [str(tiny), 'build', str(source), '-o', str(exe), '--home', str(ROOT)]
if opts.cc:
    build += ['--cc', opts.cc]
p = subprocess.run(build, capture_output=True, text=True, timeout=120)
assert p.returncode == 0, p.stderr
p = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
assert p.returncode == 0, (p.returncode, p.stdout, p.stderr)
lines = p.stdout.splitlines()
assert lines == [
    'n=-42 -9223372036854775808 18446744073709551615 0xdeadbeef 00 0.1 -2.5 0.3333333333333333',
    '1e-07',
    '0',
    '7',
    'done',
], lines
assert p.stderr.replace('\r\n', '\n') == 'diagnostic line\n', repr(p.stderr)
print('Standard library verified: StringBuilder, number formatting, stderr, Process.spawn quoting/exit codes/errors')
