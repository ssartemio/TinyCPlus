/*
 * Contrato compartido del frontend: tokens, AST, tipos, ámbitos y Context.
 * Los punteros de AST y tipos pertenecen normalmente a la arena de la unidad.
 * Node reutiliza campos según NodeKind; consulte parser.c y semantic.c antes
 * de interpretar un enlace. Véase docs/CODE_GUIDE.md para el recorrido completo.
 */
#ifndef TINY_COMPILER_H
#define TINY_COMPILER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <ctype.h>

typedef struct Arena Arena;
typedef struct Type Type;
typedef struct Node Node;
typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct Context Context;
typedef struct SourceFile SourceFile;
typedef struct Instantiation Instantiation;
/* Buffer de texto con reserva propia: len excluye el terminador NUL. */
typedef struct Buffer {
    char *data;
    size_t len, cap;
} Buffer;
/* Ubicación original conservada en tokens, AST y diagnósticos. */
typedef struct Loc {
    const char *file;
    int line, col;
} Loc;
struct SourceFile {
    const char *file, *text;
    SourceFile *next;
};
struct Instantiation {
    const char *name;
    Loc loc;
    Instantiation *parent;
};
typedef enum TokenKind { TK_EOF, TK_ID, TK_INT, TK_FLOAT, TK_STRING, TK_CHAR, TK_OP } TokenKind;
typedef struct Token {
    TokenKind kind;
    const char *text;
    Loc loc;
} Token;
typedef enum TypeKind {
    TY_VOID,
    TY_BOOL,
    TY_I8,
    TY_U8,
    TY_I16,
    TY_U16,
    TY_I32,
    TY_U32,
    TY_I64,
    TY_U64,
    TY_FLOAT,
    TY_DOUBLE,
    TY_CHAR,
    TY_STRING,
    TY_NULL,
    TY_PTR,
    TY_ARRAY,
    TY_SLICE,
    TY_TUPLE,
    TY_FUNC,
    TY_CLOSURE,
    TY_NAMED,
    TY_ENUM,
    TY_AUTO
} TypeKind;
typedef struct TypeLink {
    Type *type;
    struct TypeLink *next;
} TypeLink;
/* Descriptor de tipo; base/items/count se interpretan según kind. */
struct Type {
    TypeKind kind;
    const char *name, *cname, *module_file;
    Type *base;
    TypeLink *items;
    size_t count;
    int id, is_const, qualified, callconv;
    Node *decl;
    Type *alias;
    Type *next;
    Loc origin;
    Instantiation *trace;
};
typedef enum NodeKind {
    N_PROGRAM,
    N_FUNCTION,
    N_CLASS,
    N_INTERFACE,
    N_EXTENSION,
    N_PROPERTY,
    N_BLOCK,
    N_VAR,
    N_EXPR,
    N_RETURN,
    N_IF,
    N_WHILE,
    N_FOR,
    N_RANGE,
    N_BREAK,
    N_CONTINUE,
    N_DEFER,
    N_DELETE,
    N_INT,
    N_FLOAT,
    N_STRING,
    N_CHAR,
    N_BOOL,
    N_NULL,
    N_ID,
    N_BINARY,
    N_UNARY,
    N_CALL,
    N_INDEX,
    N_SLICE,
    N_MEMBER,
    N_INIT,
    N_TUPLE,
    N_NEW,
    N_CAST,
    N_SIZEOF,
    N_LAMBDA,
    N_AWAIT,
    N_SPAWN,
    N_MODULE,
    N_IMPORT,
    N_ENUM,
    N_SWITCH,
    N_CASE
} NodeKind;
enum {
    NF_EXTERN = 1,
    NF_STATIC = 2,
    NF_ASYNC = 4,
    NF_TEST = 8,
    NF_CONST = 16,
    NF_POST = 32,
    NF_CONSTRUCTOR = 64,
    NF_DESTRUCTOR = 128,
    NF_LVALUE = 256,
    NF_CHECKED = 512,
    NF_RETURNS = 1024,
    NF_OWNED = 2048,
    NF_STDCALL = 4096
};
/* Nodo etiquetado por kind; next enlaza listas, no es un hijo genérico.
 * El análisis añade type/sym/resolved; el emisor consume esas anotaciones. */
