#!/usr/bin/env python3
"""Rebuild bundled dependencies offline from their verified source archives."""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT/'third_party/dependencies.json').read_text())
parser = argparse.ArgumentParser()
parser.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
parser.add_argument('--only', choices=['tcc','nghttp2','all'], default='all')
parser.add_argument('--verify-only', action='store_true')
args = parser.parse_args()
env = os.environ.copy()
cc = shutil.which(args.cc) or args.cc
env['PATH'] = str(Path(cc).resolve().parent) + os.pathsep + env.get('PATH','')

def run(command, cwd):
    subprocess.run(list(map(str,command)),cwd=cwd,env=env,check=True)

def unpack(name):
    item = LOCK[name]
    archive = ROOT/item['archive']
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != item['sha256']:
        raise SystemExit(f'{name}: source archive checksum mismatch')
    print(name, 'verified', digest, flush=True)
    if args.verify_only:
        return None
    folder = ROOT/'build'/('bootstrap-'+name+'-'+uuid.uuid4().hex)
    folder.mkdir(parents=True)
    with zipfile.ZipFile(archive) as z:
        for entry in z.infolist():
            target = (folder/entry.filename).resolve()
            if not target.is_relative_to(folder.resolve()):
                raise SystemExit('Archive contains an unsafe path')
        z.extractall(folder)
    return folder/item['source_directory']

def tcc(source):
    dest = ROOT/'third_party/tcc'
    if os.name == 'nt':
        if os.environ.get('PROCESSOR_ARCHITECTURE','').upper() not in ('AMD64','X86_64'):
            raise SystemExit('Windows bootstrap currently targets x86-64; use a matching x64 shell')
        win = source/'win32'
        (source/'config.h').write_text('#define TCC_VERSION "'+(source/'VERSION').read_text().strip()+'"\n')
        flags = ['-O2','-DTCC_TARGET_PE','-DTCC_TARGET_X86_64']
        run([cc,*flags,'-shared','-DLIBTCC_AS_DLL','../libtcc.c','-o','libtcc.dll'],win)
        run([cc,*flags,'../tcc.c','-o','tcc.exe'],win)
        for header in (source/'include').glob('*.h'):
            shutil.copy2(header,win/'include'/header.name)
        shutil.copy2(source/'tcclib.h',win/'include/tcclib.h')
        compiler = win/'tcc.exe'
        objects = []
        for name in ['../lib/libtcc1.c','lib/crt1.c','lib/crt1w.c','lib/wincrt1.c','lib/wincrt1w.c',
                     'lib/dllcrt1.c','lib/dllmain.c','lib/winex.c','lib/chkstk.S','../lib/alloca.S',
                     '../lib/alloca-bt.S','../lib/stdatomic.c','../lib/atomic.S','../lib/builtin.c']:
            obj = Path(name).stem+'.o'
            run([compiler,'-B.','-c',name,'-o',obj],win)
            objects.append(obj)
        run([compiler,'-ar','lib/libtcc1.a',*objects],win)
        for name in ['bt-exe','bt-log','bt-dll','runmain']:
            run([compiler,'-B.','-I..','-c','../lib/'+name+'.c','-o','lib/'+name+'.o'],win)
        for name in ['tcc.exe','libtcc.dll']:
            shutil.copy2(win/name,dest/name)
        for name in ['include','lib']:
            shutil.copytree(win/name,dest/name,dirs_exist_ok=True)
    else:
        # The source archive includes configure; invoking sh needs no executable permission.
        run(['sh','configure','--cc='+cc,'--prefix='+str(dest),'--bindir='+str(dest),
             '--libdir='+str(dest),'--tccdir='+str(dest),'--disable-static'],source)
        run(['make','-j2'],source)
        # Avoid mixing Windows SDK headers from the portable package with host headers.
        if (dest/'include/winapi').exists():
            for directory in (dest/'include', dest/'lib'):
                resolved = directory.resolve()
                if not resolved.is_relative_to(dest.resolve()) or resolved == dest.resolve():
                    raise SystemExit('Invalid dependency install directory')
                if directory.exists():
                    shutil.rmtree(directory)
        run(['make','install'],source)
    for name in ['COPYING','VERSION','libtcc.h']:
        shutil.copy2(source/name,dest/name)

def nghttp2(source):
    dest = ROOT/'third_party/nghttp2'
    shutil.copytree(source/'lib',dest/'src',dirs_exist_ok=True)
    shutil.copy2(source/'COPYING',dest/'COPYING')
    version = LOCK['nghttp2']['version']
    template = dest/'src/includes/nghttp2/nghttp2ver.h.in'
    template.with_suffix('').write_text(template.read_text().replace('@PACKAGE_VERSION@',version)
                                      .replace('@PACKAGE_VERSION_NUM@','0x014600'))
    (dest/'src/config.h').write_text('#ifdef _WIN32\n#define HAVE_WINDOWS_H 1\n#define HAVE_WINSOCK2_H 1\n'
        '#else\n#define HAVE_ARPA_INET_H 1\n#define HAVE_NETINET_IN_H 1\n#define HAVE_CLOCK_GETTIME 1\n'
        '#define HAVE_DECL_CLOCK_MONOTONIC 1\n#endif\n')
    cmake = (dest/'src/CMakeLists.txt').read_text()
    sources = re.search(r'set\(NGHTTP2_SOURCES\s+(.*?)\)',cmake,re.S).group(1).split()
    (dest/'sources.txt').write_text('\n'.join(sources)+'\n')
    net = dest/'src/nghttp2_net.h'
    original = net.read_text()
    patch = (dest/'tinycc-byteswap.txt').read_text()
    needle = '#ifdef WIN32\n'
    if original.count(needle) != 1:
        raise SystemExit('nghttp2 byte-swap patch context changed')
    modified = original.replace(needle,needle+patch,1)
    net.write_text(modified)
    (dest/'tinycc-byteswap.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),
        modified.splitlines(True),fromfile='a/lib/nghttp2_net.h',tofile='b/lib/nghttp2_net.h')))

for name, action in [('tcc',tcc),('nghttp2',nghttp2)]:
    if args.only in ('all',name):
        source = unpack(name)
        if source:
            action(source)
print('Dependency bootstrap complete.')
