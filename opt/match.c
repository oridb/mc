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

static Dtree *addwild(Dtree *t, Node *var, Node ***cap, size_t *ncap)
{
    Node *dcl;

    dcl = decls[var->expr.did];
    /* FIXME: Avoid duplicate constants */
    if (dcl->decl.isconst)
        return NULL;
    if (t->any)
        return t->any;
    t->any = mkdtree();
    lappend(cap, ncap, var);
    return t->any;
}

static Dtree *addunion(Dtree *t, Node *pat, Node ***cap, size_t *ncap)
{
    Dtree *sub;
    size_t i;

    /* if we have the value already... */
    sub = NULL;
    for (i = 0; i < t->nval; i++) {
        if (nameeq(t->val[i], pat->expr.args[0]))
            return addpat(t->sub[i], pat->expr.args[1], cap, ncap);
    }

    sub = mkdtree();
    lappend(&t->val, &t->nval, pat->expr.args[0]);
    lappend(&t->sub, &t->nsub, sub);
    if (pat->expr.nargs == 2)
        sub = addpat(sub, pat->expr.args[1], cap, ncap);
    return sub;
}

static Dtree *addlit(Dtree *t, Node *pat, Node ***cap, size_t *ncap)
{
    Dtree *sub;
    size_t i;

    for (i = 0; i < t->nval; i++) {
        if (liteq(t->val[i]->expr.args[0], pat->expr.args[0]))
            return addpat(t->sub[i], pat->expr.args[1], cap, ncap);
    }

    sub = mkdtree();
    lappend(&t->val, &t->nval, pat);
    lappend(&t->sub, &t->nsub, sub);
    return sub;
}

static Dtree *addtup(Dtree *t, Node *pat, Node ***cap, size_t *ncap)
{
    size_t i;

    for (i = 0; i < pat->expr.nargs; i++)
        t = addpat(t, pat->expr.args[i], cap, ncap);
    return t;
}

static Dtree *addarr(Dtree *t, Node *pat, Node ***cap, size_t *ncap)
{
    size_t i;

    for (i = 0; i < pat->expr.nargs; i++)
        t = addpat(t, pat->expr.args[i], cap, ncap);
    return t;
}

static Dtree *addpat(Dtree *t, Node *pat, Node ***cap, size_t *ncap)
{
    Dtree *ret;

    if (pat == NULL)
        return t;
    switch (exprop(pat)) {
        case Ovar:
            ret = addwild(t, pat, cap, ncap);
            break;
        case Oucon:
            ret = addunion(t, pat, cap, ncap);
            break;
        case Olit:
            ret = addlit(t, pat, cap, ncap);
            break;
        case Otup:
            ret = addtup(t, pat, cap, ncap);
            break;
        case Oarr:
            ret = addarr(t, pat, cap, ncap);
            break;
        default:
            /* Right now, we just use this code for warning.
             *
             * We shoudl fatal(unsupported match) here*/
            return NULL;
    }

    return ret;
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
