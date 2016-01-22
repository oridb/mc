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
#include "mi.h"
#include "asm.h"
#include "../config.h"

/* string tables */
static char *insnfmt[] = {
#define Insn(val, gasfmt, p9fmt, use, def) p9fmt,
#include "insns.def"
#undef Insn
};

static char *regnames[] = {
#define Reg(r, gasname, p9name, mode) p9name,
#include "regs.def"
#undef Reg
};

static char* modenames[] = {
	[ModeB] = "B",
	[ModeW] = "W",
	[ModeL] = "L",
	[ModeQ] = "Q",
	[ModeF] = "S",
	[ModeD] = "D"
};

static void locprint(FILE *fd, Loc *l, char spec);

static void initconsts(Htab *globls)
{
	Type *ty;
	Node *name;
	Node *dcl;

	tyintptr = mktype(Zloc, Tyuint64);
	tyword = mktype(Zloc, Tyuint);
	tyvoid = mktype(Zloc, Tyvoid);

	ty = mktyfunc(Zloc, NULL, 0, mktype(Zloc, Tyvoid));
	ty->type = Tycode;
	name = mknsname(Zloc, "_rt", "abort_oob");
	dcl = mkdecl(Zloc, name, ty);
	dcl->decl.isconst = 1;
	dcl->decl.isextern = 1;
	htput(globls, dcl, asmname(dcl));

	abortoob = mkexpr(Zloc, Ovar, name, NULL);
	abortoob->expr.type = ty;
	abortoob->expr.did = dcl->decl.did;
	abortoob->expr.isconst = 1;
}

static void printmem(FILE *fd, Loc *l, char spec)
{
	if (l->type == Locmem) {
		if (l->mem.constdisp)
			fprintf(fd, "%ld", l->mem.constdisp);
	} else if (l->mem.lbldisp) {
		fprintf(fd, "%s", l->mem.lbldisp);
	}
	if (!l->mem.base || l->mem.base->reg.colour == Rrip) {
		fprintf(fd, "+0(SB)");
	} else {
		fprintf(fd, "(");
		locprint(fd, l->mem.base, 'r');
		fprintf(fd, ")");
	}
	if (l->mem.idx) {
		fprintf(fd, "(");
		locprint(fd, l->mem.idx, 'r');
		fprintf(fd, "*%d", l->mem.scale);
		fprintf(fd, ")");
	}
}

static void locprint(FILE *fd, Loc *l, char spec)
{
	spec = tolower(spec);
	assert(l->mode);
	switch (l->type) {
	case Loclitl:
		assert(spec == 'i' || spec == 'x' || spec == 'u');
		fprintf(fd, "$%s", l->lbl);
		break;
	case Loclbl:
		assert(spec == 'm' || spec == 'v' || spec == 'x');
		fprintf(fd, "%s", l->lbl);
		break;
	case Locreg:
		assert((spec == 'r' && isintmode(l->mode)) || 
				(spec == 'f' && isfloatmode(l->mode)) ||
				spec == 'v' ||
				spec == 'x' ||
				spec == 'u');
		if (l->reg.colour == Rnone)
			fprintf(fd, "%%P.%zd%s", l->reg.id, modenames[l->mode]);
		else
			fprintf(fd, "%s", regnames[l->reg.colour]);
		break;
	case Locmem:
	case Locmeml:
		assert(spec == 'm' || spec == 'v' || spec == 'x');
		printmem(fd, l, spec);
		break;
	case Loclit:
		assert(spec == 'i' || spec == 'x' || spec == 'u');
		fprintf(fd, "$%ld", l->lit);
		break;
	case Locnone:
		die("Bad location in locprint()");
		break;
	}
}

static int issubreg(Loc *a, Loc *b)
{
	return rclass(a) == rclass(b) && a->mode != b->mode;
}

