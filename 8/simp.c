#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "gen.h"
#include "asm.h"

#include "platform.h" /* HACK. We need some platform specific code gen behavior. *sigh.* */

static void lowerglobl(Comp *c, char *name, Node *init);
static void lowerfn(Comp *c, char *name, Node *n, Htab *globls);

static ulong ptrhash(void *key)
{
    return (ulong)key;
}

static int ptreq(void *a, void *b)
{
    return a == b;
}

Fn *mkfn(char *name)
{
    Fn *f;

    f = zalloc(sizeof(Fn));
    f->name = strdup(name);
    f->isglobl = 1;
    f->locs = mkht(ptrhash, ptreq);

    return f;
}

static char *asmname(Node *n)
{
    int i;
    char *s;
    char *sep;
    int len;

    len = strlen(Fprefix);
    for (i = 0; i < n->name.nparts; i++)
        len += strlen(n->name.parts[i]) + 1;

    s = xalloc(len);
    s[0] = '\0';
    sep = Fprefix;
    for (i = 0; i < n->name.nparts; i++) {
        sprintf(s, "%s%s", sep, n->name.parts[i]);
        sep = "$";
    }
    return s;
}

static void lowerglobl(Comp *c, char *name, Node *init)
{
    printf("gen globl %s\n", name);
}

static void lowerfn(Comp *c, char *name, Node *n, Htab *globls)
{
    Fn *fn;
    Node **nl;
    int nn;
    int i;

    /* unwrap to the function body */
    n = n->expr.args[0];
    n = n->lit.fnval;
    assert(n->type == Nfunc);

    /* lower */
    fn = mkfn(name);
    fn->name = strdup(name);

    nl = reduce(fn, n, &nn);

    if (debug) {
      for (i = 0; i < nn; i++) {
        dump(nl[i], stdout);
      }
    }

    fn->nl = nl;
    fn->nn = nn;
    genasm(fn, globls);
}

int isconstfn(Sym *s)
{
    return s->isconst && s->type->type == Tyfunc;
}

void blobdump(Blob *b, FILE *fd)
{
    int i;
    char *p;

    p = b->data;
    fprintf(fd, "\t%s => ", b->name);
    for (i = 0; i < b->ndata; i++)
        if (isprint(p[i]))
            fprintf(fd, "%c", p[i]);
        else
            fprintf(fd, "\\%x", p[i]);
    fprintf(fd, "\n");
}

void gen(Node *file, char *out)
{
    Node **n;
    int nn, i;
    Sym *s;
    char *name;
    Comp *c;
    Htab *globls;

    c = zalloc(sizeof(Comp));

    n = file->file.stmts;
    nn = file->file.nstmts;

    globls = mkht(ptrhash, ptreq);
    /* We need to declare all variables before use */
    for (i = 0; i < nn; i++)
        if (n[i]->type == Ndecl)
            htput(globls, (void*)n[i]->decl.sym->id, asmname(n[i]->decl.sym->name));

    for (i = 0; i < nn; i++) {
        switch (n[i]->type) {
            case Nuse: /* nothing to do */ 
                break;
            case Ndecl:
                s = n[i]->decl.sym;
                name = asmname(s->name);
                if (isconstfn(s)) {
                    lowerfn(c, name, n[i]->decl.init, globls);
                    free(name);
                } else {
                    lowerglobl(c, name, n[i]);
                }
                break;
            default:
                die("Bad node %s in toplevel", nodestr(n[i]->type));
                break;
        }
    }
}
