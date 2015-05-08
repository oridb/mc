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

static void checkundef(Node *n, Reaching *r, Bitset *reach, Bitset *kill)
{
    size_t i, j, did;
    Node *def;

    if (n->type != Nexpr)
        return;
    if (exprop(n) == Ovar) {
        did = n->expr.did;
        for (j = 0; j < r->ndefs[did]; j++) {
            if (bshas(kill, r->defs[did][j]))
                continue;
            if (bshas(reach, r->defs[did][j]))
                def = nodes[r->defs[did][j]];
            if (exprop(def) == Oundef)
                fatal(n, "%s used before definition", namestr(n->expr.args[0]));
        }
    } else {
        for (i = 0; i < n->expr.nargs; i++)
            checkundef(n->expr.args[i], r, reach, kill);
    }
}

void bsdump(Bitset *bs)
{
    size_t i;
    for (i = 0; bsiter(bs, &i); i++)
        printf("%zd ", i);
    printf("\n");
}

static void checkreach(Cfg *cfg)
{
    Bitset *reach, *kill;
    size_t i, j, k;
    Reaching *r;
    Node *n, *m;
    Bb *bb;

    r = reaching(cfg);
    for (i = 0; i < cfg->nbb; i++) {
        bb = cfg->bb[i];
        reach = bsdup(r->in[i]);
        kill = mkbs();
        for (j = 0; j < bb->nnl; j++) {
            n = bb->nl[j];
            if (exprop(n) == Oundef) {
                bsput(reach, n->nid);
            } else {
                m = assignee(n);
                if (m)
                    for (k = 0; k < r->ndefs[m->expr.did]; k++)
                        bsput(kill, r->defs[m->expr.did][k]);
                checkundef(n, r, reach, kill);
            }
        }
        bsfree(reach);
        bsfree(kill);
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
    if(0)
        checkreach(cfg);
}
