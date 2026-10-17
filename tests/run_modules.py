#!/usr/bin/env python3
"""Focused bank for the modules developed here; Python stdlib only.

Reuses contract/model suites, records real process outcomes and writes JSON/JUnit.
Sanitizer stages use separate binaries and do not replace bin/tiny.
"""
import argparse
from datetime import datetime, timezone
import json
import hashlib
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MODULES = ('collections', 'clones', 'iterators', 'projects', 'diagnostics')
SCRIPTS = {'collections': 'test_collections.py', 'clones': 'test_collections.py',
           'iterators': 'test_iterators.py', 'projects': 'test_project.py',
           'diagnostics': 'test_diagnostics.py'}


def positive(value):
    value = int(value)
    if value <= 0:
        raise argparse.ArgumentTypeError('must be greater than zero')
    return value


def execute(command, env, timeout):
    started = time.monotonic()
    options = {'start_new_session': True} if os.name != 'nt' else {
        'creationflags': subprocess.CREATE_NEW_PROCESS_GROUP}
    try:
        process = subprocess.Popen(list(map(str, command)), cwd=ROOT, env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, encoding='utf-8', errors='replace', **options)
        try:
            stdout, stderr = process.communicate(timeout=timeout)
            code = process.returncode
            status = 'passed' if code == 0 else 'failed'
        except subprocess.TimeoutExpired:
            # Kill the suite's child compilers/programs as well as the Python parent.
            if os.name == 'nt':
                subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                process.kill()
            else:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            stdout, stderr = process.communicate()
            code, status = 124, 'timeout'
        return dict(status=status, exit_code=code, stdout=stdout, stderr=stderr,
                    seconds=round(time.monotonic() - started, 3))
    except OSError as error:
        return dict(status='error', exit_code=127, stdout='', stderr=str(error),
                    seconds=round(time.monotonic() - started, 3))


def atomic_write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    temporary.write_text(text, encoding='utf-8')
    os.replace(temporary, path)


def xml_text(text):
    # XML 1.0 excludes most control bytes, even inside captured compiler output.
    return ''.join(ch if ch in '\t\n\r' or 0x20 <= ord(ch) <= 0xD7FF or
                   0xE000 <= ord(ch) <= 0xFFFD or 0x10000 <= ord(ch) <= 0x10FFFF
                   else '\ufffd' for ch in text)


