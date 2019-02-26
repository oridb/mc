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
#include <errno.h>

#include "util.h"
#include "parse.h"
#include "mi.h"
#include "asm.h"
#include "../config.h"

#define Tdindirect 0x80

static char * insnfmt[] = {
#define Insn(val, gasfmt, p9fmt, use, def) gasfmt,
#include "insns.def"
#undef Insn
};

static char * regnames[] = {
#define Reg(r, gasname, p9name, mode) gasname,
#include "regs.def"
#undef Reg
};

static char *modenames[] = {
	[ModeB] = "b",
	[ModeW] = "w",
	[ModeL] = "l",
	[ModeQ] = "q",
	[ModeF] = "s",
	[ModeD] = "d"
};

static void locprint(FILE *fd, Loc *l, char spec);

void
printmem(FILE *fd, Loc *l, char spec)
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

static void
locprint(FILE *fd, Loc *l, char spec)
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

static int
issubreg(Loc *a, Loc *b)
{
	return rclass(a) == rclass(b) && a->mode != b->mode;
}

void
iprintf(FILE *fd, Insn *insn)
{
	char *p;
	int i;
	int idx;

	/* x64 has a quirk; it has no movzlq because mov zero extends. Thi
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
		/* moving a reg to itself is dumb. */
		if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
                    return;
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
done
:
	return;
}


static void
writebytes(FILE *fd, char *p, size_t sz)
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

void
genstrings(FILE *fd, Htab *strtab)
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