struct Node {
    NodeKind kind;
    Loc loc;
    const char *name, *text, *cname, *label;
    Type *type, *decl_type, *task_type;
    Node *a, *b, *c, *body, *params, *args, *next;
    Node *owner, *resolved, *parameter;
    Symbol *sym;
    int flags, id;
    Instantiation *trace;
};
struct Symbol {
    const char *name, *cname;
    Type *type;
    Node *node;
    Node *function;
    Symbol *next;
};
/* Ámbito léxico: la búsqueda continúa en parent si no hay símbolo local. */
struct Scope {
    Scope *parent;
    Symbol *symbols;
};
/* Estado de una unidad. La arena retiene AST, símbolos y tipos hasta tc_free.
 * failure debe inicializarse con setjmp antes de cualquier tc_error. */
struct Context {
    int test_isolate, explain;
    Node *test_function;
    const char *test_filter;
    const char *type_file;
    int diagnostic_json;
    int uses_gui;
    Arena *arena;
    Token *tokens;
    size_t ntokens, tokcap, pos;
    Type *types, *builtin[TY_AUTO + 1];
    Node *program, *current_fn, *current_class;
    Node *loaded_modules, *adapters, *lambdas, *spawns;
    const char *active_file;
    SourceFile *sources;
    Instantiation *diagnostic_trace;
    Scope *global, *scope;
    int next_id, errors, loop_depth, switch_loop, depth, test_mode, bounds, uses_tasks, uses_io, uses_grpc,
        repl_mode, quiet;
    jmp_buf failure;
};

void *tc_alloc(Context *, size_t);
char *tc_str(Context *, const char *);
char *tc_strn(Context *, const char *, size_t);
char *tc_format(Context *, const char *, ...);
void tc_free(Context *);
void tc_error(Context *, Loc, const char *, ...);
void tc_source(Context *, const char *, const char *);
void buf_add(Buffer *, const char *);
void buf_printf(Buffer *, const char *, ...);
char *read_file(const char *);
int write_file(const char *, const char *);
void lex(Context *, const char *, const char *);
void dump_tokens(Context *, FILE *);
Node *node(Context *, NodeKind, Loc);
Node *parse(Context *);
void dump_ast(Node *, FILE *, int);
Type *type_primitive(Context *, TypeKind);
Type *type_named(Context *, const char *);
Type *type_derive(Context *, TypeKind, Type *, size_t);
Type *type_tuple(Context *, TypeLink *);
Type *type_generic(Context *, const char *, TypeLink *);
Type *type_const(Context *, Type *);
const char *type_name(Context *, Type *);
int type_equal(Type *, Type *);
int type_numeric(Type *);
int type_integer(Type *);
void analyze(Context *);
void expand_generics(Context *);
Node *load_program(Context *, const char *, const char *);
const char *module_name(Context *, const char *);
char *generate_c(Context *);
int backend(Context *, const char *, const char *, const char *, int, int, int, char **,
            const char *);
int tc_compile(const char *, const char *, int, int, const char *, int, char **, const char *);
int tc_format_file(const char *, const char *);
int tc_document_file(const char *, const char *);
int tc_repl(const char *);
void tc_repl_clear(void);
int backend_gui_backend(const char *);
int backend_option(int, const char *);
int module_path_option(const char *);
int tc_project(const char *, const char *, int, int, const char *, int, char **, const char *);

void tc_json_string(FILE *, const char *);
void dump_typed_ast(Node *, FILE *);

int backend_capabilities(const char *, const char *);
int test_selected(Context *, Node *);

void check_local_ownership(Context *);

int tc_json_mode(void);
FILE *tc_fopen(const char *, const char *);
int tc_remove(const char *);
#ifdef _WIN32
wchar_t *tc_utf16(const char *);
char *tc_utf8(const wchar_t *);
int tc_mkdir(const char *);
#endif
#endif
