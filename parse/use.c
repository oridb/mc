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

typedef struct Typefix Typefix;
struct Typefix {
	Type **dest;
	intptr_t id;
};

typedef struct Traitfix Traitfix;
struct Traitfix {
	Trait **dest;
	Type *type;
	intptr_t id;
};

static void wrtype(FILE *fd, Type *val);
static void rdtype(FILE *fd, Type **dest);
static void wrstab(FILE *fd, Stab *val);
static Stab *rdstab(FILE *fd, int isfunc);
static void wrsym(FILE *fd, Node *val);
static Node *rdsym(FILE *fd, Trait *ctx);
static void pickle(FILE *fd, Node *n);
static Node *unpickle(FILE *fd);

/* type fixup list */
static Htab *trdeduptab;	/* map from name -> type, contains all Tynames loaded ever */
static Htab *tidmap;		/* map from tid -> type */
static Htab *trmap;		/* map from trait id -> trait */
static Htab *autocallmap;	/* map from autocall name -> int */

#define Builtinmask (1 << 30)
static Typefix *typefix;	/* list of types we need to replace */
static size_t ntypefix;		/* size of replacement list */

static Traitfix *traitfix;	/* list of traits we need to replace */
static size_t ntraitfix;	/* size of replacement list */

static Node **implfix;		/* list of impls we need to fix up */
static size_t nimplfix;		/* size of replacement list */

void
addextlibs(char **libs, size_t nlibs)
{
	size_t i, j;

	for (i = 0; i < nlibs; i++) {
		for (j = 0; j < file.nextlibs; j++)
			if (!strcmp(file.extlibs[j], libs[i]))
				continue;
		lappend(&file.extlibs, &file.nextlibs, libs[i]);
	}
}

/* Outputs a symbol table to file in a way that can be
 * read back usefully. Only writes declarations, type
 * and sub-namespaces. Captured variables are ommitted. */
static void
wrstab(FILE *fd, Stab *val)
{
	size_t n, i;
	void **keys;

	wrstr(fd, val->name);

	/* write decls */
	keys = htkeys(val->dcl, &n);
	wrint(fd, n);
	for (i = 0; i < n; i++)
		wrsym(fd, getdcl(val, keys[i]));
	free(keys);

	/* write types */
	keys = htkeys(val->ty, &n);
	wrint(fd, n);
	for (i = 0; i < n; i++) {
		pickle(fd, keys[i]);			/* name */
		wrtype(fd, gettype(val, keys[i]));	/* type */
	}
	free(keys);
}

/* Reads a symbol table from file. The converse
 * of wrstab. */
static Stab *
rdstab(FILE *fd, int isfunc)
{
	Stab *st;
	Type *ty;
	Node *nm;
	int n;
	int i;

	/* read dcls */
	st = mkstab(isfunc);
	st->name = rdstr(fd);
	n = rdint(fd);
	for (i = 0; i < n; i++)
		putdcl(st, rdsym(fd, NULL));

	/* read types */
	n = rdint(fd);
	for (i = 0; i < n; i++) {
		nm = unpickle(fd);
		rdtype(fd, &ty);
		puttype(st, nm, ty);
	}
	return st;
}

static void
wrucon(FILE *fd, Ucon *uc)
{
	wrint(fd, uc->loc.line);
	wrint(fd, uc->id);
	wrbool(fd, uc->synth);
	pickle(fd, uc->name);
	wrbool(fd, uc->etype != NULL);
	if (uc->etype)
		wrtype(fd, uc->etype);
}

static Ucon *
rducon(FILE *fd, Type *ut)
{
	Type *et;
	Node *name;
	Ucon *uc;
	size_t id;
	int line;
	int synth;

	et = NULL;
	line = rdint(fd);
	id = rdint(fd);
	synth = rdbool(fd);
	name = unpickle(fd);
	uc = mkucon(Zloc, name, ut, et);
	uc->loc.line = line;
	uc->loc.file = file.nfiles - 1;
	if (rdbool(fd))
		rdtype(fd, &uc->etype);
	uc->id = id;
	uc->synth = synth;
	return uc;
}

/* Writes the name and type of a variable,
 * but only writes its intializer for thing
 * we want to inline cross-file (currently,
 * the only cross-file inline is generics) */
static void
wrsym(FILE *fd, Node *val)
{
	/* sym */
	wrint(fd, val->loc.line);
	pickle(fd, val->decl.name);
	wrtype(fd, val->decl.type);

	/* symflags */
	wrint(fd, val->decl.vis);
	wrbool(fd, val->decl.isconst);
	wrbool(fd, val->decl.isgeneric);
	wrbool(fd, val->decl.isextern);
	wrbool(fd, val->decl.ispkglocal);
	wrbool(fd, val->decl.isnoret);
	wrbool(fd, val->decl.isexportval);
	wrbyte(fd, val->decl.isinit);
	wrbyte(fd, val->decl.isfini);
	if (val->decl.isexportval)
		pickle(fd, val->decl.init);
}