static void iprintf(FILE *fd, Insn *insn)
{
	char *p;
	int i;
	int idx;

	/* x64 has a quirk; it has no movzlq because mov zero extends. This
	 * means that we need to do a movl when we really want a movzlq. Since
	 * we don't know the name of the reg to use, we need to sub it in when
	 * writing... */
	switch (insn->op) {
	case Imovzx:
		if (insn->args[0]->mode == ModeL && insn->args[1]->mode == ModeQ) {
			if (insn->args[1]->reg.colour) {
				insn->op = Imov;
				insn->args[1] = coreg(insn->args[1]->reg.colour, ModeL);
			}
		}
		break;
	case Imovs:
		if (insn->args[0]->reg.colour == Rnone || insn->args[1]->reg.colour == Rnone)
			break;
		/* moving a reg to itself is dumb. */
		if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
			return;
		break;
	case Imov:
		assert(!isfloatmode(insn->args[0]->mode));
		if (insn->args[0]->type != Locreg || insn->args[1]->type != Locreg)
			break;
		if (insn->args[0]->reg.colour == Rnone || insn->args[1]->reg.colour == Rnone)
			break;
		/* if one reg is a subreg of another, we can just use the right
		 * mode to move between them. */
		if (issubreg(insn->args[0], insn->args[1]))
			insn->args[0] = coreg(insn->args[0]->reg.colour, insn->args[1]->mode);
		/* moving a reg to itself is dumb. */
		if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
			return;
		break;
	default:
		break;
	}
	p = insnfmt[insn->op];
	i = 0; /* NB: this is 1 based indexing */
	for (; *p; p++) {
		if (*p !=  '%') {
			fputc(*p, fd);
			continue;
		}

		/* %-formating */
		p++;
		idx = i;
again:
		switch (*p) {
		case '\0':
			goto done; /* skip the final p++ */
			break;
		case 'R': /* int register */
		case 'F': /* float register */
		case 'M': /* memory */
		case 'I': /* imm */
		case 'V': /* reg/mem */
		case 'U': /* reg/imm */
		case 'X': /* reg/mem/imm */
			locprint(fd, insn->args[idx], *p);
			i++;
			break;
		case 'T':
			fputs(modenames[insn->args[idx]->mode], fd);
			break;
		default:
			/* the  asm description uses 1-based indexing, so that 0
			 * can be used as a sentinel. */
			if (!isdigit(*p))
				die("Invalid %%-specifier '%c'", *p);
			idx = strtol(p, &p, 10) - 1;
			goto again;
			break;
		}
	}
done:
	return;
}


static size_t writebytes(FILE *fd, char *name, size_t off, char *p, size_t sz)
{
	size_t i, len;

	assert(sz != 0);
	for (i = 0; i < sz; i++) {
		len = min(sz - i, 8);
		if (i % 8 == 0)
			fprintf(fd, "\tDATA %s+%zd(SB)/%zd,$\"", name, off + i, len);
		if (p[i] == '"' || p[i] == '\\')
			fprintf(fd, "\\");
		if (isprint(p[i]))
			fprintf(fd, "%c", p[i]);
		else
			fprintf(fd, "\\%03o", (uint8_t)p[i] & 0xff);
		/* line wrapping for readability */
		if (i % 8 == 7 || i == sz - 1)
			fprintf(fd, "\"\n");
	}

	return sz;
}

static void genstrings(FILE *fd, Htab *strtab)
{
	void **k;
	char *lbl;
	Str *s;
	size_t i, nk;

	k = htkeys(strtab, &nk);
	for (i = 0; i < nk; i++) {
		s = k[i];
		lbl = htget(strtab, k[i]);
		if (s->len) {
			fprintf(fd, "GLOBL %s+0(SB),$%lld\n", lbl, (vlong)s->len);
			writebytes(fd, lbl, 0, s->buf, s->len);
		}
	}
}

