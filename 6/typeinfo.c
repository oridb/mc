#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "opt.h"
#include "asm.h"
#include "../config.h"


size_t tysize(Type *t)
{
    size_t sz;
    size_t i;

    sz = 0;
    if (!t)
        die("size of empty type => bailing.");
    switch (t->type) {
        case Tyvoid:
            die("void has no size");
            return 1;
        case Tybool: case Tyint8:
        case Tybyte: case Tyuint8:
            return 1;
        case Tyint16: case Tyuint16:
            return 2;
        case Tyint: case Tyint32:
        case Tyuint: case Tyuint32:
        case Tychar:  /* utf32 */
            return 4;

        case Typtr: case Tyfunc:
        case Tyvalist: /* ptr to first element of valist */
            return Ptrsz;

        case Tyint64: case Tylong:
        case Tyuint64: case Tyulong:
            return 8;

            /*end integer types*/
        case Tyflt32:
            return 4;
        case Tyflt64:
            return 8;

        case Tyslice:
            return 2*Ptrsz; /* len; ptr */
        case Tyname:
            return tysize(t->sub[0]);
        case Tyarray:
            t->asize = fold(t->asize, 1);
            assert(exprop(t->asize) == Olit);
            return t->asize->expr.args[0]->lit.intval * tysize(t->sub[0]);
        case Tytuple:
            for (i = 0; i < t->nsub; i++) {
                sz = alignto(sz, t->sub[i]);
                sz += tysize(t->sub[i]);
            }
            sz = alignto(sz, t);
            return sz;
            break;
        case Tystruct:
            for (i = 0; i < t->nmemb; i++) {
                sz = alignto(sz, decltype(t->sdecls[i]));
                sz += size(t->sdecls[i]);
            }
            sz = alignto(sz, t);
            return sz;
            break;
        case Tyunion:
            sz = Wordsz;
            for (i = 0; i < t->nmemb; i++)
                if (t->udecls[i]->etype)
                    sz = max(sz, tysize(t->udecls[i]->etype) + Wordsz);
            return align(sz, Ptrsz);
            break;
        case Tybad: case Tyvar: case Typaram: case Tyunres: case Ntypes:
            die("Type %s does not have size; why did it get down to here?", tystr(t));
            break;
    }
    return -1;
}

/* gets the byte offset of 'memb' within the aggregate type 'aggr' */
size_t tyoffset(Type *ty, Node *memb)
{
    size_t i;
    size_t off;

    ty = tybase(ty);
    if (ty->type == Typtr)
        ty = tybase(ty->sub[0]);

    assert(ty->type == Tystruct);
    off = 0;
    for (i = 0; i < ty->nmemb; i++) {
        off = alignto(off, decltype(ty->sdecls[i]));
        if (!strcmp(namestr(memb), declname(ty->sdecls[i])))
            return off;
        off += size(ty->sdecls[i]);
    }
    die("Could not find member %s in struct", namestr(memb));
    return -1;
}

size_t size(Node *n)
{
    Type *t;

    if (n->type == Nexpr)
        t = n->expr.type;
    else
        t = n->decl.type;
    return tysize(t);
}

size_t offset(Node *aggr, Node *memb)
{
    return tyoffset(exprtype(aggr), memb);
}