static Node *
rdsym(FILE *fd, Trait *ctx)
{
	int line;
	Node *name;
	Node *n;

	line = rdint(fd);
	name = unpickle(fd);
	n = mkdecl(Zloc, name, NULL);
	n->loc.line = line;
	n->loc.file = file.nfiles - 1;
	rdtype(fd, &n->decl.type);

	if (rdint(fd) == Vishidden)
		n->decl.ishidden = 1;
	n->decl.trait = ctx;
	n->decl.isconst = rdbool(fd);
	n->decl.isgeneric = rdbool(fd);
	n->decl.isextern = rdbool(fd);
	n->decl.ispkglocal = rdbool(fd);
	n->decl.isnoret = rdbool(fd);
	n->decl.isimport = 1;
	n->decl.isexportval = rdbool(fd);
	n->decl.isinit = rdbool(fd);
	n->decl.isfini = rdbool(fd);
	if (n->decl.isexportval)
		n->decl.init = unpickle(fd);
	return n;
}

/* Writes types to a file. Errors on
 * internal only types like Tyvar that
 * will not be meaningful in another file*/
static void
typickle(FILE *fd, Type *ty)
{
	size_t i, j;

	if (!ty) {
		die("trying to pickle null type\n");
		return;
	}
	wrbyte(fd, ty->type);
	wrbyte(fd, ty->vis);
	wrint(fd, ty->nspec);
	for (i = 0; i < ty->nspec; i++) {
		wrint(fd, ty->spec[i]->ntrait);
		for (j = 0; j < ty->spec[i]->ntrait; j++)
			pickle(fd, ty->spec[i]->trait[j]);
		wrtype(fd, ty->spec[i]->param);
		wrbool(fd, ty->spec[i]->aux != NULL);
		if (ty->spec[i]->aux)
			wrtype(fd, ty->spec[i]->aux);
	}
	wrtype(fd, ty->seqaux);
	wrint(fd, ty->nsub);
	switch (ty->type) {
	case Tyunres:
		pickle(fd, ty->name);
		break;
	case Typaram:
		/* tid is generated; don't write */
		if (ty->trneed) {
			wrint(fd, bscount(ty->trneed));
			for (i = 0; bsiter(ty->trneed, &i); i++) {
				if (i < Ntraits)
					wrint(fd, i | Builtinmask);
				else
					wrint(fd, traittab[i]->uid);
			}
		} else {
			wrint(fd, 0);
		}
		wrstr(fd, ty->pname);	break;
	case Tystruct:
		wrint(fd, ty->nmemb);
		for (i = 0; i < ty->nmemb; i++)
			pickle(fd, ty->sdecls[i]);
		break;
	case Tyunion:
		wrint(fd, ty->nmemb);
		for (i = 0; i < ty->nmemb; i++)
			wrucon(fd, ty->udecls[i]);
		break;
	case Tyarray:
		wrtype(fd, ty->sub[0]);
		pickle(fd, ty->asize);
		break;
	case Tyslice:	wrtype(fd, ty->sub[0]);	break;
	case Tyvar:	die("Attempting to pickle %s. This will not work.\n", tystr(ty));	break;
	case Tyname:
		pickle(fd, ty->name);
		wrint(fd, ty->narg);
		for (i = 0; i < ty->narg; i++)
			wrtype(fd, ty->arg[i]);
		wrtype(fd, ty->sub[0]);
		break;
	case Tygeneric:
		pickle(fd, ty->name);
		wrint(fd, ty->ngparam);
		for (i = 0; i < ty->ngparam; i++)
			wrtype(fd, ty->gparam[i]);
		wrtype(fd, ty->sub[0]);
		break;
	default:
		for (i = 0; i < ty->nsub; i++)
			wrtype(fd, ty->sub[i]);
		break;
	}
}

static void
traitpickle(FILE *fd, Trait *tr)
{
	size_t i;

	wrint(fd, tr->uid);
	wrint(fd, tr->vis);
	pickle(fd, tr->name);
	typickle(fd, tr->param);
	wrint(fd, tr->naux);
	for (i = 0; i < tr->naux; i++)
		wrtype(fd, tr->aux[i]);
	wrint(fd, tr->nproto);
	for (i = 0; i < tr->nproto; i++)
		wrsym(fd, tr->proto[i]);
}

static void
wrtype(FILE *fd, Type *ty)
{
	if (!ty) {
		wrint(fd, 0);
		return;
	}
	if (ty->tid >= Builtinmask)
		die("Type id %d for %s too big", ty->tid, tystr(ty));
	if (ty->vis == Visbuiltin)
		wrint(fd, ty->type | Builtinmask);
	else
		wrint(fd, ty->tid);
}

static void
rdtype(FILE *fd, Type **dest)
{
	uintptr_t tid;

	tid = rdint(fd);
	if (tid == 0) {
		return;
	} else if (tid & Builtinmask) {
		*dest = mktype(Zloc, tid & ~Builtinmask);
	} else {
		typefix = xrealloc(typefix, (ntypefix + 1) * sizeof(typefix[0]));
		typefix[ntypefix++] = (Typefix){dest, tid};
	}
}