static void writeasm(FILE *fd, Isel *s, Func *fn)
{
	size_t i, j;
	char *hidden;

	hidden = "";
	if (fn->isexport)
		hidden = "";
	/* we don't use the stack size directive: myrddin handles
	 * the stack frobbing on its own */
	fprintf(fd, "TEXT %s%s+0(SB),$0\n", fn->name, hidden);
	for (j = 0; j < s->cfg->nbb; j++) {
		if (!s->bb[j])
			continue;
		for (i = 0; i < s->bb[j]->nlbls; i++)
			fprintf(fd, "%s:\n", s->bb[j]->lbls[i]);
		for (i = 0; i < s->bb[j]->ni; i++)
			iprintf(fd, s->bb[j]->il[i]);
	}
}

static size_t encodemin(FILE *fd, uvlong val, size_t off, char *lbl)
{
	size_t i, shift, n;
	uint8_t b;

	if (val < 128) {
		fprintf(fd, "\tDATA %s+%zd(SB)/1,$%llu //x\n", lbl, off, val);
		return 1;
	}

	for (i = 1; i < 8; i++)
		if (val < 1ULL << (7*i))
			break;

	n = 0;
	shift = 8 - i;
	b = ~0 << (shift + 1);
	b |= val & ((1 << (8 - shift)) - 1);
	fprintf(fd, "\tDATA %s+%zd(SB)/1,$%u\n", lbl, off, b);
	val >>=  shift;
	while (val != 0) {
		n++;
		fprintf(fd, "\tDATA %s+%zd(SB)/1,$%u// y\n", lbl, off+n, (uint)val & 0xff);
		val >>= 8;
	}
	return i;
}

static size_t writeblob(FILE *fd, Blob *b, size_t off, char *lbl)
{
	size_t i, n;

	n = 0;
	if (!b)
		return 0;
	switch (b->type) {
	case Bti8:
		fprintf(fd, "\tDATA %s+%zd(SB)/1,$%llud\n", lbl, off+n, b->ival);
		n += 1;
		break;
	case Bti16:
		fprintf(fd, "\tDATA %s+%zd(SB)/2,$%llud\n", lbl, off+n, b->ival);
		n += 2;
		break;
	case Bti32:
		fprintf(fd, "\tDATA %s+%zd(SB)/4,$%llud\n", lbl, off+n, b->ival);
		n += 4;
		break;
	case Bti64:
		fprintf(fd, "\tDATA %s+%zd(SB)/8,$%lld\n", lbl, off+n, (vlong)b->ival);
		n += 8;
		break;
	case Btimin:
		n += encodemin(fd, b->ival, off+n, lbl);
		break;
	case Btref:
		if (b->ref.isextern || b->ref.str[0] == '.')
			fprintf(fd, "\tDATA %s+%zd(SB)/8,$%s+%zd(SB)\n",
					lbl, off+n, b->ref.str, b->ref.off);
		else
			fprintf(fd, "\tDATA %s+%zd(SB)/8,$%s<>+%zd(SB)\n",
					lbl, off+n, b->ref.str, b->ref.off);
		n += 8;
		break;
	case Btbytes:
		n += writebytes(fd, lbl, off+n, b->bytes.buf, b->bytes.len);
		break;
	case Btseq:
		for (i = 0; i < b->seq.nsub; i++)
			n += writeblob(fd, b->seq.sub[i], off+n, lbl);
		break;
	case Btpad:
		for (i = 0; i < b->npad; i++)
			fprintf(fd, "\tDATA %s+%zd(SB)/1,$0\n", lbl, off+n+i);
		n += b->npad;
		break;
	}
	return n;
}

/* genfunc requires all nodes in 'nl' to map cleanly to operations that are
 * natively supported, as promised in the output of reduce().  No 64-bit
 * operations on x32, no structures, and so on. */
