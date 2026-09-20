#include "tiny.h"
#include <errno.h>

static Token *peek(Context *c) {
    return &c->tokens[c->pos];
}
static int at(Context *c, const char *s) {
    return !strcmp(peek(c)->text, s);
}
static int eat(Context *c, const char *s) {
    if (at(c, s)) {
        c->pos++;
        return 1;
    }
    return 0;
}
static int keyword(Context *c, const char *s) {
    return peek(c)->kind == TK_ID && at(c, s);
}
static Token *take(Context *c) {
    Token *t = peek(c);
    if (t->kind == TK_EOF)
        tc_error(c, t->loc, "unexpected end of file");
    c->pos++;
    return t;
}
static Token *expect(Context *c, const char *s) {
    Token *t = peek(c);
    if (!eat(c, s))
        tc_error(c, t->loc, "expected '%s', found '%s'", s, t->text);
    return t;
}
static const char *ident(Context *c) {
    Token *t = take(c);
    if (t->kind != TK_ID)
        tc_error(c, t->loc, "expected identifier");
    return t->text;
}
static void append(Node **head, Node *n) {
    while (*head)
        head = &(*head)->next;
    *head = n;
}
static Type *parse_type(Context *c);
static Node *expr(Context *c, int min);
static Node *stmt(Context *c);
static Node *declaration(Context *c, int flags, Node *owner);
static void enter(Context *c) {
    if (++c->depth > 256)
        tc_error(c, peek(c)->loc, "syntax nesting limit (256) exceeded");
}
static void leave(Context *c) {
    c->depth--;
}
static int type_start(Context *c) {
    const char *s = peek(c)->text;
    Type *t;
    if (c->pos + 1 < c->ntokens && !strcmp(c->tokens[c->pos + 1].text, ".")) {
        size_t p = c->pos;
        while (p + 2 < c->ntokens && !strcmp(c->tokens[p + 1].text, ".") &&
               c->tokens[p + 2].kind == TK_ID)
            p += 2;
        if (p + 1 < c->ntokens &&
            (c->tokens[p + 1].kind == TK_ID || !strcmp(c->tokens[p + 1].text, "*")))
            return 1;
        return 0;
    }
    if (c->pos + 1 < c->ntokens && !strcmp(c->tokens[c->pos + 1].text, "("))
        return 0;
    if (!strcmp(s, "const") || !strcmp(s, "var") || !strcmp(s, "Slice") || !strcmp(s, "Array") ||
        !strcmp(s, "Channel") || !strcmp(s, "Task") || !strcmp(s, "Future") || !strcmp(s, "func") ||
        !strcmp(s, "closure"))
        return 1;
    for (t = c->types; t; t = t->next)
        if (t->name && !strcmp(t->name, s))
            return 1;
    if (strstr(" void bool i8 u8 i16 u16 i32 u32 i64 u64 float double char string int uint byte "
               "short long ulong size_t Error StringView ",
               tc_format(c, " %s ", s)))
        return 1;
    if (isupper((unsigned char)s[0]) && c->pos + 1 < c->ntokens &&
        (c->tokens[c->pos + 1].kind == TK_ID || !strcmp(c->tokens[c->pos + 1].text, "*") ||
         !strcmp(c->tokens[c->pos + 1].text, "[")))
        return 1;
    return 0;
}
static TypeLink *type_items(Context *c, const char *end) {
    TypeLink *list = NULL, **tail = &list;
    do {
        TypeLink *l = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
        l->type = parse_type(c);
        *tail = l;
        tail = &l->next;
    } while (eat(c, ","));
    if (!strcmp(end, ">") && at(c, ">>")) {
        c->tokens[c->pos].text = ">";
        return list;
    }
    expect(c, end);
    return list;
}
static Type *parse_type(Context *c) {
    Type *t;
    int cn = eat(c, "const");
    (void)cn;
    enter(c);
    if (eat(c, "("))
        t = type_tuple(c, type_items(c, ")"));
    else {
        const char *name = ident(c);
        while (eat(c, "."))
            name = tc_format(c, "%s.%s", name, ident(c));
        if (eat(c, "<")) {
            if (!strcmp(name, "func") || !strcmp(name, "closure")) {
                Type *ret = parse_type(c);
                TypeLink *params = NULL;
                expect(c, "(");
                if (!eat(c, ")"))
                    params = type_items(c, ")");
                expect(c, ">");
                t = type_generic(c, "Function", params);
                t->kind = !strcmp(name, "closure") ? TY_CLOSURE : TY_FUNC;
                t->base = ret;
            } else {
                TypeLink *items = type_items(c, ">");
                if (!strcmp(name, "Slice")) {
                    if (items->next)
                        tc_error(c, peek(c)->loc, "Slice takes one type");
                    t = type_derive(c, TY_SLICE, items->type, 0);
                } else
                    t = type_generic(c, name, items);
            }
        } else
            t = type_named(c, name);
    }
    if (cn)
        t = type_const(c, t);
    while (1) {
        if (eat(c, "*"))
            t = type_derive(c, TY_PTR, t, 0);
        else if (eat(c, "[")) {
            Token *n = take(c);
            unsigned long long len;
            if (n->kind != TK_INT)
                tc_error(c, n->loc, "array length must be an integer literal");
            errno = 0;
            len = strtoull(n->text, NULL, 0);
            if (errno || len == 0 || len > 10000000)
                tc_error(c, n->loc, "array length must be between 1 and 10000000");
            expect(c, "]");
            t = type_derive(c, TY_ARRAY, t, (size_t)len);
        } else
            break;
    }
    leave(c);
    return t;
}
static Node *arguments(Context *c, const char *end) {
    Node *list = NULL;
    if (eat(c, end))
        return NULL;
    do {
        const char *label = NULL;
        Node *n;
        if (peek(c)->kind == TK_ID && c->pos + 1 < c->ntokens &&
            !strcmp(c->tokens[c->pos + 1].text, ":")) {
            label = ident(c);
            expect(c, ":");
        }
        n = expr(c, 1);
        n->label = label;
        append(&list, n);
    } while (eat(c, ","));
    expect(c, end);
    return list;
}
static Node *parameters(Context *c) {
    Node *list = NULL;
    if (eat(c, ")"))
        return NULL;
    if (at(c, "void") && !strcmp(c->tokens[c->pos + 1].text, ")")) {
        c->pos += 2;
        return NULL;
    }
    do {
        Node *p = node(c, N_VAR, peek(c)->loc);
        if (eat(c, "...")) {
            p->name = "...";
            append(&list, p);
            break;
        }
        p->decl_type = parse_type(c);
        p->name = ident(c);
        if (eat(c, "="))
            p->a = expr(c, 1);
        append(&list, p);
    } while (eat(c, ","));
    expect(c, ")");
    return list;
}
static int lambda_ahead(Context *c) {
    size_t p = c->pos;
    int depth = 0;
    if (!at(c, "("))
        return 0;
    do {
        const char *s = c->tokens[p++].text;
        if (!strcmp(s, "("))
            depth++;
        else if (!strcmp(s, ")"))
            depth--;
    } while (p < c->ntokens && depth > 0);
    return depth == 0 && p < c->ntokens && !strcmp(c->tokens[p].text, "=>");
}
static int generic_call_ahead(Context *c) {
    size_t p = c->pos;
    int depth = 0;
    if (!at(c, "<"))
        return 0;
    while (p < c->ntokens && p < c->pos + 128) {
        const char *s = c->tokens[p++].text;
        if (!strcmp(s, "<"))
            depth++;
        else if (!strcmp(s, ">"))
            depth--;
        else if (!strcmp(s, ">>"))
            depth -= 2;
        else if (!strcmp(s, ";") || !strcmp(s, "{") || !strcmp(s, "="))
            return 0;
        if (depth <= 0)
            return p < c->ntokens &&
                   (!strcmp(c->tokens[p].text, "(") || !strcmp(c->tokens[p].text, "."));
    }
    return 0;
}
static Node *primary(Context *c) {
    Token *t = peek(c);
    Node *n;
    enter(c);
    if (lambda_ahead(c)) {
        n = node(c, N_LAMBDA, t->loc);
        expect(c, "(");
        if (!eat(c, ")")) {
            do {
                Node *p = node(c, N_VAR, peek(c)->loc);
                if (type_start(c)) {
                    p->decl_type = parse_type(c);
                    p->name = ident(c);
                } else {
                    p->name = ident(c);
                    p->decl_type = type_primitive(c, TY_AUTO);
                }
                append(&n->params, p);
            } while (eat(c, ","));
            expect(c, ")");
        }
        expect(c, "=>");
        if (at(c, "{"))
            n->body = stmt(c);
        else
            n->a = expr(c, 1);
    } else if (eat(c, "(")) {
        n = expr(c, 1);
        if (eat(c, ",")) {
            Node *tuple = node(c, N_TUPLE, t->loc);
            tuple->args = n;
            do {
                append(&tuple->args, expr(c, 1));
            } while (eat(c, ","));
            n = tuple;
        }
        expect(c, ")");
    } else if (eat(c, "{")) {
        n = node(c, N_INIT, t->loc);
        n->args = arguments(c, "}");
    } else if (eat(c, "new")) {
        n = node(c, N_NEW, t->loc);
        n->decl_type = parse_type(c);
        if (eat(c, "("))
            n->args = arguments(c, ")");
    } else if (eat(c, "sizeof")) {
        n = node(c, N_SIZEOF, t->loc);
        expect(c, "(");
        n->decl_type = parse_type(c);
        expect(c, ")");
    } else if (eat(c, "cast")) {
        n = node(c, N_CAST, t->loc);
        expect(c, "<");
        n->decl_type = parse_type(c);
        expect(c, ">");
        expect(c, "(");
        n->a = expr(c, 1);
        expect(c, ")");
    } else if (at(c, "!") || at(c, "-") || at(c, "+") || at(c, "~") || at(c, "*") || at(c, "&") ||
               at(c, "++") || at(c, "--")) {
        n = node(c, N_UNARY, t->loc);
        n->text = take(c)->text;
        n->a = expr(c, 14);
    } else if (eat(c, "await")) {
        n = node(c, N_AWAIT, t->loc);
        n->a = expr(c, 14);
    } else if (eat(c, "spawn")) {
        n = node(c, N_SPAWN, t->loc);
        n->a = expr(c, 14);
    } else {
        take(c);
        if (t->kind == TK_INT)
            n = node(c, N_INT, t->loc);
        else if (t->kind == TK_FLOAT)
            n = node(c, N_FLOAT, t->loc);
        else if (t->kind == TK_STRING)
            n = node(c, N_STRING, t->loc);
        else if (t->kind == TK_CHAR)
            n = node(c, N_CHAR, t->loc);
        else if (!strcmp(t->text, "true") || !strcmp(t->text, "false"))
            n = node(c, N_BOOL, t->loc);
        else if (!strcmp(t->text, "null"))
            n = node(c, N_NULL, t->loc);
        else if (t->kind == TK_ID) {
            n = node(c, N_ID, t->loc);
            n->name = t->text;
            if (eat(c, "=>")) {
                Node *p = node(c, N_VAR, t->loc);
                p->name = n->name;
                p->decl_type = type_primitive(c, TY_AUTO);
                n->kind = N_LAMBDA;
                n->params = p;
                n->name = NULL;
                if (at(c, "{"))
                    n->body = stmt(c);
                else
                    n->a = expr(c, 1);
            } else if (generic_call_ahead(c)) {
                expect(c, "<");
                n->decl_type = type_generic(c, t->text, type_items(c, ">"));
            }
        } else {
            tc_error(c, t->loc, "expected expression, found '%s'", t->text);
            n = NULL;
        }
        n->text = t->text;
    }
    while (1) {
        if (eat(c, "(")) {
            Node *call = node(c, N_CALL, t->loc);
            call->a = n;
            call->args = arguments(c, ")");
            n = call;
        } else if (eat(c, ".") || eat(c, "->")) {
            Node *m = node(c, N_MEMBER, peek(c)->loc);
            m->a = n;
            m->name = ident(c);
            n = m;
        } else if (eat(c, "[")) {
            Node *idx = node(c, N_INDEX, peek(c)->loc);
            idx->a = n;
            if (!at(c, ":"))
                idx->b = expr(c, 1);
            if (eat(c, ":")) {
                idx->kind = N_SLICE;
                if (!at(c, "]"))
                    idx->c = expr(c, 1);
            }
            expect(c, "]");
            n = idx;
        } else if (at(c, "++") || at(c, "--")) {
            Node *u = node(c, N_UNARY, peek(c)->loc);
            u->a = n;
            u->text = take(c)->text;
            u->flags |= NF_POST;
            n = u;
        } else
            break;
    }
    leave(c);
    return n;
}
static int precedence(const char *s) {
    if (!strcmp(s, "=") || !strcmp(s, "+=") || !strcmp(s, "-=") || !strcmp(s, "*=") ||
        !strcmp(s, "/=") || !strcmp(s, "%=") || !strcmp(s, "&=") || !strcmp(s, "|=") ||
        !strcmp(s, "^="))
        return 1;
    if (!strcmp(s, "||"))
        return 2;
    if (!strcmp(s, "&&"))
        return 3;
    if (!strcmp(s, "|"))
        return 4;
    if (!strcmp(s, "^"))
        return 5;
    if (!strcmp(s, "&"))
        return 6;
    if (!strcmp(s, "==") || !strcmp(s, "!="))
        return 7;
    if (!strcmp(s, "<") || !strcmp(s, ">") || !strcmp(s, "<=") || !strcmp(s, ">="))
        return 8;
    if (!strcmp(s, "<<") || !strcmp(s, ">>"))
        return 9;
    if (!strcmp(s, "+") || !strcmp(s, "-"))
        return 10;
    if (!strcmp(s, "*") || !strcmp(s, "/") || !strcmp(s, "%"))
        return 11;
    return 0;
}
static Node *expr(Context *c, int min) {
    Node *left;
    int p;
    enter(c);
    left = primary(c);
    while ((p = precedence(peek(c)->text)) >= min) {
        Token *op = take(c);
        Node *n = node(c, N_BINARY, op->loc);
        n->text = op->text;
        n->a = left;
        n->b = expr(c, p + (p == 1 ? 0 : 1));
        left = n;
    }
    leave(c);
    return left;
}
static Node *variable(Context *c, int semi) {
    Node *n = node(c, N_VAR, peek(c)->loc);
    n->decl_type = parse_type(c);
    n->name = ident(c);
    if (n->decl_type->kind == TY_AUTO)
        while (eat(c, ",")) {
            Node *p = node(c, N_VAR, peek(c)->loc);
            p->name = ident(c);
            append(&n->params, p);
        }
    if (eat(c, "[")) {
        Token *t = take(c);
        size_t count = (size_t)strtoull(t->text, NULL, 0);
        if (t->kind != TK_INT || count == 0 || count > 10000000)
            tc_error(c, t->loc, "invalid array length");
        expect(c, "]");
        n->decl_type = type_derive(c, TY_ARRAY, n->decl_type, count);
    }
    if (eat(c, "="))
        n->a = expr(c, 1);
    else if (eat(c, "(")) {
        n->a = node(c, N_NEW, n->loc);
        n->a->decl_type = n->decl_type;
        n->a->args = arguments(c, ")");
        n->a->flags |= NF_CONSTRUCTOR;
    }
    if (semi)
        expect(c, ";");
    return n;
}
static Node *stmt(Context *c) {
    Token *t = peek(c);
    Node *n;
    enter(c);
    if (eat(c, "{")) {
        n = node(c, N_BLOCK, t->loc);
        while (!eat(c, "}")) {
            if (peek(c)->kind == TK_EOF)
                tc_error(c, t->loc, "unterminated block");
            append(&n->body, stmt(c));
        }
    } else if (eat(c, "return")) {
        n = node(c, N_RETURN, t->loc);
        if (!at(c, ";")) {
            n->a = expr(c, 1);
            if (eat(c, ",")) {
                Node *tuple = node(c, N_TUPLE, t->loc);
                tuple->args = n->a;
                do {
                    append(&tuple->args, expr(c, 1));
                } while (eat(c, ","));
                n->a = tuple;
            }
        }
        expect(c, ";");
    } else if (eat(c, "if")) {
        n = node(c, N_IF, t->loc);
        expect(c, "(");
        n->a = expr(c, 1);
        expect(c, ")");
        n->body = stmt(c);
        if (eat(c, "else"))
            n->b = stmt(c);
    } else if (keyword(c, "switch") && c->pos + 1 < c->ntokens &&
               !strcmp(c->tokens[c->pos + 1].text, "(")) {
        Node *item;
        int has_default = 0;
        take(c);
        n = node(c, N_SWITCH, t->loc);
        expect(c, "(");
        n->a = expr(c, 1);
        expect(c, ")");
        expect(c, "{");
        while (!eat(c, "}")) {
            Token *label = peek(c);
            if (label->kind == TK_EOF)
                tc_error(c, t->loc, "unterminated switch");
            item = node(c, N_CASE, label->loc);
            if (keyword(c, "case")) {
                take(c);
                do {
                    append(&item->args, expr(c, 1));
                } while (eat(c, ","));
            } else if (keyword(c, "default")) {
                take(c);
                if (has_default)
                    tc_error(c, label->loc, "switch has more than one default");
                has_default = 1;
                item->text = "default";
            } else
                tc_error(c, label->loc, "expected 'case' or 'default' in switch");
            expect(c, ":");
            item->body = node(c, N_BLOCK, label->loc);
            while (!keyword(c, "case") && !keyword(c, "default") &&
                   !(peek(c)->kind == TK_OP && at(c, "}"))) {
                if (peek(c)->kind == TK_EOF)
                    tc_error(c, t->loc, "unterminated switch");
                append(&item->body->body, stmt(c));
            }
            append(&n->body, item);
        }
    } else if (eat(c, "while")) {
        n = node(c, N_WHILE, t->loc);
        expect(c, "(");
        n->a = expr(c, 1);
        expect(c, ")");
        n->body = stmt(c);
    } else if (eat(c, "for")) {
        n = node(c, N_FOR, t->loc);
        expect(c, "(");
        if (peek(c)->kind == TK_ID && (!strcmp(c->tokens[c->pos + 1].text, ",") ||
                                       !strcmp(c->tokens[c->pos + 1].text, "in"))) {
            n->kind = N_RANGE;
            n->name = ident(c);
            if (eat(c, ","))
                n->text = ident(c);
            expect(c, "in");
            n->a = expr(c, 1);
            expect(c, ")");
            n->body = stmt(c);
        } else {
            if (!at(c, ";")) {
                if (type_start(c))
                    n->a = variable(c, 0);
                else {
                    n->a = node(c, N_EXPR, t->loc);
                    n->a->a = expr(c, 1);
                }
            }
            expect(c, ";");
            if (!at(c, ";"))
                n->b = expr(c, 1);
            expect(c, ";");
            if (!at(c, ")"))
                n->c = expr(c, 1);
            expect(c, ")");
            n->body = stmt(c);
        }
    } else if (eat(c, "break")) {
        n = node(c, N_BREAK, t->loc);
        expect(c, ";");
    } else if (eat(c, "continue")) {
        n = node(c, N_CONTINUE, t->loc);
        expect(c, ";");
    } else if (eat(c, "defer")) {
        n = node(c, N_DEFER, t->loc);
        if (at(c, "{"))
            n->body = stmt(c);
        else {
            n->a = expr(c, 1);
            expect(c, ";");
        }
    } else if (eat(c, "delete")) {
        n = node(c, N_DELETE, t->loc);
        n->a = expr(c, 1);
        expect(c, ";");
    } else if (type_start(c))
        n = variable(c, 1);
    else {
        n = node(c, N_EXPR, t->loc);
        if (!at(c, ";"))
            n->a = expr(c, 1);
        expect(c, ";");
    }
    leave(c);
    return n;
}
static Node *declaration(Context *c, int flags, Node *owner) {
    Token *t = peek(c);
    Node *n;
    Type *ret;
    if (eat(c, "@")) {
        const char *a = ident(c);
        if (strcmp(a, "test"))
            tc_error(c, t->loc, "unknown annotation '%s'", a);
        return declaration(c, flags | NF_TEST, owner);
    }
    if (eat(c, "static"))
        return declaration(c, flags | NF_STATIC, owner);
    if (eat(c, "async"))
        return declaration(c, flags | NF_ASYNC, owner);
    if (eat(c, "stdcall"))
        return declaration(c, flags | NF_STDCALL, owner);
    if (eat(c, "cdecl"))
        return declaration(c, flags, owner);
    if (eat(c, "enum")) {
        long long next = 0;
        Node *value;
        n = node(c, N_ENUM, t->loc);
        n->name = ident(c);
        n->flags = flags;
        n->type = type_named(c, n->name);
        if (n->type->decl)
            tc_error(c, t->loc, "duplicate type '%s'", n->name);
        n->type->kind = TY_ENUM;
        n->type->decl = n;
        if (flags & NF_EXTERN)
            n->type->cname = n->name;
        expect(c, "{");
        while (!eat(c, "}")) {
            value = node(c, N_VAR, peek(c)->loc);
            value->name = ident(c);
            value->owner = n;
            value->flags = NF_CONST | NF_STATIC;
            value->decl_type = n->type;
            if (eat(c, "=")) {
                int sign = eat(c, "-") ? -1 : 1;
                Token *number = take(c);
                unsigned long long number_value;
                if (number->kind != TK_INT)
                    tc_error(c, number->loc, "enum value requires an integer literal");
                errno = 0;
                number_value = strtoull(number->text, NULL, 0);
                if (errno || number_value > (sign < 0 ? 2147483648ULL : 2147483647ULL))
                    tc_error(c, number->loc, "enum value outside i32 range");
                next = (long long)number_value * sign;
            }
            if (next > 2147483647LL)
                tc_error(c, value->loc, "enum value outside i32 range");
            value->text = tc_format(c, "%lld", next++);
            value->cname = flags & NF_EXTERN ? value->name
                                             : tc_format(c, "%s_%s", n->type->cname, value->name);
            append(&n->body, value);
            if (!eat(c, ",")) {
                expect(c, "}");
                break;
            }
        }
        eat(c, ";");
        return n;
    }
    if (eat(c, "module") || eat(c, "import")) {
        n = node(c, !strcmp(t->text, "module") ? N_MODULE : N_IMPORT, t->loc);
        n->name = ident(c);
        while (eat(c, ".")) {
            const char *part = ident(c);
            n->name = tc_format(c, "%s.%s", n->name, part);
        }
        expect(c, ";");
        return n;
    }
    if (eat(c, "class") || eat(c, "struct") || eat(c, "interface") || eat(c, "extension")) {
        n = node(c,
                 (!strcmp(t->text, "class") || !strcmp(t->text, "struct")) ? N_CLASS
                 : !strcmp(t->text, "interface")                           ? N_INTERFACE
                                                                           : N_EXTENSION,
                 t->loc);
        n->flags = flags;
        n->name = ident(c);
        n->type = type_named(c, n->name);
        if (flags & NF_EXTERN)
            n->type->cname = n->name;
        if (n->kind != N_EXTENSION) {
            if (n->type->decl)
                tc_error(c, t->loc, "duplicate type '%s'", n->name);
            n->type->decl = n;
        }
        if (eat(c, "<")) {
            do {
                Node *p = node(c, N_VAR, peek(c)->loc);
                p->name = ident(c);
                p->type = type_named(c, p->name);
                append(&n->params, p);
            } while (eat(c, ","));
            expect(c, ">");
        }
        expect(c, "{");
        while (!eat(c, "}"))
            append(&n->body, declaration(c, 0, n));
        eat(c, ";");
        return n;
    }
    if (eat(c, "property")) {
        n = node(c, N_PROPERTY, t->loc);
        n->decl_type = parse_type(c);
        n->name = ident(c);
        n->owner = owner;
        if (!eat(c, ";")) {
            expect(c, "{");
            expect(c, "get");
            expect(c, "=>");
            n->a = expr(c, 1);
            expect(c, ";");
            expect(c, "}");
        }
        return n;
    }
    if (owner && (at(c, owner->name) && !strcmp(c->tokens[c->pos + 1].text, "("))) {
        n = node(c, N_FUNCTION, t->loc);
        take(c);
        n->name = "init";
        n->decl_type = type_primitive(c, TY_VOID);
        n->flags = flags | NF_CONSTRUCTOR;
    } else if (owner && eat(c, "~")) {
        n = node(c, N_FUNCTION, t->loc);
        if (strcmp(ident(c), owner->name))
            tc_error(c, t->loc, "destructor name must match class");
        n->name = "destroy";
        n->decl_type = type_primitive(c, TY_VOID);
        n->flags = flags | NF_DESTRUCTOR;
    } else {
        ret = parse_type(c);
        n = node(c, N_FUNCTION, t->loc);
        n->decl_type = ret;
        n->name = ident(c);
        n->flags = flags;
        if (eat(c, "<")) {
            do {
                Node *p = node(c, N_VAR, peek(c)->loc);
                p->name = ident(c);
                p->type = type_named(c, p->name);
                append(&n->args, p);
            } while (eat(c, ","));
            expect(c, ">");
        }
        if (!at(c, "(")) {
            n->kind = N_VAR;
            if (eat(c, "="))
                n->a = expr(c, 1);
            expect(c, ";");
            n->owner = owner;
            return n;
        }
    }
    expect(c, "(");
    n->params = parameters(c);
    n->owner = owner;
    if (!eat(c, ";"))
        n->body = stmt(c);
    return n;
}
Node *parse(Context *c) {
    Node *p = node(c, N_PROGRAM, peek(c)->loc);
    c->program = p;
    while (peek(c)->kind != TK_EOF) {
        if (eat(c, "extern")) {
            Token *t = take(c);
            if (strcmp(t->text, "C"))
                tc_error(c, t->loc, "expected C after extern");
            expect(c, "{");
            while (!eat(c, "}"))
                append(&p->body, declaration(c, NF_EXTERN, NULL));
        } else
            append(&p->body, declaration(c, 0, NULL));
    }
    return p;
}
/* Shared by --emit-ast (c == NULL) and --emit-typed-ast: with a context, every node that the
   semantic pass typed is followed by " : <type>". */