static void
rdtrait(FILE *fd, Trait **dest, Type *ty)
{
	uintptr_t tid;

	tid = rdint(fd);
	if (tid & Builtinmask) {
		if (dest)
			*dest = traittab[tid & ~Builtinmask];
		if (ty) {
			if (!ty->trneed)
				ty->trneed = mkbs();
			bsput(ty->trneed, traittab[tid & ~Builtinmask]->uid);
		}
	} else {
		traitfix = xrealloc(traitfix, (ntraitfix + 1) * sizeof(traitfix[0]));
		traitfix[ntraitfix++] = (Traitfix){dest, ty, tid};
	}
}

/* Reads types from a file. */
static Type *
tyunpickle(FILE *fd)
{
	size_t i, j, n;
	Type *ty;
	Ty t;

	t = rdbyte(fd);
	ty = mktype(Zloc, t);
	ty->isimport = 1;
	if (rdbyte(fd) == Vishidden)
		ty->ishidden = 1;
	ty->nspec = rdint(fd);
	ty->spec = malloc(ty->nspec * sizeof(Traitspec*));
	for (i = 0; i < ty->nspec; i++) {
		ty->spec[i] = zalloc(sizeof(Traitspec));
		n = rdint(fd);
		ty->spec[i]->ntrait = n;
		ty->spec[i]->trait = malloc(n * sizeof(Node*));
		for (j = 0; j < ty->spec[i]->ntrait; j++)
			ty->spec[i]->trait[j] = unpickle(fd);
		rdtype(fd, &ty->spec[i]->param);
		if (rdbool(fd))
			rdtype(fd, &ty->spec[i]->aux);
	}
	rdtype(fd, &ty->seqaux);
	ty->nsub = rdint(fd);
	if (ty->nsub > 0)
		ty->sub = zalloc(ty->nsub * sizeof(Type *));
	switch (ty->type) {
	case Tyunres:
		ty->name = unpickle(fd);
		break;
	case Typaram:	
		n = rdint(fd);
		for (i = 0; i < n; i++)
			rdtrait(fd, NULL, ty);
		ty->pname = rdstr(fd);
		break;
	case Tystruct:
		ty->nmemb = rdint(fd);
		ty->sdecls = zalloc(ty->nmemb * sizeof(Node *));
		for (i = 0; i < ty->nmemb; i++)
			ty->sdecls[i] = unpickle(fd);
		break;
	case Tyunion:
		ty->nmemb = rdint(fd);
		ty->udecls = zalloc(ty->nmemb * sizeof(Node *));
		for (i = 0; i < ty->nmemb; i++)
			ty->udecls[i] = rducon(fd, ty);
		break;
	case Tyarray:
		rdtype(fd, &ty->sub[0]);
		ty->asize = unpickle(fd);
		break;
	case Tyslice:
		rdtype(fd, &ty->sub[0]);
		break;
	case Tyname:
		ty->name = unpickle(fd);
		ty->narg = rdint(fd);
		ty->arg = zalloc(ty->narg * sizeof(Type *));
		for (i = 0; i < ty->narg; i++)
			rdtype(fd, &ty->arg[i]);
		rdtype(fd, &ty->sub[0]);
		break;
	case Tygeneric:
		ty->name = unpickle(fd);
		ty->ngparam = rdint(fd);
		ty->gparam = zalloc(ty->ngparam * sizeof(Type *));
		for (i = 0; i < ty->ngparam; i++)
			rdtype(fd, &ty->gparam[i]);
		rdtype(fd, &ty->sub[0]);
		break;
	default:
		for (i = 0; i < ty->nsub; i++)
			rdtype(fd, &ty->sub[i]);
		break;
	}
	return ty;
}

Trait *
traitunpickle(FILE *fd)
{
	Trait *tr;
	Node *proto;
	size_t i, n;
	intptr_t uid;

	/* create an empty trait */
	tr = mktrait(Zloc, NULL, NULL,
		NULL, 0,
		NULL, 0,
		0);
	uid = rdint(fd);
	if (rdint(fd) == Vishidden)
		tr->ishidden = 1;
	tr->name = unpickle(fd);
	tr->param = tyunpickle(fd);
	tr->naux = rdint(fd);
	tr->aux = zalloc(tr->naux * sizeof(Type*));
	tr->isimport = 1;
	for (i = 0; i < tr->naux; i++)
		rdtype(fd, &tr->aux[i]);
	n = rdint(fd);
	for (i = 0; i < n; i++) {
		proto = rdsym(fd, tr);
		proto->decl.impls = mkht(tyhash, tyeq);
		lappend(&tr->proto, &tr->nproto, proto);
	}
	htput(trmap, itop(uid), tr);
	return tr;
}

/* Pickles a node to a file.  The format
 * is more or less equivalen to to
 * simplest serialization of the
 * in-memory representation. Minimal
 * checking is done, so a bad type can
 * crash the compiler */
