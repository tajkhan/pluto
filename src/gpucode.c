#include "gpucode.h"

static void print_expr(struct clast_expr *e, FILE *dst);
static void print_stmt(struct localizer_info *loc, struct clast_stmt *s);

void print_indent(FILE *dst, int indent)
{
    fprintf(dst, "%*s", indent, "");
}

static void print_name(struct clast_name *n, FILE *dst)
{
    fprintf(dst, "%s", n->name);
}

static void print_term(struct clast_term *t, FILE *dst)
{
    cloog_int_print(dst, t->val);
    if (t->var) {
        fprintf(dst, "*");
        if (t->var->type == clast_expr_red)
            fprintf(dst, "(");
        print_expr(t->var, dst);
        if (t->var->type == clast_expr_red)
            fprintf(dst, ")");
    }
}

static void print_bin(struct clast_binary *b, FILE *dst)
{
    const char *s1, *s2;
    switch (b->type) {
        case clast_bin_mod:
            s1 = "MOD(", s2 = ", ";
            break;
        case clast_bin_div:
            s1 = "(", s2 = ")/(";
            break;
        case clast_bin_cdiv:
            s1 = "ceild(", s2 = ", ";
            break;
        case clast_bin_fdiv:
            s1 = "floord(", s2 = ", ";
            break;
        default:
            assert(0);
    }
    fprintf(dst, "%s", s1);
    print_expr(b->LHS, dst);
    fprintf(dst, "%s", s2);
    cloog_int_print(dst, b->RHS);
    fprintf(dst, ")");
}

static void print_red(struct clast_reduction *r, FILE *dst)
{
    int i;
    const char *s1, *s2, *s3;

    if (r->n == 1) {
        print_expr(r->elts[0], dst);
        return;
    }

    switch (r->type) {
    case clast_red_sum:
        s1 = "", s2 = " + ", s3 = "";
        break;
    case clast_red_max:
        s1 = "MAX(", s2 = ", ", s3 = ")";
        break;
    case clast_red_min:
        s1 = "MIN(", s2 = ", ", s3 = ")";
        break;
    default:
        assert(0);
    }

    for (i = 1; i < r->n; ++i)
        fprintf(dst, "%s", s1);
    print_expr(r->elts[0], dst);
    for (i = 1; i < r->n; ++i) {
        fprintf(dst, "%s", s2);
        print_expr(r->elts[i], dst);
        fprintf(dst, "%s", s3);
    }
}

static void print_expr(struct clast_expr *e, FILE *dst)
{
    switch (e->type) {
    case clast_expr_name:
        print_name((struct clast_name*) e, dst);
        break;
    case clast_expr_term:
        print_term((struct clast_term*) e, dst);
        break;
    case clast_expr_red:
        print_red((struct clast_reduction*) e, dst);
        break;
    case clast_expr_bin:
        print_bin((struct clast_binary*) e, dst);
        break;
    default:
        assert(0);
    }
}

static void print_ass(struct clast_assignment *a, FILE *dst, int indent)
{
    print_indent(dst, indent);
    fprintf(dst, "%s = ", a->LHS);
    print_expr(a->RHS, dst);
    fprintf(dst, ";\n");
}

static void print_guard(struct localizer_info *loc, struct clast_guard *g)
{
    int i;
    int n = g->n;

    print_indent(loc->dst, loc->indent);
    fprintf(loc->dst, "if (");
    for (i = 0; i < n; ++i) {
        if (i > 0)
            fprintf(loc->dst," && ");
        fprintf(loc->dst,"(");
        print_expr(g->eq[i].LHS, loc->dst);
        if (g->eq[i].sign == 0)
            fprintf(loc->dst," == ");
        else if (g->eq[i].sign > 0)
            fprintf(loc->dst," >= ");
        else
            fprintf(loc->dst," <= ");
        print_expr(g->eq[i].RHS, loc->dst);
        fprintf(loc->dst,")");
    }
    fprintf(loc->dst, ") {\n");
    loc->indent += 4;
    print_stmt(loc, g->then);
    loc->indent -= 4;
    print_indent(loc->dst, loc->indent);
    fprintf(loc->dst, "}\n");
}

static void print_for(struct localizer_info *loc, struct clast_for *f)
{
    assert(f->LB && f->UB);
    print_indent(loc->dst, loc->indent);
    fprintf(loc->dst, "for (%s = ", f->iterator);
    print_expr(f->LB, loc->dst);
    fprintf(loc->dst, "; %s <= ", f->iterator);
    print_expr(f->UB, loc->dst);
    fprintf(loc->dst, "; ++%s) {\n", f->iterator);
    loc->indent += 4;
    print_stmt(loc, f->body);
    loc->indent -= 4;
    print_indent(loc->dst, loc->indent);
    fprintf(loc->dst, "}\n");
}

static void print_stmt(struct localizer_info *loc, struct clast_stmt *s)
{
    for ( ; s; s = s->next) {
        if (CLAST_STMT_IS_A(s, stmt_root))
            continue;
        if (CLAST_STMT_IS_A(s, stmt_ass)) {
            print_ass((struct clast_assignment *) s, loc->dst, loc->indent);
        } else if (CLAST_STMT_IS_A(s, stmt_user)) {
            gpu_print_host_user(loc, (struct clast_user_stmt *) s);
            return;
        } else if (CLAST_STMT_IS_A(s, stmt_for)) {
            print_for(loc, (struct clast_for *) s);
        } else if (CLAST_STMT_IS_A(s, stmt_guard)) {
            print_guard(loc, (struct clast_guard *) s);
        } else {
            assert(0);
        }
    }
}

void gpu_print_host_stmt(struct localizer_info *loc, struct clast_stmt *s)
{
    print_stmt(loc, s);
}
