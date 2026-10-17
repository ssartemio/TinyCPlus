/* Deliberately local diagnostics: direct copies and straight-line destruction.
 * This is not a borrow checker. Branches, aliases and ownership of elements are
 * outside this pass; calls/returns retain the language's manual ownership ABI. */
#include "tiny.h"
typedef struct OwnerState { Symbol *symbol; int dead; struct OwnerState *next; } OwnerState;
static int owned_type(Type *type) {
    static const char *names[] = {"OwnedString", "StringBuilder", "Array", "Map", "Set", "Deque",
        "PriorityQueue", "BitSet", "OrderedMap", "File", "TcpSocket", "UdpSocket", "Task",
        "Future", "WorkerPool", "CancellationToken", "TaskGroup", "GuiWindow", "GapBuffer", NULL};
    int i;
    if (!type || type->kind != TY_NAMED) return 0;
    for (i = 0; names[i]; i++) if (!strcmp(type->name, names[i])) return 1;
    return 0;
}
static OwnerState *state(Context *c, OwnerState **states, Symbol *symbol) {
    OwnerState *s;
    for (s = *states; s; s = s->next) if (s->symbol == symbol) return s;
    s = (OwnerState *)tc_alloc(c, sizeof(*s)); s->symbol = symbol;
    s->next = *states; *states = s; return s;
}
static void expression(Context *c, Node *n, OwnerState **states) {
    for (; n; n = n->next) {
        if (n->kind == N_ID && n->sym && owned_type(n->type) && state(c, states, n->sym)->dead)
            tc_error(c, n->loc, "local ownership: use of '%s' after destroy/close", n->name);
        expression(c, n->a, states); expression(c, n->b, states); expression(c, n->c, states);
        expression(c, n->args, states);
        if (n->kind == N_CALL && n->a && n->a->kind == N_MEMBER && n->a->a &&
            n->a->a->kind == N_ID && n->a->a->sym && owned_type(n->a->a->type) &&
            (!strcmp(n->a->name, "destroy") || !strcmp(n->a->name, "close")))
            state(c, states, n->a->a->sym)->dead = 1;
        if (n->kind == N_BINARY && n->text && !strcmp(n->text, "=") && n->a && n->b &&
            n->a->kind == N_ID && owned_type(n->a->type) && n->b->kind == N_ID)
            tc_error(c, n->loc, "local ownership: assignment copies owner storage; use take(), clone() or a pointer borrow");
    }
}
static void block(Context *c, Node *n) {
    OwnerState *states = NULL;
    for (; n; n = n->next) {
        if (n->kind == N_VAR && owned_type(n->type) && n->a && n->a->kind == N_ID)
            tc_error(c, n->loc, "local ownership: initialization copies owner '%s'; use take(), clone() or a pointer borrow", n->a->name);
        if (n->kind == N_BLOCK) block(c, n->body);
        else if (n->kind == N_VAR || n->kind == N_EXPR || n->kind == N_RETURN)
            expression(c, n->a, &states);
        else if (n->kind != N_DEFER) {
            /* Do not propagate uncertain state through control flow. */
            block(c, n->body);
            if (n->a && n->a->kind == N_BLOCK) block(c, n->a->body);
            if (n->b && n->b->kind == N_BLOCK) block(c, n->b->body);
            if (n->c && n->c->kind == N_BLOCK) block(c, n->c->body);
            states = NULL;
        }
    }
}
void check_local_ownership(Context *c) {
    Node *n, *method;
    for (n = c->program->body; n; n = n->next) {
        if (n->kind == N_FUNCTION) block(c, n->body);
        if (n->kind == N_CLASS && !n->params)
            for (method = n->body; method; method = method->next)
                if (method->kind == N_FUNCTION) block(c, method->body);
    }
}