static void
pickle(FILE *fd, Node *n)
{
	size_t i;

	if (!n) {
		wrbyte(fd, Nnone);
		return;
	}
	wrbyte(fd, n->type);
	wrint(fd, n->loc.line);
	switch (n->type) {
	case Nexpr:
		wrbyte(fd, n->expr.op);
		wrtype(fd, n->expr.type);
		wrbool(fd, n->expr.param != NULL);
		if (n->expr.param)
			wrtype(fd, n->expr.param);
		wrbool(fd, n->expr.isconst);
		pickle(fd, n->expr.idx);
		wrint(fd, n->expr.nargs);
		for (i = 0; i < n->expr.nargs; i++)
			pickle(fd, n->expr.args[i]);
		break;
	case Nname:
		wrbool(fd, n->name.ns != NULL);
		if (n->name.ns) {
			wrstr(fd, n->name.ns);
		}
		wrstr(fd, n->name.name);
		break;
	case Nuse:
		wrbool(fd, n->use.islocal);
		wrstr(fd, n->use.name);
		break;
	case Nlit:
		wrbyte(fd, n->lit.littype);
		wrtype(fd, n->lit.type);
		wrint(fd, n->lit.nelt);
		switch (n->lit.littype) {
		case Lvoid:	break;
		case Lchr:	wrint(fd, n->lit.chrval);	break;
		case Lint:	wrint(fd, n->lit.intval);	break;
		case Lflt:	wrflt(fd, n->lit.fltval);	break;
		case Lstr:	wrlenstr(fd, n->lit.strval);	break;
		case Llbl:	wrstr(fd, n->lit.lblval);	break;
		case Lbool:	wrbool(fd, n->lit.boolval);	break;
		case Lfunc:	pickle(fd, n->lit.fnval);	break;
		}
		break;
	case Nloopstmt:
		pickle(fd, n->loopstmt.init);
		pickle(fd, n->loopstmt.cond);
		pickle(fd, n->loopstmt.step);
		pickle(fd, n->loopstmt.body);
		break;
	case Niterstmt:
		pickle(fd, n->iterstmt.elt);
		pickle(fd, n->iterstmt.seq);
		pickle(fd, n->iterstmt.body);
		break;
	case Nmatchstmt:
		pickle(fd, n->matchstmt.val);
		wrint(fd, n->matchstmt.nmatches);
		for (i = 0; i < n->matchstmt.nmatches; i++)
			pickle(fd, n->matchstmt.matches[i]);
		break;
	case Nmatch:
		pickle(fd, n->match.pat);
		pickle(fd, n->match.block);
		break;
	case Nifstmt:
		pickle(fd, n->ifstmt.cond);
		pickle(fd, n->ifstmt.iftrue);
		pickle(fd, n->ifstmt.iffalse);
		break;
	case Nblock:
		wrstab(fd, n->block.scope);
		wrint(fd, n->block.nstmts);
		for (i = 0; i < n->block.nstmts; i++)
			pickle(fd, n->block.stmts[i]);
		break;
	case Ndecl:
		/* sym */
		pickle(fd, n->decl.name);
		wrtype(fd, n->decl.type);

		/* symflags */
		wrbool(fd, n->decl.isconst);
		wrbool(fd, n->decl.isgeneric);
		wrbool(fd, n->decl.isextern);
		wrbool(fd, n->decl.isnoret);
		wrbool(fd, n->decl.ispkglocal);

		/* init */
		pickle(fd, n->decl.init);
		break;
	case Nfunc:
		wrtype(fd, n->func.type);
		wrstab(fd, n->func.scope);
		wrint(fd, n->func.nargs);
		for (i = 0; i < n->func.nargs; i++)
			pickle(fd, n->func.args[i]);
		pickle(fd, n->func.body);
		break;
	case Nimpl:
		pickle(fd, n->impl.traitname);
		wrint(fd, n->impl.trait->uid);
		wrtype(fd, n->impl.type);
		wrint(fd, n->impl.naux);
		for (i = 0; i < n->impl.naux; i++)
			wrtype(fd, n->impl.aux[i]);
		wrint(fd, n->impl.ndecls);
		for (i = 0; i < n->impl.ndecls; i++)
			wrsym(fd, n->impl.decls[i]);
		break;
	case Nnone:	die("Nnone should not be seen as node type!");	break;
	}
}

/* Unpickles a node from a file. Minimal checking
 * is done. Specifically, no checks are done for
 * sane arities, a bad file can crash the compiler */
