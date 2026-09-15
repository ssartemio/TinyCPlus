#!/usr/bin/env python3
"""Small reproducible benchmarks; outputs verified, wall time includes process startup."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc')
parser.add_argument('--iterations', type=int, default=7)
args = parser.parse_args()
if args.iterations < 1:
    parser.error('--iterations must be positive')
folder = ROOT/'build/benchmarks'
folder.mkdir(parents=True, exist_ok=True)
ext = '.exe' if os.name == 'nt' else ''
tiny = ROOT/'bin'/('tiny'+ext)
programs = {
    'arithmetic': ('int main(){i64 sum=0;for(int i=0;i<1000000;i++){sum+=i%97;}println(sum);}', '47999055\n'),
    'fused_stream': ('import std.collections;int main(){var a=Array<int>.create();defer a.destroy();for(int i=0;i<100000;i++)a.push(i);println(a.stream().filter(x=>x%2==0).map(x=>cast<i64>(x)*2).reduce(cast<i64>(0),(sum,x)=>sum+x));}', '4999900000\n'),
    'interface': ('interface Reader{int read();}class Source{int value;int read(){return value;}}int main(){Source source(3);Reader reader=&source;i64 sum=0;for(int i=0;i<1000000;i++)sum+=reader.read();println(sum);}', '3000000\n'),
    'owned_closure': ('int main(){int factor=3;var f=owned((int x)=>x*factor);defer f.destroy();i64 sum=0;for(int i=0;i<1000000;i++)sum+=f(i%7);println(sum);}', '8999991\n'),
    'allocation': ('int main(){i64 sum=0;for(int i=0;i<100000;i++){int* p=new int(i);sum+=*p;delete p;}println(sum);}', '4999950000\n'),
    'async_tasks': ('async int value(int n){return n+1;}int main(){i64 sum=0;for(int i=0;i<10000;i++){var task=value(i);sum+=task.get();task.destroy();}println(sum);}', '50005000\n'),
}

def measured(command):
    start = time.perf_counter()
    process = subprocess.Popen(list(map(str,command)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        out, err = process.communicate(timeout=120)
    except subprocess.TimeoutExpired:
        process.kill(); process.communicate(); raise
    elapsed = (time.perf_counter()-start)*1000
    peak = None
    if os.name == 'nt':
        class Counters(ctypes.Structure):
            _fields_ = [('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(name,ctypes.c_size_t) for name in
                ('peak_working_set','working_set','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile')]
        counters = Counters(); counters.cb = ctypes.sizeof(counters)
        if ctypes.windll.psapi.GetProcessMemoryInfo(ctypes.c_void_p(int(process._handle)),ctypes.byref(counters),counters.cb):
            peak = counters.peak_working_set
    if process.returncode:
        raise RuntimeError((command,process.returncode,out,err))
    return elapsed,peak,out

results=[]
for name,(source,expected) in programs.items():
    path=folder/(name+'.tc'); path.write_text(source)
    executable=folder/(name+ext)
    compile_ms=[]; run_ms=[]; compile_peak=[]; run_peak=[]
    for i in range(args.iterations):
        elapsed,peak,_=measured([tiny,'build',path,'-o',executable,*(['--cc',args.cc] if args.cc else [])])
        compile_ms.append(elapsed)
        if peak is not None: compile_peak.append(peak)
        elapsed,peak,output=measured([executable])
        assert output == expected,(name,output,expected)
        run_ms.append(elapsed)
        if peak is not None: run_peak.append(peak)
    item=dict(program=name,source_bytes=len(source),executable_bytes=executable.stat().st_size,
              compile_median_ms=round(statistics.median(compile_ms),3),run_median_ms=round(statistics.median(run_ms),3),
              compile_peak_working_set_bytes=max(compile_peak) if compile_peak else None,
              run_peak_working_set_bytes=max(run_peak) if run_peak else None)
    results.append(item); print(item,flush=True)
report=dict(platform=platform.platform(),machine=platform.machine(),processor=platform.processor(),
            backend=args.cc or 'bundled libtcc',iterations=args.iterations,
            method='Wall time includes startup and shutdown; Windows peak working set includes compiler process, not external compiler children. No baseline comparison or allocation instrumentation.',results=results)
(folder/'results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
rows=['# Mediciones locales','',report['method'],'',f"Host: {report['platform']}; backend: {report['backend']}; repeticiones: {args.iterations}.",'',
      '| Programa | Compilar (mediana ms) | Ejecutar (mediana ms) | Ejecutable bytes | RAM máxima compilador bytes |',
      '|---|---:|---:|---:|---:|']
for r in results:
    rows.append(f"| {r['program']} | {r['compile_median_ms']} | {r['run_median_ms']} | {r['executable_bytes']} | {r['compile_peak_working_set_bytes']} |")
(folder/'results.md').write_text('\n'.join(rows)+'\n',encoding='utf-8')
print('Reports:',folder/'results.json',folder/'results.md')
