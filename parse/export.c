#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "util.h"
#include "parse.h"

static void tagnode(Stab *st, Node *n, int ingeneric, int hidelocal);
static void tagtype(Stab *st, Type *t, int ingeneric, int hidelocal);

void
tagreflect(Type *t)
{
	size_t i;

	if (hasparams(t) || t->isreflect)
		return;

	t->isreflect = 1;
	for (i = 0; i < t->nsub; i++)
		tagreflect(t->sub[i]);
	switch (t->type) {
	case Tystruct:
		for (i = 0; i < t->nmemb; i++)
			tagreflect(decltype(t->sdecls[i]));
		break;
	case Tyunion:
		for (i = 0; i < t->nmemb; i++)
			if (t->udecls[i]->etype)
				tagreflect(t->udecls[i]->etype);
		break;
	case Tyname:
		for (i = 0; i < t->narg; i++)
			tagreflect(t->arg[i]);
	default:
		break;
	}
}

static void
tagtrait(Stab *st, Trait *tr, int ingeneric, int hidelocal)
{
	size_t i;

	if (!tr)
		return;
	if (tr->vis != Visintern)
		return;

	tr->vis = Vishidden;
	if (tr->param)
		tagtype(st, tr->param, ingeneric, hidelocal);
	for (i = 0; i < tr->naux; i++)
		tagtype(st, tr->aux[i], ingeneric, hidelocal);
	for (i = 0; i < tr->nproto; i++)
		tagnode(st, tr->proto[i], ingeneric, hidelocal);
}

static void
tagtype(Stab *st, Type *t, int ingeneric, int hidelocal)
{
	size_t i;

	if (!t || t->tagged)
		return;
	t->tagged = 1;
	t->vis = (t->vis == Visintern) ? Vishidden : t->vis;
	tagtype(st, t->seqaux, ingeneric, hidelocal);
	for (i = 0; i < t->nsub; i++)
		tagtype(st, t->sub[i], ingeneric, hidelocal);
	for (i = 0; i < t->nspec; i++) {
		tagtype(st, t->spec[i]->param, ingeneric, hidelocal);
		tagtype(st, t->spec[i]->aux, ingeneric, hidelocal);
	}
	switch (t->type) {
	case Tystruct:
		for (i = 0; i < t->nmemb; i++)
			tagtype(st, decltype(t->sdecls[i]), ingeneric, hidelocal);
		break;
	case Tyunion:
		for (i = 0; i < t->nmemb; i++)
			if (t->udecls[i]->etype)
				tagtype(st, t->udecls[i]->etype, ingeneric, hidelocal);
		break;
	case Tyname:
	case Tygeneric:
		tagreflect(t);
		for (i = 0; i < t->narg; i++)
			tagtype(st, t->arg[i], ingeneric, hidelocal);
		for (i = 0; i < t->ngparam; i++)
			tagtype(st, t->gparam[i], ingeneric, hidelocal);
		for (i = 0; i < t->narg; i++)
			tagtype(st, t->arg[i], ingeneric, hidelocal);
		break;
	case Typaram:
		if (t->trneed)
			for (i = 0; bsiter(t->trneed, &i); i++)
				tagtrait(st, traittab[i], ingeneric, hidelocal);
		break;
	default:
		break;
	}
}

int
isexportval(Node *n)
{
	if (n->decl.isgeneric && !n->decl.trait)
		return 1;
	/* we want to inline small values, which means we need to export them */
	if (istyprimitive(n->decl.type))
		return 1;
	return 0;
}

