"""Differential tests between stage 0 (compiler/, C) and stage 1 (selfhost/, TinyC+).

Stage 1 is built with stage 0 and must behave exactly like it: same stdout, same stderr and
the same exit status. Today that covers the lexer (`--emit-tokens`) and the parser, both its
tree (`--emit-ast`) and the type table it builds (`--emit-types`); each later phase adds an
entry to PHASES. The typed syntax tree that stage 0
prints (`--emit-typed-ast`) is checked here too, because it is the reference the semantic
phase will be compared against.
"""
from pathlib import Path
import argparse
import os
import random
import re
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cc', help='External C compiler instead of the bundled libtcc')
parser.add_argument('--fuzz', type=int, default=300, help='deterministic random lexer inputs')
parser.add_argument('--mutations', type=int, default=400, help='deterministic mutations of repository sources')
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

# Each phase that stage 1 has ported is the stage 0 flag that prints that phase's output.
# The parser leaves two observable results: the tree and the type table it built on the way.
PARSE_PHASES = ['--emit-ast', '--emit-types']
PHASES = ['--emit-tokens'] + PARSE_PHASES


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
    for flag in PHASES:
        compare('repo file %s %s' % (path.relative_to(ROOT).as_posix(), flag), path.relative_to(ROOT).as_posix(), flag)

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
    for flag in PHASES:
        compare('case %s %s' % (name, flag), source.relative_to(ROOT).as_posix(), flag)

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
    for flag in PHASES:
        compare('fuzz %d %s' % (index, flag), source.relative_to(ROOT).as_posix(), flag)

# ---- 3b. parser: grammar corners, the type table, limits and syntax errors ------------------
def nest(opening, closing, depth, core='1'):
    return opening * depth + core + closing * depth


