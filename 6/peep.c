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

#include "util.h"
#include "parse.h"
#include "mi.h"
#include "asm.h"


/* we sometimes leave dead code generated after
 * a non-conditional jump. This code scans for
 * the non-conditional exit from the BB, and truncate
 * at that point */
static void
deadcode(Isel *s, Asmbb *bb)
{
	size_t i;

	if (!bb)
		return;
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
static void
nopjmp(Isel *s, Asmbb *bb, size_t idx)
{
	Insn *jmp;
	Loc *targ;
	Asmbb *nextbb;
	size_t i;

	/* skip empty bbs */
	if (!bb || !bb->ni)
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
	nextbb = NULL;
	for (i = idx + 1; i < s->nbb; i++) {
		nextbb = s->bb[i];
		if (nextbb)
			break;
	}
	if (!nextbb)
		return;
	for (i = 0; i < nextbb->nlbls; i++) {
		if (!strcmp(nextbb->lbls[i], targ->lbl)) {
			bb->ni--;
			break;
		}
	}
}

void
peep(Isel *s)
{
	size_t i;

	for (i = 0; i < s->nbb; i++) {
		deadcode(s, s->bb[i]);
		nopjmp(s, s->bb[i], i);
	}
}