static void
tagnode(Stab *st, Node *n, int ingeneric, int hidelocal)
{
	size_t i;
	Node *d;

	if (!n)
		return;
	switch (n->type) {
	case Nblock:
		for (i = 0; i < n->block.nstmts; i++)
			tagnode(st, n->block.stmts[i], ingeneric, hidelocal);
		break;
	case Nifstmt:
		tagnode(st, n->ifstmt.cond, ingeneric, hidelocal);
		tagnode(st, n->ifstmt.iftrue, ingeneric, hidelocal);
		tagnode(st, n->ifstmt.iffalse, ingeneric, hidelocal);
		break;
	case Nloopstmt:
		tagnode(st, n->loopstmt.init, ingeneric, hidelocal);
		tagnode(st, n->loopstmt.cond, ingeneric, hidelocal);
		tagnode(st, n->loopstmt.step, ingeneric, hidelocal);
		tagnode(st, n->loopstmt.body, ingeneric, hidelocal);
		break;
	case Niterstmt:
		tagnode(st, n->iterstmt.elt, ingeneric, hidelocal);
		tagnode(st, n->iterstmt.seq, ingeneric, hidelocal);
		tagnode(st, n->iterstmt.body, ingeneric, hidelocal);
		break;
	case Nmatchstmt:
		tagnode(st, n->matchstmt.val, ingeneric, hidelocal);
		for (i = 0; i < n->matchstmt.nmatches; i++)
			tagnode(st, n->matchstmt.matches[i], ingeneric, hidelocal);
		break;
	case Nmatch:
		tagnode(st, n->match.pat, ingeneric, hidelocal);
		tagnode(st, n->match.block, ingeneric, hidelocal);
		break;
	case Nexpr:
		tagnode(st, n->expr.idx, ingeneric, hidelocal);
		tagtype(st, n->expr.type, ingeneric, hidelocal);
		if (n->expr.param)
			tagtype(st, n->expr.param, ingeneric, hidelocal);
		for (i = 0; i < n->expr.nargs; i++)
			tagnode(st, n->expr.args[i], ingeneric, hidelocal);
		/* generics need to have the decls they refer to exported. */
		if (ingeneric && exprop(n) == Ovar) {
			d = decls[n->expr.did];
			if (d->decl.isglobl && d->decl.vis == Visintern) {
				d->decl.vis = Vishidden;
				tagnode(st, d, ingeneric, hidelocal);
			}
		}
		break;
	case Nlit:
		tagtype(st, n->lit.type, ingeneric, hidelocal);
		if (n->lit.littype == Lfunc)
			tagnode(st, n->lit.fnval, ingeneric, hidelocal);
		break;
	case Ndecl:
		tagtype(st, n->decl.type, ingeneric, hidelocal);
		if (hidelocal && n->decl.ispkglocal)
			n->decl.vis = Vishidden;
		n->decl.isexportval = isexportval(n);
		if (n->decl.isexportval || ingeneric)
			tagnode(st, n->decl.init, n->decl.isgeneric, hidelocal);
		break;
	case Nfunc:
		tagtype(st, n->func.type, ingeneric, hidelocal);
		for (i = 0; i < n->func.nargs; i++)
			tagnode(st, n->func.args[i], ingeneric, hidelocal);
		tagnode(st, n->func.body, ingeneric, hidelocal);
		break;
	case Nimpl:
		tagtype(st, n->impl.type, ingeneric, hidelocal);
		for (i = 0; i < n->impl.naux; i++)
			tagtype(st, n->impl.aux[i], ingeneric, hidelocal);
		for (i = 0; i < n->impl.ndecls; i++) {
			n->impl.decls[i]->decl.vis = Vishidden;
			tagnode(st, n->impl.decls[i], 0, hidelocal);
		}
		break;
	case Nuse:
	case Nname:
		break;
	case Nnone:
		die("Invalid node for type export\n");
		break;
	}
}

void
tagexports(int hidelocal)
{
	size_t i, j, n;
	Trait *tr;
	Stab *st;
	void **k;
	Node *s;
	Type *t;

	st = file.globls;

	/* tag the initializers */
	for (i = 0; i < file.ninit; i++)
		tagnode(st, file.init[i], 0, hidelocal);
	for (i = 0; i < file.nfini; i++)
		tagnode(st, file.fini[i], 0, hidelocal);
	if (file.localinit)
		tagnode(st, file.localinit, 0, hidelocal);
	if (file.localfini)
		tagnode(st, file.localfini, 0, hidelocal);

	/* tag the exported nodes */
	k = htkeys(st->dcl, &n);
	for (i = 0; i < n; i++) {
		s = getdcl(st, k[i]);
		if (s->decl.vis == Visexport)
			tagnode(st, s, 0, hidelocal);
	}
	free(k);

	/* get the explicitly exported types */
	k = htkeys(st->ty, &n);
	for (i = 0; i < n; i++) {
		t = gettype(st, k[i]);
		if (!t->isreflect && t->vis != Visexport)
			continue;
		if (hidelocal && t->ispkglocal)
			t->vis = Vishidden;
		tagtype(st, t, 0, hidelocal);
		for (j = 0; j < t->nsub; j++)
			tagtype(st, t->sub[j], 0, hidelocal);
		if (t->type == Tyname) {
			tagreflect(t);
			for (j = 0; j < t->narg; j++)
				tagtype(st, t->arg[j], 0, hidelocal);
		} else if (t->type == Tygeneric) {
			for (j = 0; j < t->ngparam; j++)
				tagtype(st, t->gparam[j], 0, hidelocal);
		}
	}
	free(k);

	/* tag the traits */
	for (i = 0; i < ntraittab; i++) {
		tr = traittab[i];
		if (tr->vis != Visexport)
			continue;
		if (hidelocal && tr->ishidden)
			tr->vis = Vishidden;
		tagtype(st, tr->param, 0, hidelocal);
		tr->param->vis = tr->vis;
		for (j = 0; j < tr->naux; j++) {
			tagtype(st, tr->aux[j], 0, hidelocal);
			tr->aux[j]->vis = tr->vis;
		}
		for (j = 0; j < tr->nproto; j++) {
			tr->proto[j]->decl.vis = tr->vis;
			tagnode(st, tr->proto[j], 0, hidelocal);
		}
	}

	/* tag the impls */
	for (i = 0; i < nimpltab; i++) {
		s = impltab[i];
		if (s->impl.vis != Visexport)
			continue;
		tagnode(st, s, 0, hidelocal);
                tr = s->impl.trait;
		tagtrait(st, tr, 0, hidelocal);
		for (j = 0; j < tr->naux; j++)
			tr->aux[j]->vis = tr->vis;
	}
}