static Node *
unpickle(FILE *fd)
{
	size_t i;
	Ntype type;
	Node *n;

	type = rdbyte(fd);
	if (type == Nnone)
		return NULL;
	n = mknode(Zloc, type);
	n->loc.line = rdint(fd);
	n->loc.file = file.nfiles - 1;
	switch (n->type) {
	case Nexpr:
		n->expr.op = rdbyte(fd);
		rdtype(fd, &n->expr.type);
		if (rdbool(fd))
			rdtype(fd, &n->expr.param);
		n->expr.isconst = rdbool(fd);
		n->expr.idx = unpickle(fd);
		n->expr.nargs = rdint(fd);
		n->expr.args = zalloc(sizeof(Node *) * n->expr.nargs);
		for (i = 0; i < n->expr.nargs; i++)
			n->expr.args[i] = unpickle(fd);
		break;
	case Nname:
		if (rdbool(fd))
			n->name.ns = rdstr(fd);
		n->name.name = rdstr(fd);
		break;
	case Nuse:
		n->use.islocal = rdbool(fd);
		n->use.name = rdstr(fd);
		break;
	case Nlit:
		n->lit.littype = rdbyte(fd);
		rdtype(fd, &n->lit.type);
		n->lit.nelt = rdint(fd);
		switch (n->lit.littype) {
		case Lvoid:	break;
		case Lchr:	n->lit.chrval = rdint(fd);	break;
		case Lint:	n->lit.intval = rdint(fd);	break;
		case Lflt:	n->lit.fltval = rdflt(fd);	break;
		case Lstr:	rdlenstr(fd, &n->lit.strval);	break;
		case Llbl:	n->lit.lblval = rdstr(fd);	break;
		case Lbool:	n->lit.boolval = rdbool(fd);	break;
		case Lfunc:	n->lit.fnval = unpickle(fd);	break;
		}
		break;
	case Nloopstmt:
		n->loopstmt.init = unpickle(fd);
		n->loopstmt.cond = unpickle(fd);
		n->loopstmt.step = unpickle(fd);
		n->loopstmt.body = unpickle(fd);
		break;
	case Niterstmt:
		n->iterstmt.elt = unpickle(fd);
		n->iterstmt.seq = unpickle(fd);
		n->iterstmt.body = unpickle(fd);
		break;
	case Nmatchstmt:
		n->matchstmt.val = unpickle(fd);
		n->matchstmt.nmatches = rdint(fd);
		n->matchstmt.matches = zalloc(sizeof(Node *) * n->matchstmt.nmatches);
		for (i = 0; i < n->matchstmt.nmatches; i++)
			n->matchstmt.matches[i] = unpickle(fd);
		break;
	case Nmatch:
		n->match.pat = unpickle(fd);
		n->match.block = unpickle(fd);
		break;
	case Nifstmt:
		n->ifstmt.cond = unpickle(fd);
		n->ifstmt.iftrue = unpickle(fd);
		n->ifstmt.iffalse = unpickle(fd);
		break;
	case Nblock:
		n->block.scope = rdstab(fd, 0);
		n->block.nstmts = rdint(fd);
		n->block.stmts = zalloc(sizeof(Node *) * n->block.nstmts);
		n->block.scope->super = curstab();
		pushstab(n->block.scope->super);
		for (i = 0; i < n->block.nstmts; i++)
			n->block.stmts[i] = unpickle(fd);
		popstab();
		break;
	case Ndecl:
		n->decl.did = ndecls; /* unique within file */
		/* sym */
		n->decl.name = unpickle(fd);
		rdtype(fd, &n->decl.type);

		/* symflags */
		n->decl.isconst = rdbool(fd);
		n->decl.isgeneric = rdbool(fd);
		n->decl.isextern = rdbool(fd);
		n->decl.isnoret = rdbool(fd);
		n->decl.ispkglocal = rdbool(fd);

		/* init */
		n->decl.init = unpickle(fd);
		lappend(&decls, &ndecls, n);
		break;
	case Nfunc:
		rdtype(fd, &n->func.type);
		n->func.scope = rdstab(fd, 1);
		n->func.nargs = rdint(fd);
		n->func.args = zalloc(sizeof(Node *) * n->func.nargs);
		n->func.scope->super = curstab();
		pushstab(n->func.scope->super);
		for (i = 0; i < n->func.nargs; i++)
			n->func.args[i] = unpickle(fd);
		n->func.body = unpickle(fd);
		popstab();
		break;
	case Nimpl:
		n->impl.traitname = unpickle(fd);
		rdtrait(fd, &n->impl.trait, NULL);
		rdtype(fd, &n->impl.type);
		n->impl.naux = rdint(fd);
		n->impl.aux = zalloc(sizeof(Node *) * n->impl.naux);
		for (i = 0; i < n->impl.naux; i++)
			rdtype(fd, &n->impl.aux[i]);
		n->impl.ndecls = rdint(fd);
		n->impl.decls = zalloc(sizeof(Node *) * n->impl.ndecls);
		for (i = 0; i < n->impl.ndecls; i++)
			n->impl.decls[i] = rdsym(fd, n->impl.trait);
		lappend(&impltab, &nimpltab, n);
		lappend(&implfix, &nimplfix, n);
		break;
	case Nnone:
		die("Nnone should not be seen as node type!");
		break;
	}
	return n;
}

static Stab *
findstab(Stab *st, char *pkg)
{
	Stab *s;

	if (!pkg) {
		if (!st->name)
			return st;
		else
			return NULL;
	}

	s = getns(pkg);
	if (!s) {
		s = mkstab(0);
		s->name = strdup(pkg);
		putns(s);
	}
	return s;
}

static int
isspecialization(Type *t1, Type *t2)
{
	if ((t1->type != Tygeneric || t2->type != Tyname) &&
			(t1->type != Tyname || t2->type != Tygeneric) &&
			(t1->type != Tyname || t2->type != Tyname))
		return 0;
	/* FIXME: this should be done better */
	return nameeq(t1->name, t2->name);
}

