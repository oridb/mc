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

void tagreflect(Type *t)
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

static void taghidden(Type *t)
{
	size_t i;

	if (t->vis != Visintern)
		return;
	t->vis = Vishidden;
	for (i = 0; i < t->nsub; i++)
		taghidden(t->sub[i]);
	switch (t->type) {
	case Tystruct:
		for (i = 0; i < t->nmemb; i++)
			taghidden(decltype(t->sdecls[i]));
		break;
	case Tyunion:
		for (i = 0; i < t->nmemb; i++)
			if (t->udecls[i]->etype)
				taghidden(t->udecls[i]->etype);
		break;
	case Tyname:
		tagreflect(t);
		for (i = 0; i < t->narg; i++)
			taghidden(t->arg[i]);
	case Tygeneric:
		for (i = 0; i < t->ngparam; i++)
			taghidden(t->gparam[i]);
		break;
	default:
		break;
	}
}

int isexportinit(Node *n)
{
	if (n->decl.isgeneric && !n->decl.trait)
		return 1;
	/* we want to inline small values, which means we need to export them */
	if (istyprimitive(n->decl.type))
		return 1;
	return 0;
}

static void nodetag(Stab *st, Node *n, int ingeneric, int hidelocal)
{
	size_t i;
	Node *d;

	if (!n)
		return;
	switch (n->type) {
	case Nblock:
		for (i = 0; i < n->block.nstmts; i++)
			nodetag(st, n->block.stmts[i], ingeneric, hidelocal);
		break;
	case Nifstmt:
		nodetag(st, n->ifstmt.cond, ingeneric, hidelocal);
		nodetag(st, n->ifstmt.iftrue, ingeneric, hidelocal);
		nodetag(st, n->ifstmt.iffalse, ingeneric, hidelocal);
		break;
	case Nloopstmt:
		nodetag(st, n->loopstmt.init, ingeneric, hidelocal);
		nodetag(st, n->loopstmt.cond, ingeneric, hidelocal);
		nodetag(st, n->loopstmt.step, ingeneric, hidelocal);
		nodetag(st, n->loopstmt.body, ingeneric, hidelocal);
		break;
	case Niterstmt:
		nodetag(st, n->iterstmt.elt, ingeneric, hidelocal);
		nodetag(st, n->iterstmt.seq, ingeneric, hidelocal);
		nodetag(st, n->iterstmt.body, ingeneric, hidelocal);
		break;
	case Nmatchstmt:
		nodetag(st, n->matchstmt.val, ingeneric, hidelocal);
		for (i = 0; i < n->matchstmt.nmatches; i++)
			nodetag(st, n->matchstmt.matches[i], ingeneric, hidelocal);
		break;
	case Nmatch:
		nodetag(st, n->match.pat, ingeneric, hidelocal);
		nodetag(st, n->match.block, ingeneric, hidelocal);
		break;
	case Nexpr:
		nodetag(st, n->expr.idx, ingeneric, hidelocal);
		taghidden(n->expr.type);
		for (i = 0; i < n->expr.nargs; i++)
			nodetag(st, n->expr.args[i], ingeneric, hidelocal);
		/* generics need to have the decls they refer to exported. */
		if (ingeneric && exprop(n) == Ovar) {
			d = decls[n->expr.did];
			if (d->decl.isglobl && d->decl.vis == Visintern) {
				d->decl.vis = Vishidden;
				nodetag(st, d, ingeneric, hidelocal);
			}
		}
		break;
	case Nlit:
		taghidden(n->lit.type);
		if (n->lit.littype == Lfunc)
			nodetag(st, n->lit.fnval, ingeneric, hidelocal);
		break;
	case Ndecl:
		taghidden(n->decl.type);
		if (hidelocal && n->decl.ispkglocal)
			n->decl.vis = Vishidden;
		n->decl.isexportinit = isexportinit(n);
		if (n->decl.isexportinit)
			nodetag(st, n->decl.init, n->decl.isgeneric, hidelocal);
		break;
	case Nfunc:
		taghidden(n->func.type);
		for (i = 0; i < n->func.nargs; i++)
			nodetag(st, n->func.args[i], ingeneric, hidelocal);
		nodetag(st, n->func.body, ingeneric, hidelocal);
		break;
	case Nimpl:
		taghidden(n->impl.type);
		for (i = 0; i < n->impl.naux; i++)
			taghidden(n->impl.aux[i]);
		for (i = 0; i < n->impl.ndecls; i++) {
			n->impl.decls[i]->decl.vis = Vishidden;
			nodetag(st, n->impl.decls[i], 0, hidelocal);
		}
		break;
	case Nuse:
	case Nname:
		break;
	case Nfile:
	case Nnone:
		die("Invalid node for type export\n");
		break;
	}
}

void tagexports(Node *file, int hidelocal)
{
	size_t i, j, n;
	Trait *tr;
	Stab *st;
	void **k;
	Node *s;
	Type *t;

	st = file->file.globls;

	/* tag the initializers */
	for (i = 0; i < file->file.ninit; i++)
		nodetag(st, file->file.init[i], 0, hidelocal);
	if (file->file.localinit)
		nodetag(st, file->file.localinit, 0, hidelocal);

	/* tag the exported nodes */
	k = htkeys(st->dcl, &n);
	for (i = 0; i < n; i++) {
		s = getdcl(st, k[i]);
		if (s->decl.vis == Visexport)
			nodetag(st, s, 0, hidelocal);
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
		taghidden(t);
		for (j = 0; j < t->nsub; j++)
			taghidden(t->sub[j]);
		if (t->type == Tyname) {
			tagreflect(t);
			for (j = 0; j < t->narg; j++)
				taghidden(t->arg[j]);
		} else if (t->type == Tygeneric) {
			for (j = 0; j < t->ngparam; j++)
				taghidden(t->gparam[j]);
		}
	}

	/* tag the traits */
	free(k);
        tr = NULL;
	k = htkeys(st->tr, &n);
	for (j = 0; j < n; j++) {
		tr = gettrait(st, k[j]);
		if (tr->vis == Visexport) {
			tr->param->vis = Visexport;
			for (i = 0; i < tr->naux; i++)
				tr->aux[i]->vis = Visexport;
			for (i = 0; i < tr->nmemb; i++) {
				tr->memb[i]->decl.vis = Visexport;
				nodetag(st, tr->memb[i], 0, hidelocal);
			}
			for (i = 0; i < tr->nfuncs; i++) {
				tr->funcs[i]->decl.vis = Visexport;
				nodetag(st, tr->funcs[i], 0, hidelocal);
			}
		}
	}
	free(k);

	/* tag the impls */
	k = htkeys(st->impl, &n);
	for (i = 0; i < n; i++) {
		s = getimpl(st, k[i]);
		if (s->impl.vis != Visexport)
			continue;
		nodetag(st, s, 0, hidelocal);
                tr = s->impl.trait;
		for (j = 0; j < tr->naux; j++)
			tr->aux[j]->vis = Visexport;
	}
	free(k);

}