PARSER_CASES = [
    ('p_empty_function', 'void f(){}'),
    ('p_hello', 'int main(){var x=10;var y=20;println(x+y);}'),
    ('p_return_forms', 'int f(){return;} int g(){return 1;} (int,int) h(){return 1,2;}'),
    ('p_if_else_chain', 'void f(){if(a)b();else if(c)d();else{e();}}'),
    ('p_loops', 'void f(){while(a){break;}for(;;){continue;}for(int i=0;i<3;i++){}for(i=0;i<3;i++){}}'),
    ('p_range_for', 'void f(){for(v in xs){}for(i,v in xs){}}'),
    ('p_for_expr_init', 'void f(){for(x=1;x<3;x++)g();}'),
    ('p_defer_delete', 'void f(){defer g();defer{h();}delete p;}'),
    ('p_precedence', 'int f(){return 1+2*3-4/5%6<<1>>2<3>4<=5>=6==7!=8&9^10|11&&12||13;}'),
    ('p_assign_right_assoc', 'void f(){a=b=c=1;a+=b-=c*=2;}'),
    ('p_unary_chain', 'int f(){return - - !~*&a++ + --b;}'),
    ('p_postfix', 'void f(){a[1]++;b.c--;d->e++;}'),
    ('p_member_chain', 'void f(){a.b.c->d.e(1)(2)[3].f;}'),
    ('p_index_slice', 'void f(){a[1];a[1:2];a[:2];a[1:];a[:];}'),
    ('p_named_args', 'void f(){g(a:1,b:x+1);}'),
    ('p_tuple_init', 'void f(){var t=(1,2,3);int[3] a={1,2,3};var e={};}'),
    ('p_cast_sizeof', 'void f(){cast<int>(x);cast<Foo*>(p);sizeof(int);sizeof(Foo[4]);}'),
    ('p_new', 'void f(){new int(42);new Foo;new Foo(1,2);new int[3](0);}'),
    ('p_await_spawn', 'async int f(){var x=await g();var t=spawn h(1);return await x;}'),
    ('p_lambda_forms', 'void f(){var a=x=>x+1;var b=(x,y)=>x+y;var c=(int x)=>{return x;};var d=()=>1;var e=x=>{return x;};}'),
    ('p_lambda_typed_params', 'void f(){var g=(int a,Foo b,const Bar* c)=>a;}'),
    ('p_var_multi', 'void f(){var a,b=g();var c=1;const var d=2;}'),
    ('p_const_types', 'const int x=1; void f(const int* p,const string s){const Foo f;}'),
    ('p_var_decl_forms', 'void f(){int a;int b=1;Foo c(1,2);Foo d;Foo* e;int[3] f;int g[3];int h[3]=x;}'),
    ('p_array_len_hex', 'int[0x10] a; int[010] b; int[10000000] c;'),
    ('p_array_len_zero', 'int[0] a;'),
    ('p_array_len_octal_zero', 'int[08] a;'),
    ('p_array_len_big', 'int[10000001] a;'),
    ('p_array_len_overflow', 'int[18446744073709551616] a;'),
    ('p_array_len_ident', 'int[n] a;'),
    ('p_array_len_var_zero', 'void f(){int a[0];}'),
    ('p_array_len_var_ident', 'void f(){int a[n];}'),
    ('p_array_len_var_big', 'void f(){int a[10000001];}'),
    ('p_array_len_var_overflow', 'void f(){int a[99999999999999999999999];}'),
    ('p_generic_types', 'Array<int> a; Map<string,Array<int>> m; Map<string,Array<Array<int>>> n;'),
    ('p_generic_shift_split', 'Array<Array<int>> a; void f(){Array<Array<Array<int>>> b;}'),
    ('p_generic_call', 'void f(){identity<int>(1);identity<Foo<int>>(2);Foo<int>.create();}'),
    ('p_generic_call_registers_name', 'void f(){identity<int>(1);identity y;}'),
    ('p_generic_call_not_type', 'void f(){a<b>c;}'),
    ('p_less_greater', 'void f(){x=a<b;y=a>b;z=a<b>(c);}'),
    ('p_slice_type', 'Slice<int> a; void f(Slice<byte> s){}'),
    ('p_slice_two', 'Slice<int,int> a;'),
    ('p_func_types', 'func<int(int,int)> f; closure<void()> c; func<Foo*(Bar)> g;'),
    ('p_func_type_registers_function', 'func<int()> f; void g(){Function h;}'),
    ('p_func_type_errors', 'func<int> f;'),
    ('p_func_type_registers_function_alone', 'func<int()> f; void g(){Function;}'),
    ('p_closure_type_registers_function_alone', 'closure<void()> c; void g(){Function;}'),
    ('p_generic_type_registers_name_alone', 'Box<int> b; void g(){Box;}'),
    ('p_tuple_id_is_a_type_name', '(int,int) a; void f(){tc_tuple_3 x;}'),
    ('p_tuple_id_off_by_one', '(int,int) a; void f(){tc_tuple_4 x;}'),
    ('p_tuple_interned', '(int,int) a; (int,int) b; (int,string) c; (int,string) d; ((int,int),int) e; ((int,int),int) f;'),
    ('p_tuple_qualified_const', 'const (int,int) a; (int,int) b;'),
    ('p_ids_shift_with_nodes', 'void f(){int a=1+2+3+4;int b=a*2;} (int,int) t; int[3] arr;'),
    ('p_ids_array_names', 'int[3] a; int[4] b; int[3] c; int*[3] d; Foo[2] e;'),
    ('p_ids_after_statements', 'void f(){g(1,2,3);h(4);} int[5] late; (int,Error) pair;'),
    ('p_types_const_interned', 'const int a; const int b; const int* c; const Foo d; const Foo e; Foo f; const const int g;'),
    ('p_types_const_before_class', 'const Foo a; class Foo{int x;} const Foo b;'),
    ('p_types_size_t', 'size_t a; u64 b; size_t c; const size_t d; size_t* e; size_t[2] f;'),
    ('p_types_size_t_after_u64', 'u64 a; size_t b; u64 c;'),
    ('p_types_pointers_interned', 'int* a; int* b; int** c; int** d; Foo* e; Foo* f;'),
    ('p_types_slices', 'Slice<int> a; Slice<int> b; Slice<Foo> c; Slice<Slice<int>> d;'),
    ('p_types_generic_interned', 'Box<int> a; Box<int> b; Box<string> c; Box<Box<int>> d; Box<Box<int>> e; Map<int,Box<int>> f;'),
    ('p_types_generic_then_plain', 'Box<int> a; Box b; class Box<T>{T v;}'),
    ('p_types_func_never_shared', 'func<int(int)> a; func<int(int)> b; closure<int(int)> c; func<void()> d; func<void()> e;'),
    ('p_types_func_return_types', 'func<Foo*(const Bar,(int,int))> a; func<func<int()>()> b;'),
    ('p_types_alias_share_primitive', 'int a; i32 b; Error c; uint d; u32 e; byte f; u8 g; short h; long i; ulong j; StringView k; string l;'),
    ('p_types_var_and_auto', 'void f(){var a=1;const var b=2;var* c;}'),
    ('p_types_qualified_names', 'lib.Point a; lib.Point b; lib.sub.Thing c; lib.Point* d;'),
    ('p_types_enum_and_class_registered', 'enum E{A} class C{} interface I{} struct S{} extension X{} E e; C c; I i; S s; X x;'),
    ('p_types_generic_params_are_types', 'class Box<T,U>{T a;U b;} T free; U* other;'),
    ('p_types_function_param_generics', 'T id<T>(T x){return x;} T* p;'),
    ('p_types_extern_cnames', 'extern C{struct FILE{int x;} enum Mode{A,B} int puts(const char* s);} FILE* f; Mode m;'),
    ('p_types_enum_cnames', 'enum Color{Red,Green} extern C{enum Kind{X=1,Y}}'),
    ('p_types_lambda_auto', 'void f(){var g=x=>x;var h=(a,b)=>a;var k=(int a,Foo b)=>a;}'),
    ('p_types_ctor_dtor_void', 'class A{A(){} ~A(){}}'),
    ('p_precedence_relational_then_shift', 'int f(){return a<b<<c>d>>e;}'),
    ('p_precedence_shift_then_relational', 'int f(){return a<<b<c>>d>e;}'),
    ('p_tuple_types', '(int,string) t; (int,Error) f(){return 1,0;} ((int,int),int) n;'),
    ('p_qualified_types', 'lib.Point p; lib.sub.Thing* q; void f(){lib.Point x; lib.f(1);}'),
    ('p_qualified_errors', 'lib. p;'),
    ('p_pointer_array_types', 'int* a; int** b; int*[3] c; Foo[3]* d;'),
    ('p_unknown_lowercase_is_expr', 'void f(){foo x;}'),
    ('p_uppercase_ident_is_decl', 'void f(){Foo x;Foo * y;Foo z[2];}'),
    ('p_uppercase_expression', 'void f(){Foo(1);Foo.bar();Foo;}'),
    ('p_type_table_learns_class', 'class Box{} void f(){Box b;box c;}'),
    ('p_type_table_learns_alias', 'void f(){int x;i32 y;u8 z;size_t n;StringView s;Error e;}'),
    ('p_type_table_null', 'void f(){null x;}'),
    ('p_type_table_learns_from_use', 'Foo a; void f(){Foo;}'),
    ('p_type_start_after_use', 'void f(){widget w;} widget q;'),
    ('p_class_generic_registers_param', 'class Box<T>{T v;} void f(){T x;}'),
    ('p_class_full', 'class A{int x;string s="k";static int n;A(int x){this.x=x;}~A(){}int get(){return x;}}'),
    ('p_class_struct_interface_extension', 'struct S{int a;} interface I{int f();} extension S{int g(){return a;}}'),
    ('p_class_generic', 'class Box<T,U>{T a;U b;T first<V>(V v){return a;}}'),
    ('p_class_semicolon', 'class A{}; class B{};'),
    ('p_class_nested_decl_error', 'class A{ class B{} }'),
    ('p_class_unterminated', 'class A{ int x;'),
    ('p_constructor_forms', 'class A{A(){} A(int x):x(1){}}'),
    ('p_constructor_call_decl', 'class A{A(int x){}} void f(){A a(1);}'),
    ('p_destructor_ok', 'class A{~A(){}}'),
    ('p_destructor_wrong_name', 'class A{~B(){}}'),
    ('p_property_forms', 'class R{int w;property int area{get=>w*2;}property int p;}'),
    ('p_property_errors', 'class R{property int area{set=>1;}}'),
    ('p_modifiers', 'static int a; async int f(){} stdcall int g(); cdecl int h();'),
    ('p_modifiers_in_class', 'class A{static int f(){} async int g(){}}'),
    ('p_annotation_test', '@test void f(){} @test static void g(){}'),
    ('p_annotation_unknown', '@bogus void f(){}'),
    ('p_annotation_missing_name', '@ void f(){}'),
    ('p_extern_block', 'extern C{int puts(const char* t);void* malloc(u64 n);int printf(const char* f,...);}'),
    ('p_extern_wrong_language', 'extern D{int f();}'),
    ('p_extern_unterminated', 'extern C{int f();'),
    ('p_extern_missing_brace', 'extern C int f();'),
    ('p_extern_enum_struct', 'extern C{enum E{A,B} struct S{int x;}}'),
    ('p_params_void', 'void f(void){} void g(){} void h(int a,int b=2,string c="s"){}'),
    ('p_params_variadic', 'int f(const char* fmt,...);'),
    ('p_params_variadic_middle', 'int f(...,int a);'),
    ('p_params_errors', 'void f(int){}'),
    ('p_function_prototype', 'int f(int a); int g(int a){return a;}'),
    ('p_field_initializer', 'class A{int x=1;string s="a";}'),
    ('p_function_generic', 'T id<T>(T x){return x;} T pick<T,U>(T a,U b){return a;}'),
    ('p_function_var_global', 'int counter=0; string name="x"; Foo* head=null;'),
    ('p_module_import', 'module a.b.c; import std.string; import x;'),
    ('p_module_errors', 'module ; import a.;'),
    ('p_enum_plain', 'enum Color{Red,Green,Blue}'),
    ('p_enum_values', 'enum E{A=1,B,C=10,D,E2=-5,F,G=0x10,H=010}'),
    ('p_enum_trailing_comma', 'enum E{A,B,}'),
    ('p_enum_semicolon', 'enum E{A}; enum F{B};'),
    ('p_enum_empty', 'enum E{}'),
    ('p_enum_min', 'enum E{A=-2147483648,B}'),
    ('p_enum_max', 'enum E{A=2147483647}'),
    ('p_enum_max_overflows_next', 'enum E{A=2147483647,B}'),
    ('p_enum_too_big', 'enum E{A=2147483648}'),
    ('p_enum_too_small', 'enum E{A=-2147483649}'),
    ('p_enum_overflow_u64', 'enum E{A=99999999999999999999999}'),
    ('p_enum_non_integer', 'enum E{A=1.5}'),
    ('p_enum_identifier_value', 'enum E{A=B}'),
    ('p_enum_octal_garbage', 'enum E{A=089,B=0779}'),
    ('p_enum_duplicate', 'enum E{A} enum E{B}'),
    ('p_enum_duplicate_alias', 'enum int{A} enum i32{B}'),
    ('p_enum_then_class_duplicate', 'enum E{A} class E{}'),
    ('p_class_duplicate', 'class A{} class A{}'),
    ('p_class_duplicate_alias', 'class int{} class Error{}'),
    ('p_extension_after_class_ok', 'class A{} extension A{} extension A{}'),
    ('p_class_after_reference', 'A a; class A{}'),
    ('p_enum_extern', 'extern C{enum E{X=3,Y}}'),
    ('p_switch_forms', 'int f(int x){switch(x){case 1:return 1;case 2,3,4:{return 2;}case -1:g();h();default:return 0;}}'),
    ('p_switch_empty', 'void f(){switch(x){}}'),
    ('p_switch_only_default', 'void f(){switch(x){default:g();}}'),
    ('p_switch_two_defaults', 'void f(){switch(x){default:g();default:h();}}'),
    ('p_switch_junk_label', 'void f(){switch(x){foo:g();}}'),
    ('p_switch_stmt_first', 'void f(){switch(x){g();}}'),
    ('p_switch_unterminated', 'void f(){switch(x){case 1:g();'),
    ('p_switch_unterminated_label', 'void f(){switch(x){'),
    ('p_switch_word_as_identifier', 'void f(){switch=1;switch(1);}'),
    ('p_switch_contextual_case_default', 'void f(){int default=0;int case=1;switch(1){case 1:default=3;case=case+default;case++;println(default);default:println(9);}}'),
    ('p_switch_case_expression_label', 'void f(){switch(x){case 1+2:g();case (3):h();}}'),
    ('p_switch_case_minus', 'void f(){switch(x){case -1:g();case - 2:h();}}'),
    ('p_switch_case_string_char', 'void f(){switch(s){case "a","b":g();case \'c\':h();}}'),
    ('p_switch_case_enum_member', 'void f(){switch(c){case Color.Red,Color.Blue:g();}}'),
    ('p_switch_nested', 'void f(){switch(a){case 1:switch(b){case 2:g();default:h();}default:i();}}'),
    ('p_true_false_null', 'void f(){var a=true;var b=false;var c=null;}'),
    ('p_keywords_as_names', 'void f(){int new_=1;int sizeof_=2;int cast_=3;int Await=4;}'),
    ('p_string_char_literals', 'void f(){var a="x\\n";var b=\'y\';var c="";}'),
    ('p_float_int_literals', 'void f(){var a=1;var b=1.5;var c=0xFF;var d=1e3;}'),
    ('p_comments_everywhere', 'void /*a*/ f(/*b*/) // c\n{ /* d */ } /* e */'),
    ('p_unicode_in_strings', 'void f(){var s="h\xc3\xa9llo \xe2\x82\xac";}'),
    ('p_expression_statement_call', 'void f(){g();g(1)(2);}'),
    ('p_empty_statements', 'void f(){;;;}'),
    ('p_missing_semicolon', 'void f(){int x=1}'),
    ('p_missing_paren', 'void f(){if(x{}}'),
    ('p_missing_brace', 'void f(){'),
    ('p_extra_brace', 'void f(){}}'),
    ('p_unexpected_eof_type', 'int'),
    ('p_unexpected_eof_expr', 'void f(){int x='),
    ('p_unexpected_eof_args', 'void f(){g(1,'),
    ('p_unexpected_eof_index', 'void f(){a[1'),
    ('p_expected_expression', 'void f(){x=;}'),
    ('p_expected_expression_op', 'void f(){x=*;}'),
    ('p_expected_expression_close', 'void f(){g(,);}'),
    ('p_expected_identifier', 'void f(int 3){}'),
    ('p_expected_identifier_member', 'void f(){a.;}'),
    ('p_expected_identifier_type', 'void f(){int ;}'),
    ('p_top_level_expression', 'x=1;'),
    ('p_top_level_junk', '} void f(){}'),
    ('p_top_level_number', '42'),
    ('p_return_missing_semi', 'int f(){return 1}'),
    ('p_call_trailing_comma', 'void f(){g(1,);}'),
    ('p_generic_unclosed', 'Array<int a;'),
    ('p_generic_empty', 'Array<> a;'),
    ('p_generic_call_window', 'void f(){a<' + 'b,' * 70 + 'c>(1);}'),
    ('p_generic_window_60', 'void f(){a<' + 'b,' * 60 + '>(1);}'),
    ('p_generic_window_61', 'void f(){a<' + 'b,' * 61 + '>(1);}'),
    ('p_generic_window_62', 'void f(){a<' + 'b,' * 62 + '>(1);}'),
    ('p_generic_window_63', 'void f(){a<' + 'b,' * 63 + '>(1);}'),
    ('p_generic_window_64', 'void f(){a<' + 'b,' * 64 + '>(1);}'),
    ('p_generic_window_65', 'void f(){a<' + 'b,' * 65 + '>(1);}'),
    ('p_generic_window_66', 'void f(){a<' + 'b,' * 66 + '>(1);}'),
    ('p_generic_window_67', 'void f(){a<' + 'b,' * 67 + '>(1);}'),
    ('p_generic_window_odd_61', 'void f(){a<' + 'b,' * 61 + 'b>(1);}'),
    ('p_generic_window_odd_62', 'void f(){a<' + 'b,' * 62 + 'b>(1);}'),
    ('p_generic_window_odd_63', 'void f(){a<' + 'b,' * 63 + 'b>(1);}'),
    ('p_generic_window_odd_64', 'void f(){a<' + 'b,' * 64 + 'b>(1);}'),
    ('p_generic_window_odd_65', 'void f(){a<' + 'b,' * 65 + 'b>(1);}'),
    ('p_generic_window_dot_62', 'void f(){a<' + 'b,' * 62 + '>.x;}'),
    ('p_generic_window_dot_63', 'void f(){a<' + 'b,' * 63 + '>.x;}'),
    ('p_generic_window_dot_64', 'void f(){a<' + 'b,' * 64 + '>.x;}'),
    ('p_generic_call_window_far', 'void f(){a<' + 'b,' * 200 + 'c>(1);}'),
    ('p_depth_parens_ok', 'int f(){return %s;}' % nest('(', ')', 250)),
    ('p_depth_parens_edge_a', 'int f(){return %s;}' % nest('(', ')', 253)),
    ('p_depth_parens_edge_b', 'int f(){return %s;}' % nest('(', ')', 254)),
    ('p_depth_parens_edge_c', 'int f(){return %s;}' % nest('(', ')', 255)),
    ('p_depth_parens_edge_d', 'int f(){return %s;}' % nest('(', ')', 256)),
    ('p_depth_parens_over', 'int f(){return %s;}' % nest('(', ')', 300)),
    ('p_depth_blocks_ok', 'void f()%s' % nest('{', '}', 250, '')),
    ('p_depth_blocks_edge', 'void f()%s' % nest('{', '}', 255, '')),
    ('p_depth_blocks_over', 'void f()%s' % nest('{', '}', 300, '')),
    ('p_depth_types_ok', 'void f(){%sx;}' % ('Array<' * 100 + 'int' + '>' * 100 + ' ')),
    ('p_depth_types_over', 'void f(){%sx;}' % ('Array<' * 300 + 'int' + '>' * 300 + ' ')),
    ('p_depth_pointer_stars', 'int' + '*' * 400 + ' p;'),
    ('p_depth_assignments', 'void f(){' + 'a=' * 100 + '1;}'),
    ('p_depth_assignments_over', 'void f(){' + 'a=' * 300 + '1;}'),
    ('p_depth_unary', 'int f(){return ' + '-' * 250 + '1;}'),
    ('p_depth_unary_over', 'int f(){return ' + '-' * 300 + '1;}'),
    ('p_depth_ifs', 'void f(){' + 'if(a)' * 100 + 'g();}'),
    ('p_depth_ifs_over', 'void f(){' + 'if(a)' * 300 + 'g();}'),
    ('p_depth_lambda', 'void f(){var a=' + 'x=>' * 100 + '1;}'),
    ('p_depth_lambda_over', 'void f(){var a=' + 'x=>' * 300 + '1;}'),
    ('p_long_expression_chain', 'int f(){return ' + '+'.join('1' * 1 for _ in range(2000)) + ';}'),
    ('p_many_statements', 'void f(){' + 'g();' * 3000 + '}'),
    ('p_many_arguments', 'void f(){g(' + ','.join(str(i) for i in range(1500)) + ');}'),
    ('p_many_members', 'class A{' + ''.join('int m%d;' % i for i in range(800)) + '}'),
    ('p_many_enum_members', 'enum E{' + ','.join('M%d' % i for i in range(800)) + '}'),
]
BINARY_OPS = ['=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '||', '&&', '|', '^', '&', '==', '!=', '<', '>',
              '<=', '>=', '<<', '>>', '+', '-', '*', '/', '%']