static void
fixtypemappings(Stab *st)
{
	size_t i;
	Type *t, *old;

	/*
	* merge duplicate definitions.
	* This allows us to compare named types by id, instead
	* of doing a deep walk through the type. This ability I
	* depend on when we do type inference.
	*/
	for (i = 0; i < ntypefix; i++) {
		t = htget(tidmap, itop(typefix[i].id));
		if (!t)
			die("Unable to find type for id %zd\n", typefix[i].id);
		*typefix[i].dest = t;
	}

	for (i = 0; i < ntypefix; i++) {
		old = *typefix[i].dest;
		if (old->type == Tyname || old->type == Tygeneric) {
			t = tydedup(old);
			*typefix[i].dest = t;
		}
	}

	/* check for duplicate type definitions */
	for (i = 0; i < ntypefix; i++) {
		t = htget(tidmap, itop(typefix[i].id));
		if ((t->type != Tyname && t->type != Tygeneric))
			continue;
		old = tydedup(t);
		if (!tyeq(t, old) && !isspecialization(t, old))
			lfatal(t->loc, "Duplicate definition of type %s on %s:%d",
				tystr(old), file.files[old->loc.file], old->loc.line);
	}
	lfree(&typefix, &ntypefix);
}

static void
fixtraitmappings(Stab *st)
{
	size_t i;
	Trait *t, *tr;

	/*
	* merge duplicate definitions.
	* This allows us to compare named types by id, instead
	* of doing a deep walk through the type. This ability i
	* depended on when we do type inference.
	*/
	for (i = 0; i < ntraitfix; i++) {
		t = htget(trmap, itop(traitfix[i].id));
		if (!t)
			die("Unable to find trait for id %zd\n", traitfix[i].id);

		tr = htget(trdeduptab, t->name);
		if (!tr) {
			htput(trdeduptab, t->name, t);
			tr = t;
		}
		if (traitfix[i].dest)
			*traitfix[i].dest = tr;
		if (traitfix[i].type && traitfix[i].type->type == Typaram) {
			if (!traitfix[i].type->trneed)
				traitfix[i].type->trneed = mkbs();
			bsput(traitfix[i].type->trneed, tr->uid);
		}
	}

	free(traitfix);
	traitfix = NULL;
	ntraitfix = 0;
}

static void
protomap(Trait *tr, Type *ty, Node *dcl)
{
	size_t i, len;
	char *protoname, *dclname, *p;
	Node *proto, *n;
	Stab *st;

	dclname = declname(dcl);
	for (i = 0; i < tr->nproto; i++) {
		n = tr->proto[i]->decl.name;
		st = file.globls;
		if (n->name.ns)
			st = getns(n->name.ns);
		proto = getdcl(st, n);
		if (!proto)
			proto = tr->proto[i];
		protoname = declname(proto);
		len = strlen(protoname);
		p = strstr(dclname, protoname);
		if (!p || p[len] != '$')
			continue;
		if (hthas(proto->decl.impls, ty))
			continue;
		htput(proto->decl.impls, ty, dcl);
		if (ty->type == Tygeneric || hasparams(ty)) {
			lappend(&proto->decl.gimpl, &proto->decl.ngimpl, dcl);
			lappend(&proto->decl.gtype, &proto->decl.ngtype, ty);
		}
	}
}

static void
fiximplmappings(Stab *st)
{
	Node *impl;
	Trait *tr;
	size_t i, j;

	for (i = 0; i < nimplfix; i++) {
		impl = implfix[i];
		tr = impl->impl.trait;

		/* FIXME: handle duplicate impls properly */
		if (getimpl(st, impl))
			continue;
		putimpl(st, impl);
		for (j = 0; j < impl->impl.ndecls; j++) {
			putdcl(file.globls, impl->impl.decls[j]);
			protomap(tr, impl->impl.type, impl->impl.decls[j]);
		}
	}
}

/* Usefile format:
 *     U<pkgname>
 *     T<pickled-type>
 *     R<picled-trait>
 *     I<pickled-impl>
 *     D<picled-decl>
 *     G<pickled-decl><pickled-initializer>
 */
