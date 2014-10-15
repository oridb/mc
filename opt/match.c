#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "opt.h"

typedef struct Dtree Dtree;
struct Dtree {
    /* If the values are equal, go to 'sub'. If 'val' is null, anything matches. */
    Node **val;
    size_t nval;
    Dtree **sub;
    size_t nsub;
    Dtree *any;

    /* captured variables and action */
    Node **cap;
    size_t ncap;
    Node *act;

};

static Dtree *addpat(Dtree *t, Node *pat, Node ***cap, size_t *ncap);

static Dtree *mkdtree()
{
    return zalloc(sizeof(Dtree));
}

static int uconeq(Node *a, Node *b)
{
    return !strcmp(namestr(a), namestr(b));
}

static Dtree *addunion(Dtree *t, Node *pat, Node ***cap, size_t *ncap)
{
    Dtree *sub;
    size_t i;

    /* if we have the value already... */
    sub = NULL;
    for (i = 0; i < t->nval; i++) {
        if (!t->val[i])
            fatal(pat, "constructor already matched by earlier variable");
        if (uconeq(t->val[i], pat->expr.args[0])) {
            return addpat(t->sub[i], pat->expr.args[1], cap, ncap);
        }
    }

    sub = mkdtree();
    lappend(&t->val, &t->nval, pat->expr.args[0]);
    lappend(&t->sub, &t->nsub, sub);
    if (pat->expr.nargs == 2)
        sub = addpat(sub, pat->expr.args[1], cap, ncap);
    return sub;
}

static Dtree *addwild(Dtree *t, Node *var, Node ***cap, size_t *ncap)
{
    if (t->any)
        return t->any;
    t->any = mkdtree();
    lappend(cap, ncap, var);
    return t->any;
}

static Dtree *addpat(Dtree *t, Node *pat, Node ***cap, size_t *ncap)
{
    Type *ty;

    if (pat == NULL)
        return t;
    ty = tybase(exprtype(pat));
    if (exprop(pat) == Ovar)
        return addwild(t, pat, cap, ncap);

    switch (ty->type) {
        case Tyunion:
            t = addunion(t, pat, cap, ncap);
            break;
            /*
        case Tyslice:
            t = addslice(t, pat);
            break;
        case Tytuple:
            t = addtuple(t, pat);
            break;
        case Tyarray:
            t = addtuple(t, pat);
            break;
        case Tystruct:
            t = addstruct(t, pat);

        case Tybool: case Tychar: case Tybyte:
        case Tyint8: case Tyint16: case Tyint32: case Tyint:
        case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint:
        case Tyint64: case Tyuint64: case Tylong:  case Tyulong:
        case Tyflt32: case Tyflt64:
        case Typtr: case Tyfunc:
            t = addlit(t, pat);
            break;

            */
        default:
            /* Right now, we just use this code for warning. */
            /*
            fatal(pat, "unsupported match type %s", tystr(ty));
            */
            return NULL;
            break;
    }

    return t;
}

static void checkcomprehensive(Dtree *dt)
{
}

static Node *genmatch(Dtree *dt)
{
    return NULL;
}

Node *gensimpmatch(Node *m)
{
    Dtree *t, *leaf;
    Node **pat, **cap;
    size_t npat, ncap;
    size_t i;

    t = mkdtree();
    pat = m->matchstmt.matches;
    npat = m->matchstmt.nmatches;
    cap = NULL;
    ncap = 0;
    for (i = 0; i < npat; i++) {
        leaf = addpat(t, pat[i]->match.pat, &cap, &ncap);
        /* TODO: NULL is returned by unsupported patterns. */
        if (!leaf)
            return NULL;
        if (leaf->act)
            fatal(pat[i], "pattern matched by earlier case");
        leaf->act = pat[i]->match.block;
        leaf->cap = cap;
        leaf->ncap = ncap;
    }
    checkcomprehensive(t);
    return genmatch(t);
}
