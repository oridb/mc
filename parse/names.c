#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "parse.h"

char *opstr[] = {
#define O(op, pure, class, pretty) #op,
#include "ops.def"
#undef O
};

char *oppretty[] = {
#define O(op, pure, class, pretty) pretty,
#include "ops.def"
#undef O
};

int opispure[] = {
#define O(op, pure, class, pretty) pure,
#include "ops.def"
#undef O
};

int opclass[] = {
#define O(op, pure, class, pretty) class,
#include "ops.def"
#undef O
};

char *nodestr[] = {
#define N(nt) #nt,
#include "nodes.def"
#undef N
};

char *litstr[] = {
#define L(lt) #lt,
#include "lits.def"
#undef L
};

char *tidstr[] = {
#define Ty(t, n, stk) n,
#include "types.def"
#undef Ty
};