static void genfunc(FILE *fd, Func *fn, Htab *globls, Htab *strtab)
{
	Isel is = {0,};

	is.reglocs = mkht(varhash, vareq);
	is.stkoff = fn->stkoff;
	is.envoff = fn->envoff;
	is.globls = globls;
	is.ret = fn->ret;
	is.cfg = fn->cfg;
	if (fn->hasenv)
		is.envp = locreg(ModeQ);

	selfunc(&is, fn, globls, strtab);
	if (debugopt['i'])
		writeasm(stdout, &is, fn);
	writeasm(fd, &is, fn);
}

static void gentype(FILE *fd, Type *ty)
{
	Blob *b;
	char lbl[1024];

	ty = tydedup(ty);
	if (ty->type == Tyvar || ty->isemitted)
		return;

	ty->isemitted = 1;
	b = tydescblob(ty);
	if (!b)
		return;
	if (b->isglobl) {
		fprintf(fd, "GLOBL %s%s+0(SB),$%zd\n", Symprefix, b->lbl, blobsz(b));
		bprintf(lbl, sizeof lbl, "%s%s", Symprefix, b->lbl);
	} else {
		fprintf(fd, "GLOBL %s%s<>+0(SB),$%zd\n", Symprefix, b->lbl, blobsz(b));
		bprintf(lbl, sizeof lbl, "%s%s<>", Symprefix, b->lbl);
	}
	writeblob(fd, b, 0, lbl);
}

static void gentypes(FILE *fd)
{
	Type *ty;
	size_t i;

	for (i = Ntypes; i < ntypes; i++) {
		if (!types[i]->isreflect)
			continue;
		ty = tydedup(types[i]);
		if (ty->isemitted || ty->isimport)
			continue;
		gentype(fd, ty);
	}
	fprintf(fd, "\n");
}


static void genblob(FILE *fd, Node *blob, Htab *globls, Htab *strtab)
{
	char *lbl;
	Blob *b;

	/* lits and such also get wrapped in decls */
	assert(blob->type == Ndecl);

	lbl = htget(globls, blob);
	fprintf(fd, "GLOBL %s+0(SB),$%zd\n", lbl, size(blob));
	if (blob->decl.init)
		b = litblob(globls, strtab, blob->decl.init);
	else
		b = mkblobpad(size(blob));
	writeblob(fd, b, 0, lbl);
}

void genp9(Node *file, char *out)
{
	Htab *globls, *strtab;
	Node *n, **blob;
	Func **fn;
	size_t nfn, nblob;
	size_t i;
	FILE *fd;

	/* ensure that all physical registers have a loc created before any
	 * other locs, so that locmap[Physreg] maps to the Loc for the physreg
	 * in question */
	for (i = 0; i < Nreg; i++)
		locphysreg(i);

	fn = NULL;
	nfn = 0;
	blob = NULL;
	nblob = 0;
	globls = mkht(varhash, vareq);
	initconsts(globls);

	/* We need to define all global variables before use */
	fillglobls(file->file.globls, globls);

	pushstab(file->file.globls);
	for (i = 0; i < file->file.nstmts; i++) {
		n = file->file.stmts[i];
		switch (n->type) {
		case Nuse: /* nothing to do */ 
		case Nimpl:
			break;
		case Ndecl:
			n = flattenfn(n);
			simpglobl(n, globls, &fn, &nfn, &blob, &nblob);
			break;
		default:
			die("Bad node %s in toplevel", nodestr[n->type]);
			break;
		}
	}
	popstab();

	fd = fopen(out, "w");
	if (!fd)
		die("Couldn't open fd %s", out);

	strtab = mkht(strlithash, strliteq);
	for (i = 0; i < nblob; i++)
		genblob(fd, blob[i], globls, strtab);
	for (i = 0; i < nfn; i++)
		genfunc(fd, fn[i], globls, strtab);
	gentypes(fd);
	fprintf(fd, "\n");

	genstrings(fd, strtab);
	fprintf(fd, "\n");

	fclose(fd);
}
