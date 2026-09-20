"""Black-box compiler tests; run with Python 3, no third-party packages."""
from pathlib import Path
import argparse
import os
import random
import subprocess
import tempfile
import time
import uuid
import re
from contextlib import nullcontext

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default=str(ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')))
parser.add_argument('--cc', help='Differential C compiler, e.g. gcc or clang')
parser.add_argument('--fuzz', type=int, default=40)
opts = parser.parse_args()

RUN = [
    ('hello', 'int main(){var x=10;var y=20;println(x+y);}', '30\n'),
    ('precedence', 'int main(){println(2+3*4);println((2+3)*4);println(16>>2);}', '14\n20\n4\n'),
    ('floats', 'int main(){var x=2.5;println(x*2.0);}', '5\n'),
    ('short_circuit', 'int main(){int x=0;false && ++x;true || ++x;println(x);}', '0\n'),
    ('recursion', 'int fib(int n){if(n<2)return n;return fib(n-1)+fib(n-2);}int main(){println(fib(10));}', '55\n'),
    ('forward', 'int main(){println(add(2,3));}int add(int a,int b){return a+b;}', '5\n'),
    ('named_order', 'int mark(int n){print(n);return n;}int f(int a,int b){return a*10+b;}int main(){println(f(b:mark(2),a:mark(1)));}', '2112\n'),
    ('scope', 'int main(){var x=1;{var x=2;println(x);}println(x);}', '2\n1\n'),
    ('while', 'int main(){int x=0;while(x<4){x++;if(x==2)continue;if(x==4)break;println(x);}}', '1\n3\n'),
    ('for', 'int main(){for(var i=0;i<4;i++){if(i==1)continue;println(i);}}', '0\n2\n3\n'),
    ('defer_lifo', 'int main(){defer println(1);defer println(2);println(3);return 0;}', '3\n2\n1\n'),
    ('defer_return', 'int f(){int x=4;defer x=9;return x;}int main(){println(f());}', '4\n'),
    ('defer_branch', 'int main(){if(false){defer println(9);}if(true){defer println(2);println(1);}println(3);}', '1\n2\n3\n'),
    ('defer_loop', 'int main(){for(int i=0;i<3;i++){defer println(i);if(i==0)continue;if(i==1)break;}}', '0\n1\n'),
    ('array_slice', 'int main(){int[4] a={1,2,3,4};var s=a[1:3];s[0]=20;println(a[1]);println(len(s));}', '20\n2\n'),
    ('range', 'int main(){int[3] a={2,4,6};var sum=0;for(i,v in a){sum+=v+i;}println(sum);}', '15\n'),
    ('string', 'int main(){var s="TinyC+";println(s[0:4]);assert(s=="TinyC+");assert(s!="other");println(s.length);}', 'Tiny\n6\n'),
    ('string_nul', 'int main(){var s="a\\0b";assert(len(s)==3);assert(s[1]==\'\\0\');println("ok");}', 'ok\n'),
    ('tuples', '(int,Error) f(int x){if(x==0)return (0,1);return (20/x,0);}int main(){var v,e=f(4);println(v);println(e);}', '5\n0\n'),
    ('memory', 'int main(){int* p=new int(42);println(*p);*p=7;println(*p);delete p;}', '42\n7\n'),
    ('pointer', 'int main(){int a=5;int* p=&a;*p=10;println(a);}', '10\n'),
    ('ffi', 'extern C{int puts(const char* text);}int main(){var s="ffi";puts(s.data);}', 'ffi\n'),
    ('class', 'class Person{string name;int age;void show(){println(name);}}int main(){Person p("Ana",30);p.show();}', 'Ana\n'),
    ('constructor', 'class Box{int value;Box(int value){this.value=value;}~Box(){println(value);}int get(){return value;}}int main(){Box b(42);println(b.get());}', '42\n42\n'),
    ('heap_class', 'class Box{int value;Box(int n){value=n;}~Box(){println(value);}}int main(){Box* p=new Box(9);delete p;}', '9\n'),
    ('property', 'class Rect{int w;int h;property int area{get=>w*h;}}int main(){Rect r(3,4);println(r.area);}', '12\n'),
    ('extension', 'class Box{int n;}extension Box{int twice(){return n*2;}}int main(){Box b(8);println(b.twice());}', '16\n'),
    ('large_integer', 'int main(){i64 x=5000000000;println(x);}', '5000000000\n'),
    ('generic_class', 'class Box<T>{T value;T get(){return value;}}int main(){Box<int> x(42);Box<string> s("generic");println(x.get());println(s.get());}', '42\ngeneric\n'),
    ('generic_function', 'T identity<T>(T x){return x;}int main(){println(identity<int>(42));println(identity<string>("yes"));}', '42\nyes\n'),
    ('array', 'import std.collections;int main(){var a=Array<int>.create();defer a.destroy();a.push(10);a.push(20);a[0]=11;for(i,v in a){println(v);}println(a.pop());}', '11\n20\n20\n'),
    ('interface', 'interface Reader{int read();}class Box{int n;int read(){return n;}}int consume(Reader r){return r.read();}int main(){Box b(42);Reader r=&b;println(r.read());println(consume(&b));}', '42\n42\n'),
    ('named_identifiers', 'int f(int a,int b){return a*10+b;}int main(){int x=4;int y=2;println(f(b:y,a:x));}', '42\n'),
    ('lambda', 'int main(){var square=(int x)=>x*x;println(square(7));}', '49\n'),
    ('lambda_callback', 'int apply(func<int(int)> f,int x){return f(x);}int main(){println(apply(x=>x*2,21));}', '42\n'),
    ('function_value', 'int a(int x){return x+1;}int b(int x){return x+2;}int main(){var f=a;f=b;println(f(40));}', '42\n'),
    ('closure', 'int main(){int factor=3;var multiply=(int x)=>x*factor;factor=9;println(multiply(14));}', '42\n'),
    ('owned_closure', 'closure<int(int)> make(int factor){return owned((int x)=>x*factor);}int main(){var f=make(6);defer f.destroy();println(f(7));}', '42\n'),
    ('threads', 'import std.concurrent;void worker(void* p){int* value=p;*value=42;}int main(){int value=0;var t=Thread.start(worker,&value);assert(t.join()==0);println(value);}', '42\n'),
    ('channels', 'import std.concurrent;int main(){var c=Channel<int>.create(2);defer c.destroy();assert(c.send(42)==0);var value,error=c.receive();assert(error==0);println(value);c.close();var empty,closed=c.receive();assert(closed!=0);}', '42\n'),
    ('spawn', 'int calculate(int n){return n*2;}int main(){var task=spawn calculate(21);defer task.destroy();println(task.get());}', '42\n'),
    ('async_ready', 'async int value(){return 21;}async int twice(){var n=await value();return n*2;}int main(){var task=twice();defer task.destroy();println(task.get());}', '42\n'),
    ('async_one_worker', 'import std.concurrent;async int consumer(Future<int> future){var n=await future;return n+1;}void producer(Future<int> future){future.complete(41);}int main(){var future=Future<int>.create();defer future.destroy();var a=consumer(future);defer a.destroy();var b=spawn producer(future);defer b.destroy();println(a.get());b.get();}', '42\n'),
    ('async_loop', 'async int value(int n){return n;}async int sum(){int total=0;defer println(99);for(int i=0;i<3;i++){total+=await value(i+1);}return total;}int main(){var task=sum();defer task.destroy();println(task.get());}', '99\n6\n'),
    ('stdlib_string', 'import std.string;int main(){var s=String.concat("Tiny","C+");defer s.destroy();println(s.view());var number,error=String.parseInt("42");assert(error==0);println(number);assert(String.contains(s.view(),"C+"));}', 'TinyC+\n42\n'),
    ('stdlib_fs', 'import std.fs;int main(){assert(File.writeAll("build/test-file.txt","content")==0);var s,error=File.readAll("build/test-file.txt");assert(error==0);defer s.destroy();println(s.view());assert(File.remove("build/test-file.txt")==0);}', 'content\n'),
    ('timer', 'import std.time;async int wait(){await Timer.after(2);return 42;}int main(){var t=wait();defer t.destroy();println(t.get());}', '42\n'),
    ('network', 'import std.net;async int server(TcpSocket listener){var socket=await listener.acceptAsync();defer socket.close();byte[8] buffer={};var n=await socket.readAsync(buffer[:]);assert(n==3);assert(buffer[0]==1);return 42;}int main(){var listener,error=TcpSocket.listen("127.0.0.1",0);assert(error==0);defer listener.close();var serving=server(listener);defer serving.destroy();var connecting=TcpSocket.connectAsync("127.0.0.1",listener.port());defer connecting.destroy();var client=connecting.get();defer client.close();byte[3] data={1,2,3};assert(client.write(data[:])==3);println(serving.get());}', '42\n'),
    ('switch_int', 'int f(int x){switch(x){case -1:return 100;case 0,1,2:return 1;case 0x10:return 16;default:return 0;}}int main(){println(f(-1));println(f(2));println(f(16));println(f(9));}', '100\n1\n16\n0\n'),
    ('switch_enum', 'enum Color{Red,Green=5,Blue}string name(Color c){switch(c){case Color.Red:return \"red\";case Color.Green,Color.Blue:return \"cold\";}}int main(){println(name(Color.Red));println(name(Color.Blue));}', 'red\ncold\n'),
    ('switch_string_char', 'int main(){string s=\"stop\";switch(s){case \"go\":println(1);case \"stop\":println(2);default:println(3);}char c=\'b\';switch(c){case \'a\':println(4);case \'b\':println(5);default:{}}}', '2\n5\n'),
    ('switch_loop_control', 'int main(){int total=0;for(int i=0;i<5;i++){switch(i){case 2:continue;case 4:total+=100;default:total+=i;}}println(total);}', '104\n'),
    ('switch_defer_scope', 'int main(){switch(1){case 1:defer println(2);println(1);default:{}}println(3);}', '1\n2\n3\n'),
    ('switch_async', 'import std.concurrent;async int f(int x,Future<int> input){int r=0;switch(x){case 1:var n=await input;r=n+10;default:r=x*2;}return r;}void produce(Future<int> o){o.complete(5);}int main(){var input=Future<int>.create();defer input.destroy();var t=f(1,input);defer t.destroy();var p=spawn produce(input);defer p.destroy();println(t.get());p.get();}', '15\n'),
    ('switch_keywords_as_identifiers', 'int main(){int case=1;int default=2;println(case+default);}', '3\n'),
    ('hash_builtin', 'int main(){assert(hash(42)==hash(42));assert(hash(\"abc\")==hash(\"abc\"));assert(hash(\"abc\")!=hash(\"abd\"));int x=0;assert(hash(&x)==hash(&x));println(\"ok\");}', 'ok\n'),
    ('hash_user_function', 'u64 hash(int x){return 7;}int main(){println(hash(3));}', '7\n'),
    ('map_string', 'import std.collections;int main(){var m=Map<string,int>.create();defer m.destroy();assert(m.put(\"one\",1));assert(m.put(\"two\",2));assert(!m.put(\"one\",11));var v,ok=m.get(\"one\");assert(ok);println(v);println(m.getOr(\"three\",-1));assert(m.remove(\"two\"));assert(!m.contains(\"two\"));println(m.length);}', '11\n-1\n1\n'),
    ('map_growth_tombstones', 'import std.collections;int main(){var m=Map<i64,i64>.create();defer m.destroy();for(i64 i=0;i<20000;i++)m.put(i*7919,i);for(i64 i=0;i<20000;i+=2)assert(m.remove(i*7919));i64 sum=0;for(i64 i=0;i<20000;i++){var x,found=m.get(i*7919);if(found)sum+=x;assert(found==(i%2==1));}println(m.length);println(sum);var cap=m.capacity;for(int r=0;r<50;r++){for(i64 i=1;i<=500;i++)m.put(-i,i);for(i64 i=1;i<=500;i++)assert(m.remove(-i));}assert(m.capacity==cap);println(m.length);}', '10000\n100000000\n10000\n'),
    ('map_enum_keys', 'import std.collections;enum Kind{Fn,Var,Type}int main(){var m=Map<Kind,string>.create();defer m.destroy();m.put(Kind.Fn,\"function\");m.put(Kind.Type,\"type\");println(m.getOr(Kind.Type,\"?\"));println(m.getOr(Kind.Var,\"?\"));var keys=Array<Kind>.create();defer keys.destroy();m.keys(&keys);println(keys.length);m.clear();println(m.length);}', 'type\n?\n2\n0\n'),
]
FAIL = [
    ('switch_not_exhaustive', 'enum C{A,B,D}int main(){C c=C.A;switch(c){case C.A:println(1);case C.B:println(2);}}', 'not exhaustive: missing C.D'),
    ('switch_duplicate', 'int main(){switch(1){case 0x10:println(1);case 16:println(2);}}', 'duplicate case label'),
    ('switch_break', 'int main(){switch(1){case 1:break;}}', 'cases never fall through'),
    ('switch_label_constant', 'int main(){int y=2;switch(1){case y:println(1);}}', 'must be a literal or an enum member'),
    ('switch_subject_type', 'int main(){double d=1.0;switch(d){default:{}}}', 'switch requires an integer'),
    ('switch_two_defaults', 'int main(){switch(1){default:{}default:{}}}', 'more than one default'),
    ('switch_missing_return', 'int f(int x){switch(x){case 1:return 1;}}', 'not all paths return'),
    ('hash_float', 'int main(){println(hash(1.5));}', 'hash supports'),
    ('undefined', 'int main(){println(missing);}', 'undefined identifier'),
    ('duplicate', 'int main(){var x=1;var x=2;}', 'duplicate declaration'),
    ('var_initializer', 'int main(){var x;}', 'var requires an initializer'),
    ('wrong_type', 'int main(){int x="bad";}', 'expected i32'),
    ('return_type', 'int f(){return "bad";}', 'expected i32'),
    ('missing_return', 'int f(int x){if(x)return 1;}', 'not all paths return'),
    ('break', 'int main(){break;}', 'outside a loop'),
    ('continue', 'int main(){continue;}', 'outside a loop'),
    ('call_args', 'void f(int x){}int main(){f();}', 'missing argument'),
    ('bad_label', 'void f(int x){}int main(){f(y:1);}', 'unknown argument label'),
    ('double_label', 'void f(int x){}int main(){f(x:1,x:2);}', 'supplied twice'),
    ('bad_lvalue', 'int main(){1=2;}', 'mutable location'),
    ('bad_index', 'int main(){int[2] a={};println(a[1.2]);}', 'index must be an integer'),
    ('too_many_elements', 'int main(){int[1] a={1,2};}', 'too many array initializers'),
    ('bad_delete', 'int main(){delete 4;}', 'delete requires a pointer'),
    ('bad_pointer', 'int main(){println(*42);}', 'cannot dereference'),
    ('unterminated', 'int main(){println("oops);}', 'unterminated literal'),
    ('comment', 'int main(){/*oops', 'unterminated block comment'),
    ('numeric_suffix', 'int main(){var x=42oops;}', 'numeric literal suffix'),
    ('huge_number', 'int main(){var x=9999999999999999999999999;}', 'out of range'),
    ('deep', 'int main(){println('+'('*300+'1'+')'*300+');}', 'nesting limit'),
]
RUNTIME_FAIL = [
    ('switch_enum_out_of_range', 'enum C{A,B}int f(C c){switch(c){case C.A:return 1;case C.B:return 2;}}int main(){println(f(cast<C>(7)));}', 'switch value matches no enum case'),
    ('bounds', 'int main(){int[2] a={1,2};println(a[2]);}', 'index out of bounds'),
    ('negative_bounds', 'int main(){int[2] a={1,2};println(a[-1]);}', 'index out of bounds'),
    ('slice_bounds', 'int main(){int[2] a={1,2};var s=a[1:3];}', 'slice out of bounds'),
    ('assert', 'int main(){assert(false);}', 'assertion failed'),
]

RUN += [
    ('return_resource_tuple', 'class Box{int value;~Box(){println(value);}}(Box,Error) make(){Box b(42);return (b,0);}int main(){var b,error=make();assert(error==0);println(b.value);}', '42\n42\n'),
    ('return_resource', 'class Box{int* p;Box(int n){p=new int(n);}~Box(){println(*p);delete p;}}Box make(){Box b(42);return b;}int main(){var b=make();println(*b.p);}', '42\n42\n'),
    ('async_file_dns', 'import std.net;import std.fs;int main(){var writing=AsyncFile.writeAll("build/async-test.txt","async");defer writing.destroy();assert(writing.result()==0);var reading=AsyncFile.readAll("build/async-test.txt");defer reading.destroy();var bytes,error=reading.result();assert(error==0);OwnedString text(bytes);defer text.destroy();println(text.view());assert(File.remove("build/async-test.txt")==0);var query=Dns.resolveAsync("localhost");defer query.destroy();var address,dnsError=query.result();assert(dnsError==0);OwnedString owned(address);defer owned.destroy();assert(len(address)>0);}', 'async\n'),
    ('udp_async', 'import std.net;int main(){var a,e=UdpSocket.bind("127.0.0.1",0);assert(e==0);defer a.close();var b,e2=UdpSocket.bind("127.0.0.1",0);assert(e2==0);defer b.close();byte[3] buffer={};var reading=b.receiveAsync(buffer[:]);defer reading.destroy();byte[3] data={1,2,3};Address peer("127.0.0.1",b.port());assert(a.sendTo(peer,data[:])==3);assert(reading.get()==3);println(buffer[2]);}', '3\n'),
    ('spawn_function_pointer', 'int a(int x){return x+1;}int b(int x){return x+2;}int main(){var f=a;f=b;var t=spawn f(40);defer t.destroy();println(t.get());}', '42\n'),
    ('hex_bytes', 'int main(){var s="\\x00\\xffab";assert(len(s)==4);println(cast<byte>(s[1]));println(cast<int>(\'\\x41\'));}', '255\n65\n'),
    ('promotions', 'int main(){char a=100;char b=100;println(a+b);u8 x=255;println(x+1);println(~cast<u8>(0));}', '200\n256\n-1\n'),
    ('constructor_expression', 'class Point { int x; int y; } int main(){var p=Point(3,4);println(p.x+p.y);}', '7\n'),
    ('nested_capture', 'int main(){int factor=10;var outer=(int x)=>{var inner=(int y)=>x+y+factor;return inner(2);};factor=99;println(outer(3));}', '15\n'),
    ('const_capture', 'int main(){const int factor=7;var closure=(int x)=>x+factor;println(closure(2));}', '9\n'),
    ('async_const', 'import std.time;async int work(){const int x=7;await Timer.after(1);return x;}int main(){var t=work();defer t.destroy();println(t.get());}', '7\n'),
    ('task_result', 'import std.concurrent;async int work(Future<int> f){return await f;}int main(){var f=Future<int>.create();defer f.destroy();var t=work(f);defer t.destroy();f.complete(0,7);var value,error=t.result();println(error);}', '7\n'),
    ('task_group', 'import std.concurrent;int work(int x){return x*2;}int main(){var group=TaskGroup<int>.create();defer group.destroy();for(int i=0;i<20;i++){var t=spawn work(i);group.add(t);t.destroy();}println(group.wait());}', '0\n'),
    ('stream_pipeline', (ROOT/'examples'/'streams.tc').read_text(), '24\n40\n60\n'),
    ('stream_capture_reduce', 'int main(){int[3] a={1,2,3};int add=2;println(a.stream().reduce(0,(sum,x)=>sum+x+add));}', '12\n'),
    ('stream_empty', 'int main(){int[1] a={1};var s=a[0:0];println(s.stream().all(x=>x>0));println(s.stream().any(x=>x>0));println(s.stream().reduce(9,(a,b)=>a+b));}', '1\n0\n9\n'),
]
FAIL += [
    ('const_field', 'class Box{int value;}int main(){const Box b=Box(1);b.value=2;}', 'const location'),
    ('const_array', 'int main(){const int[2] a={1,2};a[0]=3;}', 'const location'),
    ('instance_required', 'class Box{void show(){}}int main(){Box.show();}', 'requires an object'),
    ('const_increment', 'int main(){const int x=2;x++;}', 'mutable location'),
    ('hex_short', 'int main(){println("\\x1");}', 'exactly two digits'),
    ('stream_missing_terminal', 'int main(){int[1] a={1};var s=a.stream().filter(x=>x>0);}', 'terminal operation'),
    ('stream_bad_map', 'int main(){int[1] a={1};a.stream().map(x=>println(x)).count();}', 'cannot return void'),
]

count = 0
start = time.monotonic()
errors = []
scratch = ROOT / 'build' / 'tests'
scratch.mkdir(parents=True, exist_ok=True)
os.environ['TMP'] = os.environ['TEMP'] = str(scratch)
os.environ['TINY_WORKERS'] = '1'
run_folder = scratch / ('run ' + uuid.uuid4().hex)
run_folder.mkdir()
with nullcontext(run_folder) as folder:
    folder = Path(folder)
    assert folder.resolve().is_relative_to(scratch.resolve())
    def batch(cases, mode, tag, extra=()):
        manifest=folder/(tag+'.tsv')
        lines=[]
        for name,src,_ in cases:
            path=folder/(name+'.tc'); path.write_text(src,encoding='utf-8')
            lines.append(str(mode)+'\t'+str(path)+'\n')
        manifest.write_text(''.join(lines),encoding='utf-8')
        p=subprocess.run([opts.compiler,'--batch',str(manifest),'--home',str(ROOT),*extra],cwd=ROOT,capture_output=True,text=True,timeout=300)
        results={int(i):(int(code),out) for i,out,j,code in re.findall(r'@@BEGIN (\d+)\n(.*?)@@END (\d+) (-?\d+)\n',p.stdout,re.S) if i==j}
        return results,p.stderr
    def invoke(name, src, command='run', extra=()):
        path=folder/(name+'.tc')
        path.write_text(src,encoding='utf-8')
        return subprocess.run([opts.compiler,command,str(path),'--home',str(ROOT),*extra],cwd=ROOT,capture_output=True,text=True,timeout=30)
    results,diagnostics=batch(RUN,5,'run')
    print('Native run batch finished',flush=True)
    differential = None
    if opts.cc:
        differential, diff_diagnostics = batch(RUN,5,'differential',('--cc',opts.cc))
        print('Full differential run batch finished',flush=True)
    for idx,(name, src, expected) in enumerate(RUN):
        try:
            code,out=results.get(idx,(-999,''))
            assert code==0 and out==expected, (code,out,diagnostics)
            if differential is not None:
                code,out=differential.get(idx,(-999,''))
                assert code==0 and out==expected, ('differential',code,out,diff_diagnostics)
                count+=1
            count+=1
        except Exception as e: errors.append((name,str(e)))
    results,diagnostics=batch(FAIL,1,'fail')
    print('Negative compilation batch finished',flush=True)
    for idx,(name,src,expected) in enumerate(FAIL):
        try:
            code,out=results.get(idx,(-999,''))
            relevant='\n'.join(line for line in diagnostics.splitlines() if f'{name}.tc:' in line)
            assert code==1 and expected in relevant, (code,relevant)
            count+=1
        except Exception as e: errors.append((name,str(e)))
    for name,src,expected in RUNTIME_FAIL:
        try:
            p=invoke(name,src)
            assert p.returncode==101 and expected in p.stderr, (p.returncode,p.stdout,p.stderr)
            count+=1
        except Exception as e: errors.append((name,str(e)))
    rng=random.Random(1729)
    alphabet='{}()[];,:+-*/%&|= abcdef0123456789\n"'
    fuzz=[]
    for i in range(opts.fuzz):
        src=''.join(rng.choice(alphabet) for _ in range(rng.randrange(1,500)))
        fuzz.append(('fuzz_'+str(i),src,''))
    results,diagnostics=batch(fuzz,1,'fuzz')
    for i in range(opts.fuzz):
        try:
            code,out=results.get(i,(-999,''))
            assert code in (0,1), (code,diagnostics)
            count+=1
        except Exception as e: errors.append(('fuzz_'+str(i),str(e)))
    try:
        p=invoke('tokens','// comment\nint main(){var a=0xff;}','--emit-tokens')
        assert p.returncode==0 and 'integer\t0xff' in p.stdout and 'comment' not in p.stdout
        p=invoke('ast','int main(){var a=2+3*4;}','--emit-ast')
        assert p.returncode==0 and 'FunctionDecl main' in p.stdout and 'Binary *' in p.stdout
        p=invoke('typed_ast','int main(){var a=2+3*4;}','--emit-typed-ast')
        assert p.returncode==0 and 'VarDecl a @1:12 : i32' in p.stdout and 'Binary + @1:19 : i32' in p.stdout and 'Int 3 @1:20 : i32' in p.stdout
        untyped=invoke('ast','int main(){var a=2+3*4;}','--emit-ast')
        assert untyped.returncode==0 and 'Binary + @1:19\n' in untyped.stdout and ' : ' not in untyped.stdout
        p=invoke('typed_ast_error','int main(){int x="text";}','--emit-typed-ast')
        assert p.returncode==1 and p.stdout=='' and 'expected i32, found string' in p.stderr
        p=invoke('lowering','int main(){defer println(8);return 0;}','--emit-c')
        assert p.returncode==0 and '#line 1 ' in p.stdout and p.stdout.index('tc_print_integer')<p.stdout.index('return tc_tmp')
        count+=5
    except Exception as e: errors.append(('inspection',str(e)))

for name,error in errors: print('FAIL',name,error)
print(f'{count} passed; {len(errors)} failed; {time.monotonic()-start:.2f}s')
raise SystemExit(bool(errors))