int
loaduse(char *path, FILE *f, Stab *st, Vis vis)
{
	size_t startdecl, starttype, startimpl;
	Node *dcl, *impl, *fn;
	Stab *s, *ns;
	intptr_t tid;
	size_t i, j;
	char *pkg;
	Type *ty;
	Trait *tr;
	char *lib;
	int c, v;

	startdecl = ndecls;
	starttype = ntypes;
	startimpl = nimpltab;
	pushstab(file.globls);
	if (!trdeduptab)
		trdeduptab = mkht(namehash, nameeq);
	if (fgetc(f) != 'U')
		return 0;
	v = rdint(f);
	if (v != Abiversion) {
		fprintf(stderr, "%s: abi version %d, expected %d\n", path, v, Abiversion);
		return 0;
	}
	pkg = rdstr(f);
	/* if the package names match up, or the usefile has no declared
	* package, then we simply add to the current stab. Otherwise,
	* we add a new stab under the current one */
	if (st->name) {
		if (pkg && !strcmp(pkg, st->name)) {
			s = st;
		} else {
			s = findstab(st, pkg);
		}
	} else {
		if (pkg) {
			s = findstab(st, pkg);
		} else {
			s = st;
		}
	}
	if (!streq(st->name, pkg))
		vis = Visintern;
	if (!s) {
		printf("could not find matching package for merge: %s in %s\n", st->name, path);
		exit(1);
	}
	tidmap = mkht(ptrhash, ptreq);
	trmap = mkht(ptrhash, ptreq);
	if (!autocallmap)
		autocallmap = mkht(namehash, nameeq);
	/* builtin traits */
	for (i = 0; i < Ntraits; i++)
		htput(trmap, itop(i), traittab[i]);
	while ((c = fgetc(f)) != EOF) {
		switch (c) {
		case 'L':
			lib = rdstr(f);
			for (i = 0; i < file.nlibdeps; i++)
				if (!strcmp(file.libdeps[i], lib))
					/* break out of both loop and switch */
					goto foundlib;
			lappend(&file.libdeps, &file.nlibdeps, lib);
foundlib:
			break;
		case 'X':
			lib = rdstr(f);
			for (i = 0; i < file.nextlibs; i++)
				if (!strcmp(file.extlibs[i], lib))
					/* break out of both loop and switch */
					goto foundextlib;
			lappend(&file.extlibs, &file.nextlibs, lib);
foundextlib:
			break;
		case 'F': lappend(&file.files, &file.nfiles, rdstr(f)); break;
		case 'G':
		case 'D':
			dcl = rdsym(f, NULL);
			dcl->decl.vis = vis;
			dcl->decl.isglobl = 1;
			putdcl(s, dcl);
			break;
		case 'S':
		case 'E':
			fn = unpickle(f);
			if (hthas(autocallmap, fn))
				break;
			htput(autocallmap, fn, fn);
			if (c == 'S')
				lappend(&file.init, &file.ninit, fn);
			else
				lappend(&file.fini, &file.nfini, fn);
			break;
		case 'R':
			tr = traitunpickle(f);
			tr->vis = vis;
			puttrait(s, tr->name, tr);
			for (i = 0; i < tr->nproto; i++) {
				putdcl(s, tr->proto[i]);
				tr->proto[i]->decl.ishidden = tr->ishidden;
				if (tr->proto[i]->decl.env)
					tr->proto[i]->decl.env->super = tr->env;
			}
			break;
		case 'T':
			tid = rdint(f);
			ty = tyunpickle(f);
			if (!ty->ishidden)
				ty->vis = vis;
			htput(tidmap, itop(tid), ty);
			/* fix up types */
			if (ty->type == Tyname || ty->type == Tygeneric) {
				if (!streq(s->name, ty->name->name.ns))
					ty->ishidden = 1;
				if (!gettype(s, ty->name) && !ty->ishidden)
					puttype(s, ty->name, ty);
			} else if (ty->type == Tyunion) {
				for (i = 0; i < ty->nmemb; i++) {
					ns = findstab(s, ty->udecls[i]->name->name.ns);
					if (getucon(ns, ty->udecls[i]->name))
						continue;
					if (ty->udecls[i]->synth)
						continue;
					putucon(ns, ty->udecls[i]);
				}
			}
			break;
		case 'I':
			impl = unpickle(f);
			impl->impl.isextern = 1;
			impl->impl.vis = vis;
			/*
			 * Unfortunately, impls can insert their symbols into whatever
			 * namespace the trait comes from. This complicates things a bit.
			 */
			for (i = 0; i < impl->impl.ndecls; i++) {
				dcl = impl->impl.decls[i];
				dcl->decl.isglobl = 1;
				ns = file.globls;
				if (dcl->decl.name->name.ns)
					ns = findstab(s, dcl->decl.name->name.ns);
				putdcl(ns, dcl);
			}
			break;
		case EOF:
			break;
		}
	}
	fixtypemappings(s);
	fixtraitmappings(s);
	htfree(tidmap);
	for (i = starttype; i < ntypes; i++) {
		ty = types[i];
		if (ty->type == Tygeneric || ty->type == Tyname) {
			if (hasparams(ty) && !ty->env) {
				ty->env = mkenv();
				for (j = 0; j < ty->ngparam; j++)
					bindtype(ty->env, ty->gparam[j]);
				for (j = 0; j < ty->narg; j++)
					bindtype(ty->env, ty->arg[j]);
			}
			if (ty->sub[0]->env)
				ty->sub[0]->env->super = ty->env;
			else
				ty->sub[0]->env = ty->env;
		}
	}
	for (i = startdecl; i < ndecls; i++) {
		dcl = decls[i];
		if (hasparams(dcl->decl.type)) {
			dcl->decl.env = mkenv();
			bindtype(dcl->decl.env, dcl->decl.type);
		}
	}
	for (i = startimpl; i < nimpltab; i++) {
		impl = impltab[i];
		if (!impl->impl.env) {
			impl->impl.env = mkenv();
			bindtype(impl->impl.env, impl->impl.type);
		}
	}
	fiximplmappings(s);
	popstab();
	return 1;
}

int
hassuffix(char *str, char *suff)
{
	size_t nstr, nsuff;

	nstr = strlen(str);
	nsuff = strlen(suff);
	if (nstr < nsuff)
		return 0;
	return !strcmp(str + nstr - nsuff, suff);
}