PARSER_CASES.append(('p_precedence_matrix', 'void f(){%s}' % ''.join(
    'x=a %s b %s c;' % (first, second) for first in BINARY_OPS for second in BINARY_OPS)))
PARSER_CASES.append(('p_precedence_matrix_prefix', 'void f(){%s}' % ''.join(
    'x=-a %s !b %s ~c;' % (first, second) for first in BINARY_OPS[9:] for second in BINARY_OPS[9:])))
for name, text in PARSER_CASES:
    source = folder / (name + '.tc')
    source.write_bytes(text.encode('utf-8'))
    for flag in PARSE_PHASES:
        compare('parser case %s %s' % (name, flag), source.relative_to(ROOT).as_posix(), flag)

# Every prefix of a program that uses most of the grammar: each cut is a different syntax error
# or a shorter valid program, so this walks the "unexpected end of file" paths systematically.
RICH = """module demo; import std.string;
enum Kind { A = 1, B, C = 10 }
extern C { int puts(const char* text); }
class Box<T> { T value; Array<Box<int>> kids; static int made = 0; Box(T v) { value = v; } ~Box() { } T get() { return value; }
  property int size { get => 1 + 2; } }
@test async int run(const Kind k, int n = 3, string s = "x") {
  var t = (1, 2); var f = (int a) => a + n; int[3] arr = {1, 2, 3}; Box<int> b(1);
  for (i, v in arr) { if (v > 1 && i < 2) continue; else break; }
  for (int j = 0; j < n; j++) { defer puts(s.data); }
  switch (k) { case Kind.A, Kind.B: return -1; default: { g<int>(cast<int>(arr[1:2])); } }
  while (n-- > 0) { n = n * 2 + (arr[0] << 1); } delete new int(1); return await spawn h(n), 2;
}
"""
rich = RICH.encode()
cuts = sorted(set(m.end() for m in re.finditer(rb'\s+|\w+|.', rich, re.S)))
for cut in cuts:
    source = folder / ('prefix%d.tc' % cut)
    source.write_bytes(rich[:cut])
    for flag in PARSE_PHASES:
        compare('rich prefix %d %s' % (cut, flag), source.relative_to(ROOT).as_posix(), flag)

