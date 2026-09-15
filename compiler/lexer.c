#include "tiny.h"

static void push(Context *c, TokenKind k, const char *s, size_t n, Loc l) {
    Token *t;
    if (c->ntokens == c->tokcap) {
        size_t cap = c->tokcap ? c->tokcap * 2 : 256;
        if (cap > 2000000)
            tc_error(c, l, "source exceeds token limit");
        t = (Token *)realloc(c->tokens, cap * sizeof(Token));
        if (!t)
            tc_error(c, l, "out of memory");
        c->tokens = t;
        c->tokcap = cap;
    }
    t = &c->tokens[c->ntokens++];
    t->kind = k;
    t->text = tc_strn(c, s, n);
    t->loc = l;
}
void lex(Context *c, const char *file, const char *s) {
    size_t i = 0, start;
    int line = 1, col = 1;
    const char *ops[] = {">>=", "<<=", "...", "==", "!=", "<=", ">=", "&&", "||",
                         "++",  "--",  "+=",  "-=", "*=", "/=", "%=", "&=", "|=",
                         "^=",  "<<",  ">>",  "->", "=>", "::", NULL};
    if ((unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb && (unsigned char)s[2] == 0xbf)
        i = 3;
    while (s[i]) {
        unsigned char ch = (unsigned char)s[i];
        Loc l = {file, line, col};
        if (isspace(ch)) {
            if (ch == '\n') {
                line++;
                col = 1;
            } else
                col++;
            i++;
            continue;
        }
        if (s[i] == '/' && s[i + 1] == '/') {
            while (s[i] && s[i] != '\n') {
                i++;
                col++;
            }
            continue;
        }
        if (s[i] == '/' && s[i + 1] == '*') {
            int level = 1;
            i += 2;
            col += 2;
            while (s[i] && level) {
                if (s[i] == '/' && s[i + 1] == '*') {
                    level++;
                    i += 2;
                    col += 2;
                } else if (s[i] == '*' && s[i + 1] == '/') {
                    level--;
                    i += 2;
                    col += 2;
                } else {
                    if (s[i] == '\n') {
                        line++;
                        col = 1;
                    } else
                        col++;
                    i++;
                }
            }
            if (level)
                tc_error(c, l, "unterminated block comment");
            continue;
        }
        start = i;
        if (isalpha(ch) || ch == '_') {
            do {
                i++;
                col++;
            } while (isalnum((unsigned char)s[i]) || s[i] == '_');
            push(c, TK_ID, s + start, i - start, l);
            continue;
        }
        if (isdigit(ch)) {
            TokenKind k = TK_INT;
            if (s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                i += 2;
                col += 2;
                if (!isxdigit((unsigned char)s[i]))
                    tc_error(c, l, "hex literal needs digits");
                while (isxdigit((unsigned char)s[i])) {
                    i++;
                    col++;
                }
            } else {
                while (isdigit((unsigned char)s[i])) {
                    i++;
                    col++;
                }
                if (s[i] == '.' && s[i + 1] != '.') {
                    k = TK_FLOAT;
                    i++;
                    col++;
                    while (isdigit((unsigned char)s[i])) {
                        i++;
                        col++;
                    }
                }
                if (s[i] == 'e' || s[i] == 'E') {
                    k = TK_FLOAT;
                    i++;
                    col++;
                    if (s[i] == '+' || s[i] == '-') {
                        i++;
                        col++;
                    }
                    if (!isdigit((unsigned char)s[i]))
                        tc_error(c, l, "exponent needs digits");
                    while (isdigit((unsigned char)s[i])) {
                        i++;
                        col++;
                    }
                }
            }
            if (isalpha((unsigned char)s[i]) || s[i] == '_')
                tc_error(c, l, "invalid numeric literal suffix");
            push(c, k, s + start, i - start, l);
            continue;
        }
        if (ch == '"' || ch == '\'') {
            unsigned char quote = ch;
            int count = 0;
            i++;
            col++;
            while (s[i] && (unsigned char)s[i] != quote) {
                if (s[i] == '\n' || s[i] == '\r')
                    tc_error(c, l, "newline in literal");
                if (s[i] == '\\') {
                    i++;
                    col++;
                    if (!s[i])
                        break;
                    if (s[i] == 'x') {
                        if (!isxdigit((unsigned char)s[i + 1]) ||
                            !isxdigit((unsigned char)s[i + 2]))
                            tc_error(c, l, "hex escape requires exactly two digits");
                        i += 2;
                        col += 2;
                    } else if (!strchr("abfnrtv\\\"'0", s[i]))
                        tc_error(c, l, "unsupported escape '\\%c'", s[i]);
                }
                i++;
                col++;
                count++;
            }
            if (!s[i])
                tc_error(c, l, "unterminated literal");
            i++;
            col++;
            if (quote == '\'' && count != 1)
                tc_error(c, l, "character literal must contain one character");
            push(c, quote == '"' ? TK_STRING : TK_CHAR, s + start, i - start, l);
            continue;
        }
        {
            int j, found = 0;
            for (j = 0; ops[j]; j++) {
                size_t n = strlen(ops[j]);
                if (strncmp(s + i, ops[j], n) == 0) {
                    push(c, TK_OP, s + i, n, l);
                    i += n;
                    col += (int)n;
                    found = 1;
                    break;
                }
            }
            if (found)
                continue;
        }
        if (strchr("+-*/%=<>!&|^~(){}[];,.:?@", ch)) {
            push(c, TK_OP, s + i, 1, l);
            i++;
            col++;
            continue;
        }
        tc_error(c, l, "unexpected byte 0x%02x", (unsigned)ch);
    }
    {
        Loc l = {file, line, col};
        push(c, TK_EOF, "", 0, l);
    }
}
void dump_tokens(Context *c, FILE *out) {
    static const char *names[] = {"eof",    "identifier", "integer", "float",
                                  "string", "char",       "operator"};
    size_t i;
    for (i = 0; i < c->ntokens; i++)
        fprintf(out, "%d:%d\t%s\t%s\n", c->tokens[i].loc.line, c->tokens[i].loc.col,
                names[c->tokens[i].kind], c->tokens[i].text);
}