static void
writeasm(FILE *fd, Isel *s, Func *fn)
{
	size_t i, j;

	switch (asmsyntax) {
	case Gnugaself:
		//fprintf(fd, ".section .text.%s,\"ax\",@progbits\n", fn->name);
		fprintf(fd, ".type %s, @function\n", fn->name);
		break;
	case Gnugasmacho:
		fprintf(fd, ".section __TEXT,__text,regular\n");
		break;
	default:
		die("unknown target");  break;
	}
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

static void
encodemin(FILE *fd, uvlong val)
{
	size_t i, shift;
	uint8_t b;

	if (val < 128) {
		fprintf(fd, "\t.byte %llu\n", val);
		return;
	}

	for (i = 1; i < 8; i++)
		if (val < 1ULL << (7*i))
			break;
	shift = 8 - i;
	b = ~0ull << (shift + 1);
	b |= val & ~(~0ull << shift);
	fprintf(fd, "\t.byte %u\n", b);
	val >>=  shift;
	while (val != 0) {
		fprintf(fd, "\t.byte %u\n", (uint)val & 0xff);
		val >>= 8;
	}
}

static void
emitonce(FILE *fd, Blob *b)
{
	if (asmsyntax == Gnugaself) {
		fprintf(fd, ".section .text.%s%s,\"aG\",%s%s,comdat\n",
			Symprefix, b->lbl, Symprefix, b->lbl);
	} else if (asmsyntax == Gnugasmacho) {
		if (b->isglobl)
			fprintf(fd, ".weak_def_can_be_hidden %s%s\n", Symprefix, b->lbl);
	} else {
		die("Unknown asm flavor");
	}
}

static void
writeblob(FILE *fd, Blob *b)
{
	size_t i;

	if (!b)
		return;
	if (b->lbl) {
		if (b->iscomdat)
			emitonce(fd, b);
		if (b->isglobl)
			fprintf(fd, ".globl %s%s\n", Symprefix, b->lbl);
		fprintf(fd, "%s%s:\n", Symprefix, b->lbl);
	}
	switch (b->type) {
	case Btimin:	encodemin(fd, b->ival);	break;
	case Bti8:	fprintf(fd, "\t.byte %llu\n", b->ival);	break;
	case Bti16:	fprintf(fd, "\t.short %llu\n", b->ival);	break;
	case Bti32:	fprintf(fd, "\t.long %llu\n", b->ival);	break;
	case Bti64:	fprintf(fd, "\t.quad %llu\n", b->ival);	break;
	case Btbytes:	writebytes(fd, b->bytes.buf, b->bytes.len);	break;
	case Btpad:	fprintf(fd, "\t.fill %llu,1,0\n", b->npad);	break;
	case Btref:	fprintf(fd, "\t.quad %s + %zd\n", b->ref.str, b->ref.off);	break;
	case Btseq:
		for (i = 0; i < b->seq.nsub; i++)
			writeblob(fd, b->seq.sub[i]);
		break;
	}
	if (b->lbl && b->iscomdat)
		fprintf(fd, ".text\n");
}

/* genfunc requires all nodes in 'nl' to map cleanly to operations that are
 * natively supported, as promised in the output of reduce().  No 64-bit
 * operations on x32, no structures, and so on. */
static void
genfunc(FILE *fd, Func *fn, Htab *globls, Htab *strtab)
{
	Isel is = {0,};
	char cwd[1024];

	resetregs();
	if (!getcwd(cwd, sizeof cwd))
		die("getcwd failed: %s\n", cwd);
	is.reglocs = mkht(varhash, vareq);
	is.name = fn->name;
	is.stkoff = fn->stkoff;
	is.envoff = fn->envoff;
	is.globls = globls;
	is.ret = fn->ret;
	is.cfg = fn->cfg;
	is.cwd = strdup(cwd);

	if (fn->hasenv)
		is.envp = locreg(ModeQ);

	selfunc(&is, fn, globls, strtab);
	if (debugopt['i'])
		writeasm(stdout, &is, fn);
	writeasm(fd, &is, fn);
}

static void
gentype(FILE *fd, Type *ty)
{
	Blob *b;

	ty = tydedup(ty);
	if (ty->type == Tyvar || ty->isemitted)
		return;

	ty->isemitted = 1;
	b = tydescblob(ty);
	if (b->isglobl)
		b->iscomdat = 1;
	if (asmsyntax == Gnugaself)
		fprintf(fd, ".section .data.%s,\"aw\",@progbits\n", b->lbl);
	writeblob(fd, b);
	blobfree(b);
}

static void
gentypes(FILE *fd)
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


void
genblob(FILE *fd, Node *blob, Htab *globls, Htab *strtab)
{
	char *lbl;
	Blob *b;

	/* lits and such also get wrapped in decls */
	assert(blob->type == Ndecl);

	lbl = htget(globls, blob);
	if (blob->decl.vis != Visintern)
		fprintf(fd, ".globl %s\n", lbl);
	if (asmsyntax == Gnugaself)
		fprintf(fd, ".section .data.%s,\"aw\",@progbits\n", lbl);
	if (blob->decl.init) {
		fprintf(fd, ".align %zd\n", tyalign(decltype(blob)));
		fprintf(fd, "%s:\n", lbl);

		b = litblob(globls, strtab, blob->decl.init);
		writeblob(fd, b);
		blobfree(b);
	} else {
		fprintf(fd, ".comm %s,%zd\n", lbl, size(blob));
	}
}

void
gengas(FILE *fd)
{
	Htab *globls, *strtab;
	Node *n, **blob;
	Func **fn;
	char dir[1024], *path;
	size_t nfn, nblob;
	size_t i;

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
	fillglobls(file.globls, globls);

	pushstab(file.globls);
	for (i = 0; i < file.nstmts; i++) {
		n = file.stmts[i];
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

	if (!getcwd(dir, sizeof dir))
		die("could not get cwd: %s\n", strerror(errno));
	for (i = 0; i < file.nfiles; i++) {
		path = file.files[i];
		fprintf(fd, ".file %zd \"%s/%s\"\n", i + 1, dir, path);
	}

	strtab = mkht(strlithash, strliteq);
	fprintf(fd, ".data\n");
	for (i = 0; i < nblob; i++)
		genblob(fd, blob[i], globls, strtab);
	fprintf(fd, "\n");

	fprintf(fd, ".text\n");
	for (i = 0; i < nfn; i++)
		genfunc(fd, fn[i], globls, strtab);
	gentypes(fd);
	fprintf(fd, "\n");

	genstrings(fd, strtab);
	fprintf(fd, "\n");

	fclose(fd);
}

void
dbglocprint(FILE *fd, Loc *l, char spec)
{
	locprint(fd, l, spec);
}

void
dbgiprintf(FILE *fd, Insn *i)
{
	iprintf(fd, i);
}
