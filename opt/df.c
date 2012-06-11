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


Bb *mkbb(Cfg *cfg)
{
    static int nextbbid = 0;
    Bb *bb;

    bb = zalloc(sizeof(Bb));
    bb->id = nextbbid++;
    bb->in = mkbs();
    bb->out = mkbs();
    lappend(&cfg->bb, &cfg->nbb, bb);
    return bb;
}

void label(Cfg *cfg, Node *lbl, Bb *bb)
{
    htput(cfg->lblmap, lbl->lbl.name, bb);
    lappend(&bb->lbls, &bb->nlbls, lbl->lbl.name);
}

int addnode(Cfg *cfg, Bb *bb, Node *n)
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

Cfg *mkcfg(Node **nl, int nn)
{
    Cfg *cfg;
    Bb *bb, *targ;
    Node *a, *b;
    int i;

    cfg = zalloc(sizeof(Cfg));
    cfg->lblmap = mkht(strhash, streq);
    bb = mkbb(cfg);
    for (i = 0; i < nn; i++) {
        switch (nl[i]->type) {
            case Nlbl:
                if (bb->nnl)
                    bb = mkbb(cfg);
                label(cfg, nl[i], bb);
                break;
            case Nexpr:
                if (addnode(cfg, bb, nl[i]))
                    bb = mkbb(cfg);
                break;
            case Ndecl:
                break;
            default:
                die("Invalid node type %s in mkcfg", nodestr(nl[i]->type));
        }
    }
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
            targ = htget(cfg->lblmap, a->lbl.name);
            if (!targ)
                die("No bb with label %s", a->lbl.name);
            bsput(bb->out, targ->id);
            bsput(targ->in, bb->id);
        }
        if (b) {
            targ = htget(cfg->lblmap, b->lbl.name);
            if (!targ)
                die("No bb with label %s", b->lbl.name);
            bsput(bb->out, targ->id);
            bsput(targ->in, bb->id);
        }
    }
    return cfg;
}
void dumpcfg(Cfg *cfg, FILE *fd)
{
    int i, j;
    Bb *bb;
    char *sep;

    for (j = 0; j < cfg->nbb; j++) {
        bb = cfg->bb[j];
        fprintf(fd, "\n");
        fprintf(fd, "Bb: %d\n", bb->id);

        /* in edges */
        fprintf(fd, "In:  ");
        sep = "";
        for (i = 0; i < bsmax(bb->in); i++) {
             if (bshas(bb->in, i))
                fprintf(fd, "%d%s", i, sep);
             sep = ",";
        }
        fprintf(fd, "\n");

        /* out edges */
        fprintf(fd, "Out: ");
        sep = "";
        for (i = 0; i < bsmax(bb->out); i++) {
             if (bshas(bb->in, i)) {
                fprintf(fd, "%d%s", i, sep);
                sep = ",";
             }
        }
        fprintf(fd, "\n");

        for (i = 0; i < bb->nnl; i++)
            dump(bb->nl[i], fd);
        fprintf(fd, "\n");
    }
}
