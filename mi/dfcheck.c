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
#include "mi.h"

static void nodevars(Node *n, Bitset *bs)
{
    size_t i;

    if (!n || n->type != Nexpr)
        return;
    switch (exprop(n)) {
        case Ovar:
            bsput(bs, n->expr.did);
            break;
        default:
            nodevars(n->expr.idx, bs);
            for (i = 0; i < n->expr.nargs; i++)
                nodevars(n->expr.args[i], bs);
            break;
    }
}
void nodedef(Node *n, Bitset *bs)
{
    Node *p;
    size_t i;

    switch(exprop(n)) {
        case Oset:
        case Oasn: case Oaddeq:
        case Osubeq: case Omuleq:
        case Odiveq: case Omodeq:
        case Oboreq: case Obandeq:
        case Obxoreq: case Obsleq:
        case Obsreq:
            nodevars(n->expr.args[0], bs);
            nodedef(n->expr.args[1], bs);
            break;
            /* for the sake of less noise: assume that f(&var) inits the var. */
        case Ocall:
            for (i = 1; i < n->expr.nargs; i++) {
                p = n->expr.args[i];
                if (exprop(p) == Oaddr && exprop(p->expr.args[0]) == Ovar)
                    nodevars(p, bs);
            }
            break;
        default:
            break;
    }
}

static void checkdefined(Node *n, Bitset *def)
{
    size_t i;
    Node *d;

    if (!n || n->type != Nexpr)
        return;
    switch (exprop(n)) {
        case Ovar:
            d = decls[n->expr.did];
            if (!bshas(def, n->expr.did) && !d->decl.isglobl)
                fatal(n, "%s used before definition", namestr(n->expr.args[0]));
            break;
        default:
            nodevars(n->expr.idx, def);
            for (i = 0; i < n->expr.nargs; i++)
                checkdefined(n->expr.args[i], def);
            break;
    }
}

static void checkuse(Bb *bb, Bitset *def)
{
    size_t i;
    Node *n;

    if (!bb)
        return;
    for (i = 0; i < bb->nnl; i++) {
        n = bb->nl[i];
        /* Tradeoff.
         *
         * We could check after, and get slightly more accurate checking,
         * but then we error on things like:
         *      init(&foo);
         *
         * We can check before, but then we don't error on things like:
         *      x = f(x)
         *
         * Eventually we should check both ways. Right now, I want to get
         * something working.
         */
        nodedef(n, def);
        switch(exprop(n)) {
            case Oset:
            case Oasn:
                checkdefined(n->expr.args[1], def);
                break;
            default:
                checkdefined(n, def);
        }
    }
}

static void bbdef(Bb *bb, Bitset *bs)
{
    size_t i;

    if (!bb)
        return;
    for (i = 0; i < bb->nnl; i++)
        nodedef(bb->nl[i], bs);
}

static Bitset *indef(Cfg *cfg, Bb *bb, Bitset **outdef)
{
    size_t j;
    Bitset *def;

    j = 0;
    if (!bb || !bsiter(bb->pred, &j))
        return mkbs();
    def = bsdup(outdef[j]);
    for (; bsiter(bb->pred, &j); j++)
        bsintersect(def, outdef[j]);
    return def;
}

static void addargs(Cfg *cfg, Bitset *def)
{
    Node *n;
    size_t i;

    n = cfg->fn;
    assert(n->type == Ndecl);
    n = n->decl.init;
    assert(n->type == Nexpr);
    n = n->expr.args[0];
    assert(n->type == Nlit);
    n = n->lit.fnval;

    for (i = 0; i < n->func.nargs; i++)
        bsput(def,n->func.args[i]->decl.did); 
}

static void checkreach(Cfg *cfg)
{
    Bitset **outdef;
    Bitset *def;
    size_t i, j;
    int changed;
    Bb *bb;

    outdef = xalloc(sizeof(Bitset*) * cfg->nbb);

    def = mkbs();

    for (i = 0; i < cfg->nbb; i++) {
        outdef[i] = mkbs();
        bbdef(cfg->bb[i], outdef[i]);
    }
    addargs(cfg, outdef[cfg->start->id]);

    for (i = 0; i < cfg->nbb; i++)
        for (j= 0; bsiter(outdef[i], &j); j++)
            printf("bb %zd defines %s\n", i, declname(decls[j]));

    do {
        changed = 0;
        for (i = 0; i < cfg->nbb; i++) {
            bb = cfg->bb[i];

            def = indef(cfg, bb, outdef);
            bsunion(def, outdef[i]);

            if (!bseq(outdef[i], def)) {
                changed = 1;
                bsfree(outdef[i]);
                outdef[i] = def;
            }

        }
    } while (changed);



    printf("---\n");
    for (i = 0; i < cfg->nbb; i++) {
        for (j= 0; bsiter(outdef[i], &j); j++)
            printf("bb %zd defines %s\n", i, declname(decls[j]));
        def = indef(cfg, bb, outdef);
        checkuse(cfg->bb[i], def);
        bsfree(def);
    }
}

static void checkpredret(Cfg *cfg, Bb *bb)
{
    Bb *pred;
    Op op;
    size_t i;

    for (i = 0; bsiter(bb->pred, &i); i++) {
        pred = cfg->bb[i];
        if (pred->nnl == 0) {
            checkpredret(cfg, pred);
        } else {
            op = exprop(pred->nl[pred->nnl - 1]);
            if (op != Oret && op != Odead) {
                fatal(pred->nl[pred->nnl-1], "Reaches end of function without return\n");
            }
        }
    }
}

static void checkret(Cfg *cfg)
{
    Type *ft;

    ft = tybase(decltype(cfg->fn));
    assert(ft->type == Tyfunc);
    if (ft->sub[0]->type == Tyvoid)
        return;

    checkpredret(cfg, cfg->end);
}

void check(Cfg *cfg)
{
    checkret(cfg);
    if (0)
        checkreach(cfg);
}
