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

int addnode(Cfg *cfg, Bb *bb, Node *n)
{
    switch (exprop(n)) {
        case Ojmp:
        case Ocjmp:
            lappend(&bb->nl, &bb->nnl, n);
            lappend(&cfg->fixjmp, &cfg->nfixjmp, n);
            lappend(&cfg->fixblk, &cfg->nfixblk, n);
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
    Bb *bb;
    int i;
    
    cfg = zalloc(sizeof(Cfg));
    for (i = 0; i < nn; i++) {
        switch (nl[i]->type) {
            case Nlbl:
                if (bb->nnl)
                    bb = mkbb(cfg);
                lappend(&bb->lbls, &bb->nlbls, nl[i]->lbl.name);
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
    return cfg;
}
