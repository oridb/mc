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

#include "parse.h"
#include "mi.h"
#include "asm.h"
#include "../config.h"

#define Tdindirect 0x80

static char *insnfmt[] = {
#define Insn(val, gasfmt, p9fmt, use, def) gasfmt,
#include "insns.def"
#undef Insn
};

static char *regnames[] = {
#define Reg(r, gasname, p9name, mode) gasname,
#include "regs.def"
#undef Reg
};

static char* modenames[] = {
	[ModeB] = "b",
	[ModeW] = "w",
	[ModeL] = "l",
	[ModeQ] = "q",
	[ModeF] = "s",
	[ModeD] = "d"
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
	dcl->decl.isglobl = 1;
	htput(globls, dcl, asmname(dcl));

	abortoob = mkexpr(Zloc, Ovar, name, NULL);
	abortoob->expr.type = ty;
	abortoob->expr.did = dcl->decl.did;
	abortoob->expr.isconst = 1;
}

void printmem(FILE *fd, Loc *l, char spec)
{
	if (l->type == Locmem) {
		if (l->mem.constdisp)
			fprintf(fd, "%ld", l->mem.constdisp);
	} else {
		if (l->mem.lbldisp)
			fprintf(fd, "%s", l->mem.lbldisp);
	}
	if (l->mem.base) {
		fprintf(fd, "(");
		locprint(fd, l->mem.base, 'r');
		if (l->mem.idx) {
			fprintf(fd, ",");
			locprint(fd, l->mem.idx, 'r');
		}
		if (l->mem.scale > 1)
			fprintf(fd, ",%d", l->mem.scale);
		if (l->mem.base)
			fprintf(fd, ")");
	} else if (l->type != Locmeml) {
		die("Only locmeml can have unspecified base reg");
	}
}

static void locprint(FILE *fd, Loc *l, char spec)
{
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

void iprintf(FILE *fd, Insn *insn)
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
	i = 0;
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
		case 'r': /* int register */
		case 'f': /* float register */
		case 'm': /* memory */
		case 'i': /* imm */
		case 'v': /* reg/mem */
		case 'u': /* reg/imm */
		case 'x': /* reg/mem/imm */
			locprint(fd, insn->args[idx], *p);
			i++;
			break;
		case 't':
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


static void writebytes(FILE *fd, char *p, size_t sz)
{
	size_t i;

	for (i = 0; i < sz; i++) {
		if (i % 60 == 0)
			fprintf(fd, "\t.ascii \"");
		if (p[i] == '"' || p[i] == '\\')
			fprintf(fd, "\\");
		if (isprint(p[i]))
			fprintf(fd, "%c", p[i]);
		else
			fprintf(fd, "\\%03o", (uint8_t)p[i] & 0xff);
		/* line wrapping for readability */
		if (i % 60 == 59 || i == sz - 1)
			fprintf(fd, "\"\n");
	}
}

void genstrings(FILE *fd, Htab *strtab)
{
	void **k;
	Str *s;
	size_t i, nk;

	k = htkeys(strtab, &nk);
	for (i = 0; i < nk; i++) {
		s = k[i];
		fprintf(fd, "%s:\n", (char*)htget(strtab, k[i]));
		writebytes(fd, s->buf, s->len);
	}
}

static void writeasm(FILE *fd, Isel *s, Func *fn)
{
	size_t i, j;

	if (fn->isexport)
		fprintf(fd, ".globl %s\n", fn->name);
	fprintf(fd, "%s:\n", fn->name);
	for (j = 0; j < s->cfg->nbb; j++) {
		if (!s->bb[j])
			continue;
		for (i = 0; i < s->bb[j]->nlbls; i++)
			fprintf(fd, "%s:\n", s->bb[j]->lbls[i]);
		for (i = 0; i < s->bb[j]->ni; i++)
			iprintf(fd, s->bb[j]->il[i]);
	}
}

static void encodemin(FILE *fd, uint64_t val)
{
	size_t i, shift;
	uint8_t b;

	if (val < 128) {
		fprintf(fd, "\t.byte %zd\n", val);
		return;
	}

	for (i = 1; i < 8; i++)
		if (val < 1ULL << (7*i))
			break;
	shift = 8 - i;
	b = ~0ull << (shift + 1);
	b |= val & ((1 << (8 - shift)) - 1);
	fprintf(fd, "\t.byte %u\n", b);
	val >>=  shift;
	while (val != 0) {
		fprintf(fd, "\t.byte %u\n", (uint)val & 0xff);
		val >>= 8;
	}
}

static void writeblob(FILE *fd, Blob *b)
{
	size_t i;

	if (!b)
		return;
	if (b->lbl) {
		if (b->isglobl)
			fprintf(fd, ".globl %s%s\n", Symprefix, b->lbl);
		fprintf(fd, "%s%s:\n", Symprefix, b->lbl);
	}
	switch (b->type) {
	case Btimin:	encodemin(fd, b->ival);	break;
	case Bti8:	fprintf(fd, "\t.byte %zd\n", b->ival);	break;
	case Bti16:	fprintf(fd, "\t.short %zd\n", b->ival);	break;
	case Bti32:	fprintf(fd, "\t.long %zd\n", b->ival);	break;
	case Bti64:	fprintf(fd, "\t.quad %zd\n", b->ival);	break;
	case Btbytes:	writebytes(fd, b->bytes.buf, b->bytes.len);	break;
	case Btpad:	fprintf(fd, "\t.fill %zd,1,0\n", b->npad);	break;
	case Btref:	fprintf(fd, "\t.quad %s + %zd\n", b->ref.str, b->ref.off);	break;
	case Btseq:
		for (i = 0; i < b->seq.nsub; i++)
			writeblob(fd, b->seq.sub[i]);
		break;
	}
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

void gentype(FILE *fd, Type *ty)
{
	Blob *b;

	if (ty->type == Tyvar)
		return;
	b = tydescblob(ty);
	writeblob(fd, b);
	blobfree(b);
}

void genblob(FILE *fd, Node *blob, Htab *globls, Htab *strtab)
{
	char *lbl;
	Blob *b;

	/* lits and such also get wrapped in decls */
	assert(blob->type == Ndecl);

	lbl = htget(globls, blob);
	if (blob->decl.vis != Visintern)
		fprintf(fd, ".globl %s\n", lbl);
	if (blob->decl.init) {
		fprintf(fd, ".align %zd\n", tyalign(decltype(blob)));
		fprintf(fd, "%s:\n", lbl);

		b = litblob(globls, strtab, blob->decl.init);
		writeblob(fd, b);
		blobfree(b);
	} else {
		fprintf(fd, ".comm %s,%zd,5\n", lbl, size(blob));
	}
}

void gengas(Node *file, char *out)
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
	fprintf(fd, ".data\n");
	for (i = 0; i < nblob; i++)
		genblob(fd, blob[i], globls, strtab);
	fprintf(fd, "\n");

	fprintf(fd, ".text\n");
	for (i = 0; i < nfn; i++)
		genfunc(fd, fn[i], globls, strtab);
	fprintf(fd, "\n");

	for (i = 0; i < ntypes; i++)
		if (types[i]->isreflect && (!types[i]->isimport || types[i]->ishidden))
			gentype(fd, types[i]);
	fprintf(fd, "\n");

	genstrings(fd, strtab);
	fclose(fd);
}

void dbglocprint(FILE *fd, Loc *l, char spec)
{
	locprint(fd, l, spec);
}

void dbgiprintf(FILE *fd, Insn *i)
{
	iprintf(fd, i);
}