static void dump_node(Context *c, Node *n, FILE *out, int indent) {
    static const char *names[] = {
        "Program", "FunctionDecl", "Class",  "Interface",  "Extension", "Property", "Block",
        "VarDecl", "ExprStmt",     "Return", "If",         "While",     "For",      "Range",
        "Break",   "Continue",     "Defer",  "Delete",     "Int",       "Float",    "String",
        "Char",    "Bool",         "Null",   "Identifier", "Binary",    "Unary",    "Call",
        "Index",   "Slice",        "Member", "Init",       "Tuple",     "New",      "Cast",
        "Sizeof",  "Lambda",       "Await",  "Spawn",      "Module",    "Import",   "Enum",
        "Switch",  "Case"};
    for (; n; n = n->next) {
        int i;
        for (i = 0; i < indent; i++)
            fputc(' ', out);
        fprintf(out, "%s%s%s%s%s @%d:%d", names[n->kind], n->name ? " " : "",
                n->name ? n->name : "", n->text ? " " : "", n->text ? n->text : "", n->loc.line,
                n->loc.col);
        if (c && n->type)
            fprintf(out, " : %s", type_name(c, n->type));
        fputc('\n', out);
        if (n->params)
            dump_node(c, n->params, out, indent + 2);
        if (n->args)
            dump_node(c, n->args, out, indent + 2);
        if (n->a)
            dump_node(c, n->a, out, indent + 2);
        if (n->b)
            dump_node(c, n->b, out, indent + 2);
        if (n->c)
            dump_node(c, n->c, out, indent + 2);
        if (n->body)
            dump_node(c, n->body, out, indent + 2);
    }
}
void dump_ast(Node *n, FILE *out, int indent) {
    dump_node(NULL, n, out, indent);
}
void dump_typed_ast(Context *c, Node *n, FILE *out, int indent) {
    dump_node(c, n, out, indent);
}
