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

/*
static void nodeuse(Node *n, Bitset *bs)
{
}

static void nodedef(Node *n, Bitset *bs)
{
}

static void bbuse(Bb *bb, Bitset *bs)
{
}

static void bbdef(Bb *bb, Bitset *bs)
{
}
*/

static void checkreach(Cfg *cfg)
{
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
    checkreach(cfg);
}
