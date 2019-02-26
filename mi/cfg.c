#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
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


static Bb *
mkbb(Cfg *cfg)
{
	Bb *bb;

	bb = zalloc(sizeof(Bb));
	bb->id = cfg->nextbbid++;
	bb->pred = mkbs();
	bb->succ = mkbs();
	lappend(&cfg->bb, &cfg->nbb, bb);
	return bb;
}


static void
strlabel(Cfg *cfg, char *lbl, Bb *bb)
{
	if (htget(cfg->lblmap, lbl) != bb) {
		htput(cfg->lblmap, lbl, bb);
		lappend(&bb->lbls, &bb->nlbls, lbl);
	}
}

static void
label(Cfg *cfg, Node *lbl, Bb *bb)
{
	strlabel(cfg, lblstr(lbl), bb);
}

static int
isnonretcall(Node *fn)
{
	Node *dcl;

	if (exprop(fn) != Ovar)
		return 0;
	dcl = decls[fn->expr.did];
	return dcl->decl.isnoret;
}

static int
addnode(Cfg *cfg, Bb *bb, Node *n)
{
	switch (exprop(n)) {
	case Ojmp:
	case Ocjmp:
	case Oret:
		lappend(&bb->nl, &bb->nnl, n);
		lappend(&cfg->fixjmp, &cfg->nfixjmp, n);
		lappend(&cfg->fixblk, &cfg->nfixblk, bb);
		return 1;
	case Ocall:
		lappend(&bb->nl, &bb->nnl, n);
		return isnonretcall(n->expr.args[0]);
	case Odead:
		lappend(&bb->nl, &bb->nnl, n);
		return 1;
	default:
		lappend(&bb->nl, &bb->nnl, n);
		break;
	}
	return 0;
}

static int
islabel(Node *n)
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

static Bb *
addlabel(Cfg *cfg, Bb *bb, Node **nl, size_t i, Srcloc loc)
{
	/* if the current block assumes fall-through, insert an explicit jump */
	if (i > 0 && nl[i - 1]->type == Nexpr) {
		if (exprop(nl[i - 1]) != Ocjmp && exprop(nl[i - 1]) != Ojmp)
			addnode(cfg, bb, mkexpr(loc, Ojmp, mklbl(loc, lblstr(nl[i])), NULL));
	}
	if (bb->nnl)
		bb = mkbb(cfg);
	label(cfg, nl[i], bb);
	return bb;
}

void
delete(Cfg *cfg, Bb *bb)
{
	size_t i, j;

	if (bb == cfg->start || bb == cfg->end)
		return;
	for (i = 0; bsiter(bb->pred, &i); i++) {
		bsunion(cfg->bb[i]->succ, bb->succ);
		bsdel(cfg->bb[i]->succ, bb->id);
	}
	for (i = 0; bsiter(bb->succ, &i); i++) {
		bsunion(cfg->bb[i]->pred, bb->pred);
		bsdel(cfg->bb[i]->pred, bb->id);
		for (j = 0; j < bb->nlbls; j++)
			strlabel(cfg, bb->lbls[j], cfg->bb[i]);
	}
	cfg->bb[bb->id] = NULL;
}

void
noexit(Cfg *cfg, Bb *bb)
{
	size_t i;
	for (i = 0; bsiter(bb->succ, &i); i++)
		bsdel(cfg->bb[i]->pred, bb->id);
	bsclear(bb->succ);
}

void
trimdead(Cfg *cfg, Bb *bb)
{
	size_t i;

	if (!bb)
		return;
	for (i = 0; i < bb->nnl; i++) {
		switch (exprop(bb->nl[i])) {
			/* if we're jumping, we can't keep going
			 * within this BB */
		case Ojmp:
		case Ocjmp:
		case Oret:
			bb->nnl = i + 1;
			return;
		case Odead:
			noexit(cfg, bb);
			bb->nnl = i + 1;
			return;
		case Ocall:
			if (isnonretcall(bb->nl[i]->expr.args[0])) {
				noexit(cfg, bb);
				bb->nnl = i + 1;
				return;
			}
			break;
		default:
			/* nothing */
			break;
		}
	}
}

void
trim(Cfg *cfg)
{
	Bb *bb;
	size_t i;
	int deleted;

	deleted = 1;
	while (deleted) {
		deleted = 0;
		for (i = 0; i < cfg->nbb; i++) {
			bb = cfg->bb[i];
			if (bb == cfg->start || bb == cfg->end)
				continue;
			trimdead(cfg, bb);
			if (bb && bsisempty(bb->pred)) {
				delete(cfg, bb);
				deleted = 1;
			}
		}
	}
}

Cfg *
mkcfg(Node *fn, Node **nl, size_t nn)
{
	Cfg *cfg;
	Bb *pre, *post;
	Bb *bb, *targ;
	Node *a, *b;
	static int nextret;
	char buf[32];
	size_t i;

	cfg = zalloc(sizeof(Cfg));
	cfg->fn = fn;
	cfg->lblmap = mkht(strhash, streq);
	pre = mkbb(cfg);
	bb = mkbb(cfg);
	for (i = 0; i < nn; i++) {
		switch (nl[i]->type) {
		case Nexpr:
			if (islabel(nl[i]))
				bb = addlabel(cfg, bb, nl, i, nl[i]->loc);
			else if (addnode(cfg, bb, nl[i]))
				bb = mkbb(cfg);
			break;
			break;
		case Ndecl:
			break;
		default:
			die("Invalid node type %s in mkcfg", nodestr[nl[i]->type]);
		}
	}
	post = mkbb(cfg);
	bprintf(buf, sizeof buf, ".Lret%d", nextret++);
	label(cfg, mklbl(fn->loc, buf), post);

	cfg->start = pre;
	cfg->end = post;
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
		case Oret:
			a = mklbl(cfg->fixjmp[i]->loc, cfg->end->lbls[0]);
			b = NULL;
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
        if (debugopt['C'])
            dumpcfg(cfg, stdout);
	trim(cfg);
	return cfg;
}

void
dumpbb(Bb *bb, FILE *fd)
{
	size_t i;
	char *sep;

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
		dumpn(bb->nl[i], fd);
	fprintf(fd, "\n");
}

void
dumpcfg(Cfg *cfg, FILE *fd)
{
	size_t i;

	for (i = 0; i < cfg->nbb; i++) {
		if (!cfg->bb[i])
			continue;
		fprintf(fd, "\n");
		dumpbb(cfg->bb[i], fd);
	}
}