def save(report, json_path, junit_path):
    results = report['results']
    counts = {state: sum(r['status'] == state for r in results)
              for state in ('passed', 'failed', 'error', 'timeout', 'skipped')}
    report['summary'] = dict(stages=len(results), **counts)
    atomic_write(json_path, json.dumps(report, indent=2, ensure_ascii=True) + '\n')
    suite = ET.Element('testsuite', name='TinyCPlus developed modules', tests=str(len(results)),
                       failures=str(counts['failed']), errors=str(counts['error'] + counts['timeout']),
                       skipped=str(counts['skipped']), time=str(round(sum(r['seconds'] for r in results), 3)))
    for result in results:
        case = ET.SubElement(suite, 'testcase', name=result['name'],
                             classname=f"{result['module']}.{result['backend']}", time=str(result['seconds']))
        if result['status'] != 'passed':
            tag = 'skipped' if result['status'] == 'skipped' else 'failure' if result['status'] == 'failed' else 'error'
            node = ET.SubElement(case, tag, message=result.get('reason', f"{result['status']}: exit {result['exit_code']}"))
            node.text = xml_text(result['stderr'])
        ET.SubElement(case, 'system-out').text = xml_text(result['stdout'])
        ET.SubElement(case, 'system-err').text = xml_text(result['stderr'])
    atomic_write(junit_path, ET.tostring(suite, encoding='unicode', xml_declaration=True) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', nargs='+', choices=MODULES, default=list(MODULES))
    parser.add_argument('--backend', choices=('both', 'default', 'external'), default='both')
    parser.add_argument('--cc', default=shutil.which('cc') or shutil.which('gcc') or shutil.which('clang'))
    parser.add_argument('--no-build', action='store_true', help='Use the existing frontend')
    parser.add_argument('--repeat', type=positive, default=1)
    parser.add_argument('--timeout', type=positive, default=600, help='Seconds per stage')
    parser.add_argument('--sanitize', action='store_true', help='Also check generated C and frontend with ASan/UBSan (POSIX GCC/Clang)')
    parser.add_argument('--report', type=Path, default=ROOT / 'build/module-bench/report.json')
    parser.add_argument('--junit', type=Path, help='Defaults to the JSON path with .xml suffix')
    args = parser.parse_args()
    args.report = args.report.resolve()
    args.junit = (args.junit or args.report.with_suffix('.xml')).resolve()
    if args.report == args.junit:
        parser.error('JSON and JUnit paths must differ')
    if (args.backend != 'default' or args.sanitize) and not args.cc:
        parser.error('an external C compiler is required; supply --cc or use --backend default')
    if args.sanitize and os.name == 'nt':
        parser.error('--sanitize currently supports POSIX GCC/Clang')
    modules = list(dict.fromkeys(args.module))
    run_id = uuid.uuid4().hex
    artifacts = ROOT / 'build/module-bench' / run_id
    artifacts.mkdir(parents=True)
    tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
    env = os.environ.copy()
    env.pop('PYTHONOPTIMIZE', None)  # Existing suites use assert: never disable their checks.
    env.pop('TINY_TEST_COMPILER', None)
    env['TINY_HOME'] = str(ROOT)
    env['PYTHONIOENCODING'] = 'utf-8'
    if args.cc:
        compiler = shutil.which(args.cc) or args.cc
        env['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + env.get('PATH', '')
    report = dict(schema_version=1, run_id=run_id, started_at=datetime.now(timezone.utc).isoformat(),
                  status='running', platform=platform.platform(), python=sys.version,
                  modules=modules, backend=args.backend, cc=args.cc, repeat=args.repeat,
                  artifacts=str(artifacts), sanitizer_requested=args.sanitize, results=[])

    # Identify the actual source inputs even while the checkout has local edits.
    inputs = [*(ROOT / 'compiler').glob('*.[ch]'), *(ROOT / 'runtime').glob('*.[ch]'),
              *(ROOT / 'std').glob('*.tc'), *(ROOT / 'tests').glob('*.tc'),
              *(ROOT / 'tests').glob('*.py'), ROOT / 'third_party/dependencies.json']
    report['source_sha256'] = {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                               for path in sorted(inputs)}

    def stage(name, module, backend, command=None, stage_env=None, reason=None):
        print(f'[{backend}] {name}', flush=True)
        result = dict(name=name, module=module, backend=backend, command=list(map(str, command or [])))
        if reason:
            result.update(status='skipped', exit_code=None, stdout='', stderr='', seconds=0, reason=reason)
        else:
            result.update(execute(command, stage_env or env, args.timeout))
        index = len(report['results']) + 1
        log = artifacts / f'{index:03d}.log'
        log.write_text(result['stdout'] + '\n--- stderr ---\n' + result['stderr'], encoding='utf-8')
        result['log'] = str(log)
        report['results'].append(result)
        save(report, args.report, args.junit)
        print(f"  {result['status']} ({result['seconds']}s)", flush=True)
        if result['status'] in ('failed', 'error', 'timeout'):
            print(result['stderr'][-6000:], file=sys.stderr, flush=True)
        return result['status'] == 'passed'

    command = [str(tiny), '--version'] if args.no_build else [sys.executable, ROOT / 'build.py']
    if not args.no_build and args.cc:
        command += ['--cc', args.cc]
    ready = stage('Existing compiler' if args.no_build else 'Build compiler', 'setup', 'host', command)
    if ready and tiny.is_file():
        report['compiler_sha256'] = hashlib.sha256(tiny.read_bytes()).hexdigest()
    for repeat in range(1, args.repeat + 1):
        for module in modules:
            backends = ['frontend'] if module == 'diagnostics' else (
                ['default', 'external'] if args.backend == 'both' else [args.backend])
            for backend in backends:
                command = [sys.executable, ROOT / 'tests' / SCRIPTS[module]]
                if module in ('clones', 'collections'):
                    command += ['--only', module]
                if backend == 'external':
                    command += ['--cc', args.cc]
                stage(f'{module} / run {repeat}', module, backend, command,
                      reason=None if ready else 'Compiler prerequisite failed')
    if args.sanitize:
        sanitized_env = env.copy()
        sanitized_env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
        sanitized_env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
        flags = ['-std=c11', '-D_POSIX_C_SOURCE=200809L', '-fsanitize=address,undefined',
                 '-fno-omit-frame-pointer', '-g']
        for module in modules:
            if module not in ('collections', 'clones', 'iterators'):
                continue
            source = artifacts / (module + '.c')
            exe = artifacts / module
            emitted = stage(f'Emit {module}', module, 'sanitize', [tiny, '--emit-c', ROOT / 'tests' / (module + '.tc'), '-o', source],
                            reason=None if ready else 'Compiler prerequisite failed')
            compiled = stage(f'Compile {module}', module, 'sanitize',
                             [args.cc, *flags, '-I' + str(ROOT / 'runtime'), source, '-pthread', '-lm', '-o', exe],
                             reason=None if emitted else 'C emission failed')
            stage(f'Run {module}', module, 'sanitize', [exe], stage_env=sanitized_env,
                  reason=None if compiled else 'Sanitized compilation failed')
        if 'diagnostics' in modules or 'projects' in modules:
            frontend = artifacts / 'tiny-sanitize'
            compiled = stage('Build sanitized frontend', 'setup', 'sanitize',
                             [args.cc, *flags, *sorted((ROOT / 'compiler').glob('*.c')),
                              *([] if sys.platform == 'darwin' else ['-ldl']), '-o', frontend],
                             reason=None if ready else 'Compiler prerequisite failed')
            sanitized_env['TINY_TEST_COMPILER'] = str(frontend)
            for module in ('diagnostics', 'projects'):
                if module not in modules:
                    continue
                command = [sys.executable, ROOT / 'tests' / SCRIPTS[module]]
                if module == 'projects':
                    command += ['--cc', args.cc]
                stage(f'Sanitized {module}', module, 'sanitize', command, stage_env=sanitized_env,
                      reason=None if compiled else 'Sanitized frontend compilation failed')
    failed = any(result['status'] != 'passed' for result in report['results'])
    report['status'] = 'failed' if failed else 'passed'
    report['finished_at'] = datetime.now(timezone.utc).isoformat()
    save(report, args.report, args.junit)
    counts = report['summary']
    print(f"\n{counts['passed']}/{counts['stages']} stages passed. JSON: {args.report}\nJUnit: {args.junit}")
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
