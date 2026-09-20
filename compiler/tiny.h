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
typedef struct Buffer {
    char *data;
    size_t len, cap;
} Buffer;
typedef struct Loc {
    const char *file;
    int line, col;
} Loc;
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
struct Type {
    TypeKind kind;
    const char *name, *cname;
    Type *base;
    TypeLink *items;
    size_t count;
    int id, is_const, qualified, callconv;
    Node *decl;
    Type *alias;
    Type *next;
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
    N_ENUM
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
struct Node {
    NodeKind kind;
    Loc loc;
    const char *name, *text, *cname, *label;
    Type *type, *decl_type, *task_type;
    Node *a, *b, *c, *body, *params, *args, *next;
    Node *owner, *resolved, *parameter;
    Symbol *sym;
    int flags, id;
};
struct Symbol {
    const char *name, *cname;
    Type *type;
    Node *node;
    Node *function;
    Symbol *next;
};
struct Scope {
    Scope *parent;
    Symbol *symbols;
};
struct Context {
    Arena *arena;
    Token *tokens;
    size_t ntokens, tokcap, pos;
    Type *types, *builtin[TY_AUTO + 1];
    Node *program, *current_fn, *current_class;
    Node *loaded_modules, *adapters, *lambdas, *spawns;
    const char *active_file;
    Scope *global, *scope;
    int next_id, errors, loop_depth, depth, test_mode, bounds, uses_tasks, uses_io, uses_grpc, uses_gui,
        repl_mode, quiet;
    jmp_buf failure;
};

void *tc_alloc(Context *, size_t);
char *tc_str(Context *, const char *);
char *tc_strn(Context *, const char *, size_t);
char *tc_format(Context *, const char *, ...);
void tc_free(Context *);
void tc_error(Context *, Loc, const char *, ...);
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
int backend_option(int, const char *);
#endif