# Mutations of real sources: delete, insert, swap or duplicate tokens, or truncate. They produce
# plausible-looking programs that break in every corner of the grammar.
POOL = [b';', b'{', b'}', b'(', b')', b'<', b'>', b'>>', b'*', b'=', b'=>', b',', b'.', b':', b'[', b']', b'->', b'++',
        b'@', b'const', b'var', b'int', b'class', b'enum', b'switch', b'case', b'default', b'func', b'extern', b'new',
        b'Foo', b'1', b'0x', b'"s"', b"'c'", b'~', b'property', b'import', b'static', b'async']
mutation_rng = random.Random(0x50617273)
bases = [p for p in corpus if p.stat().st_size < 40000 and 'experimental' not in p.parts]
for index in range(opts.mutations):
    base = mutation_rng.choice(bases)
    pieces = re.findall(rb'\s+|\w+|.', base.read_bytes(), re.S)
    for _ in range(mutation_rng.randint(1, 3)):
        if not pieces:
            break
        op = mutation_rng.choice(['delete', 'insert', 'swap', 'duplicate', 'truncate', 'replace'])
        i = mutation_rng.randrange(len(pieces))
        if op == 'delete':
            del pieces[i:i + mutation_rng.randint(1, 4)]
        elif op == 'insert':
            pieces.insert(i, mutation_rng.choice(POOL))
        elif op == 'swap':
            j = mutation_rng.randrange(len(pieces))
            pieces[i], pieces[j] = pieces[j], pieces[i]
        elif op == 'duplicate':
            pieces[i:i] = pieces[i:i + mutation_rng.randint(1, 5)]
        elif op == 'truncate':
            del pieces[i:]
        else:
            pieces[i] = mutation_rng.choice(POOL)
    source = folder / ('mutation%d.tc' % index)
    source.write_bytes(b''.join(pieces))
    for flag in PARSE_PHASES:
        compare('mutation %d of %s %s' % (index, base.relative_to(ROOT).as_posix(), flag), source.relative_to(ROOT).as_posix(), flag)

# ---- 4. the CLI contract that both stages share ---------------------------------------------
code, out, err = run([stage1])
assert code == 2 and b'usage' in err, (code, err)
missing = folder / 'does-not-exist.tc'
for flag in PHASES:
    a = run([tiny, flag, missing])
    b = run([stage1, flag, missing])
    assert a[0] == b[0] == 1 and b"cannot read" in a[2] and a[2] == b[2], (flag, a, b)

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
print('Self-hosting verified: stage 1 lexer and parser match stage 0 on %d repository files, %d lexer edge cases, '
      '%d parser cases, %d program prefixes, %d lexer fuzz inputs and %d mutations; typed AST checked on %d programs'
      % (len(corpus), len(CASES), len(PARSER_CASES), len(cuts), opts.fuzz, opts.mutations, typed))