void
readuse(Node *use, Stab *st, Vis vis)
{
	size_t i;
	FILE *fd;
	char *t, *p;
	char buf[512];

	/* local (quoted) uses are always relative to the output */
	fd = NULL;
	p = NULL;
	if (use->use.islocal) {
		fd = NULL;
		if (objdir) {
			snprintf(buf,sizeof buf, "%s/%s/%s.use", objdir, localincpath, use->use.name);
			p = strdup(buf);
			fd = fopen(p, "r");
		}
		if (!fd) {
			snprintf(buf,sizeof buf, "%s/%s.use", localincpath, use->use.name);
			p = strdup(buf);
			fd = fopen(p, "r");
		}
		if (!fd) {
			fprintf(stderr, "could not open usefile %s\n", buf);
			exit(1);
		}
	/* nonlocal (barename) uses are always searched on the include path */
	} else {
		for (i = 0; i < nincpaths; i++) {
			snprintf(buf, sizeof buf, "lib%s.use", use->use.name);
			t = strjoin(incpaths[i], "/");
			p = strjoin(t, buf);
			fd = fopen(p, "r");
			if (fd) {
				free(t);
				break;
			}
			free(p);

			t = strjoin(incpaths[i], "/");
			p = strjoin(t, use->use.name);
			fd = fopen(p, "r");
			if (fd) {
				free(t);
				break;
			}
			free(p);
		}
		if (!fd) {
			fprintf(stderr, "could not open usefile %s in search path:\n", use->use.name);
			for (i = 0; i < nincpaths; i++)
				fprintf(stderr, "\t%s\n", incpaths[i]);
			exit(1);
		}
	}

	if (!loaduse(p, fd, st, vis))
		fatal(use, "unable to load %s (full path: %s)", use->use.name, p);
	free(p);
}

void
loaduses(void)
{
	size_t i;

	for (i = 0; i < file.nuses; i++)
		readuse(file.uses[i], file.globls, Visintern);
}

/* Usefile format:
 * U<pkgname>
 * L<liblist>
 * I<initlist>
 * T<pickled-type>
 * D<picled-decl>
 * G<pickled-decl><pickled-initializer>
 * Z
 */
void
writeuse(FILE *f)
{
	Stab *st;
	void **k;
	Trait *tr;
	Node *s, *u;
	size_t i, n;

	st = file.globls;

	/* usefile name */
	wrbyte(f, 'U');
	wrint(f, Abiversion); /* use version */
	if (st->name)
		wrstr(f, st->name);
	else
		wrstr(f, NULL);

	/* library deps */
	for (i = 0; i < file.nuses; i++) {
		u = file.uses[i];
		if (!u->use.islocal) {
			wrbyte(f, 'L');
			wrstr(f, u->use.name);
		}
	}
	for (i = 0; i < file.nlibdeps; i++) {
		wrbyte(f, 'L');
		wrstr(f, file.libdeps[i]);
	}
	for (i = 0; i < file.nextlibs; i++) {
		wrbyte(f, 'X');
		wrstr(f, file.extlibs[i]);
	}

	/* source file name */
	wrbyte(f, 'F');
	wrstr(f, file.files[0]);

	for (i = 0; i < ntypes; i++) {
		if (types[i]->vis == Visexport || types[i]->vis == Vishidden) {
			wrbyte(f, 'T');
			wrint(f, types[i]->tid);
			typickle(f, types[i]);
		}
	}

	for (i = 0; i < ntraittab; i++) {
		tr = traittab[i];
		if (tr->uid < Ntraits)
			continue;
		if (tr->vis == Visexport || tr->vis == Vishidden) {
			wrbyte(f, 'R');
			traitpickle(f, tr);
		}
	}

	for (i = 0; i < nimpltab; i++) {
		/* merging during inference should remove all protos */
		s = impltab[i];
		assert(!s->impl.isproto);
		if (s->impl.vis == Visexport || s->impl.vis == Vishidden) {
			wrbyte(f, 'I');
			pickle(f, s);
		}
	}

	k = htkeys(st->dcl, &n);
	for (i = 0; i < n; i++) {
		s = getdcl(st, k[i]);
		assert(s != NULL);
		if (s->decl.vis == Visintern || s->decl.vis == Visbuiltin)
			continue;
		/* trait functions get written out with their traits */
		if (s->decl.trait || s->decl.isinit || s->decl.isfini)
			continue;
		else if (s->decl.isgeneric)
			wrbyte(f, 'G');
		else
			wrbyte(f, 'D');
		wrsym(f, s);
	}
	for (i = 0; i < file.ninit; i++) {
		wrbyte(f, 'S');
		pickle(f, file.init[i]);
	}
	if (file.localinit) {
		wrbyte(f, 'S');
		pickle(f, file.localinit->decl.name);
	}
	for (i = 0; i < file.nfini; i++) {
		wrbyte(f, 'E');
		pickle(f, file.fini[i]);
	}
	if (file.localfini) {
		wrbyte(f, 'E');
		pickle(f, file.localfini->decl.name);
	}
	free(k);
}
