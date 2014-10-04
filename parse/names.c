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

static char *optab[] =  {
#define O(op, pure) #op,
#include "ops.def"
#undef O
};

int ispureop[] = {
#define O(op, pure) pure,
#include "ops.def"
#undef O
};

static char *nodetab[] =  {
#define N(nt) #nt,
#include "nodes.def"
#undef N
};

static char *littab[] =  {
#define L(lt) #lt,
#include "lits.def"
#undef L
};

static char *tidtab[] =  {
#define Ty(t, n) n,
#include "types.def"
#undef Ty
};

char *opstr(Op o)
{
    return optab[o];
}

char *nodestr(Ntype nt)
{
    return nodetab[nt];
}

char *litstr(Littype lt)
{
    return littab[lt];
}

char *tidstr(Ty tid)
{
    return tidtab[tid];
}
