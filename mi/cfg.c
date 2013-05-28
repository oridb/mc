#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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


static Bb *mkbb(Cfg *cfg)
{
    Bb *bb;

    bb = zalloc(sizeof(Bb));
    bb->id = cfg->nextbbid++;
    bb->pred = mkbs();
    bb->succ = mkbs();
    lappend(&cfg->bb, &cfg->nbb, bb);
    return bb;
}

static char *lblstr(Node *n)
{
    assert(exprop(n) == Olit);
    assert(n->expr.args[0]->type == Nlit);
    assert(n->expr.args[0]->lit.littype == Llbl);
    return n->expr.args[0]->lit.lblval;
}

static void label(Cfg *cfg, Node *lbl, Bb *bb)
{
    htput(cfg->lblmap, lblstr(lbl), bb);
    lappend(&bb->lbls, &bb->nlbls, lblstr(lbl));
}

static int addnode(Cfg *cfg, Bb *bb, Node *n)
{
    switch (exprop(n)) {
        case Ojmp:
        case Ocjmp:
            lappend(&bb->nl, &bb->nnl, n);
            lappend(&cfg->fixjmp, &cfg->nfixjmp, n);
            lappend(&cfg->fixblk, &cfg->nfixblk, bb);
            return 1;
            break;
        default:
            lappend(&bb->nl, &bb->nnl, n);
            break;
    }
    return 0;
}

static int islabel(Node *n)
{
    Node *l;
    if (n->type != Nexpr)
        return 0;
    if (exprop(n) != Olit)
        return 0;
    l = n->expr.args[0];
    if (l->type != Nlit)
        return 0;
    if (l->lit.littype != Llbl)
        return 0;
    return 1;
}

static Bb *addlabel(Cfg *cfg, Bb *bb, Node **nl, size_t i)
{
    /* if the current block assumes fall-through, insert an explicit jump */
    if (i > 0 && nl[i - 1]->type == Nexpr) {
        if (exprop(nl[i - 1]) != Ocjmp && exprop(nl[i - 1]) != Ojmp)
            addnode(cfg, bb, mkexpr(-1, Ojmp, mklbl(-1, lblstr(nl[i])), NULL));
    }
    if (bb->nnl)
        bb = mkbb(cfg);
    label(cfg, nl[i], bb);
    return bb;
}

Cfg *mkcfg(Node **nl, size_t nn)
{
    Cfg *cfg;
    Bb *pre, *post;
    Bb *bb, *targ;
    Node *a, *b;
    size_t i;

    cfg = zalloc(sizeof(Cfg));
    cfg->lblmap = mkht(strhash, streq);
    pre = mkbb(cfg);
    bb = mkbb(cfg);
    for (i = 0; i < nn; i++) {
        switch (nl[i]->type) {
            case Nexpr:
                if (islabel(nl[i]))
                    bb = addlabel(cfg, bb, nl, i);
                else if (addnode(cfg, bb, nl[i]))
                    bb = mkbb(cfg);
                break;
                break;
            case Ndecl:
                break;
            default:
                die("Invalid node type %s in mkcfg", nodestr(nl[i]->type));
        }
    }
    post = mkbb(cfg);
    bsput(pre->succ, cfg->bb[1]->id);
    bsput(cfg->bb[1]->pred, pre->id);
    bsput(cfg->bb[cfg->nbb - 2]->succ, post->id);
    bsput(post->pred, cfg->bb[cfg->nbb - 2]->id);
    for (i = 0; i < cfg->nfixjmp; i++) {
        bb = cfg->fixblk[i];
        switch (exprop(cfg->fixjmp[i])) {
            case Ojmp:
                a = cfg->fixjmp[i]->expr.args[0];
                b = NULL;
                break;
            case Ocjmp:
                a = cfg->fixjmp[i]->expr.args[1];
                b = cfg->fixjmp[i]->expr.args[2];
                break;
            default:
                die("Bad jump fix thingy");
                break;
        }
        if (a) {
            targ = htget(cfg->lblmap, lblstr(a));
            if (!targ)
                die("No bb with label \"%s\"", lblstr(a));
            bsput(bb->succ, targ->id);
            bsput(targ->pred, bb->id);
        }
        if (b) {
            targ = htget(cfg->lblmap, lblstr(b));
            if (!targ)
                die("No bb with label \"%s\"", lblstr(b));
            bsput(bb->succ, targ->id);
            bsput(targ->pred, bb->id);
        }
    }
    return cfg;
}

void dumpcfg(Cfg *cfg, FILE *fd)
{
    size_t i, j;
    Bb *bb;
    char *sep;

    for (j = 0; j < cfg->nbb; j++) {
        bb = cfg->bb[j];
        fprintf(fd, "\n");
        fprintf(fd, "Bb: %d labels=(", bb->id);
        sep = "";
        for (i = 0; i < bb->nlbls; i++) {;
            fprintf(fd, "%s%s", bb->lbls[i], sep);
            sep = ",";
        }
        fprintf(fd, ")\n");

        /* in edges */
        fprintf(fd, "Pred: ");
        sep = "";
        for (i = 0; i < bsmax(bb->pred); i++) {
            if (bshas(bb->pred, i)) {
                fprintf(fd, "%s%zd", sep, i);
                sep = ",";
            }
        }
        fprintf(fd, "\n");

        /* out edges */
        fprintf(fd, "Succ: ");
        sep = "";
        for (i = 0; i < bsmax(bb->succ); i++) {
             if (bshas(bb->succ, i)) {
                fprintf(fd, "%s%zd", sep, i);
                sep = ",";
             }
        }
        fprintf(fd, "\n");

        for (i = 0; i < bb->nnl; i++)
            dump(bb->nl[i], fd);
        fprintf(fd, "\n");
    }
}
