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
#include "asm.h"

struct Asmbb {
    int id;
    char **lbls;
    size_t nlbls;
    Insn **insns;
    size_t ninsns;
    Bitset *livein;
    Bitset *liveout;
};

void adduses(Insn *i, Bitset *bs)
{
}

void subdefs(Insn *i, Bitset *bs)
{
}
