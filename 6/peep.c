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
#include "asm.h"


/* we sometimes leave dead code generated after 
 * a non-conditional jump. This code scans for
 * the non-conditional exit from the BB, and truncates
 * at that point */
static void deadcode(Isel *s, Asmbb *bb)
{
    size_t i;

    for (i = 0; i < bb->ni; i++) {
        if (bb->il[i]->op == Ijmp) {
            i++;
            break;
        }
    }
    bb->ni = i;
}

/* checks for of dumb jump code.
 * nop jumps:
 *      jmp .l1
 *      .l1:
 * TODO: check for jumps over jumps.
 */     
static void nopjmp(Isel *s, Asmbb *bb, size_t idx)
{
    Insn *jmp;
    Loc *targ;
    Asmbb *nextbb;
    size_t i;

    /* skip empty bbs */
    if (!bb->ni)
        return;
    /* find the target of the last unconditional
     * jump in the bb */
    targ = NULL;
    if (bb->il[bb->ni - 1]->op == Ijmp) {
        jmp = bb->il[bb->ni - 1];
        if (jmp->args[0]->type == Loclbl)
            targ = jmp->args[0];
    }
    if (!targ)
        return;
    
    /* figure out if it's somewhere in the head of the next bb */
    if (idx < s->nbb)
        nextbb = s->bb[idx + 1];
    for (i = 0; i < nextbb->nlbls; i++) {
        if (!strcmp(nextbb->lbls[i], targ->lbl)) {
            bb->ni--;
            break;
        }
    }
}

static void bbpeep(Isel *s, Asmbb *bb, size_t idx)
{
    deadcode(s, bb);
    nopjmp(s, bb, idx);
}

void peep(Isel *s)
{
    size_t i;

    for (i = 0; i < s->nbb; i++) {
        bbpeep(s, s->bb[i], i);
    }
}
