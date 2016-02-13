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

void fillglobls(Stab *st, Htab *globls)
{
	size_t i, j, nk, nns;
	void **k, **ns;
	Stab *stab;
	Node *s;

	k = htkeys(st->dcl, &nk);
	for (i = 0; i < nk; i++) {
		s = htget(st->dcl, k[i]);
		if (isconstfn(s))
			s->decl.type = codetype(s->decl.type);
		htput(globls, s, asmname(s));
	}
	free(k);

	ns = htkeys(file->file.ns, &nns);
	for (j = 0; j < nns; j++) {
		stab = htget(file->file.ns, ns[j]);
		k = htkeys(stab->dcl, &nk);
		for (i = 0; i < nk; i++) {
			s = htget(stab->dcl, k[i]);
			htput(globls, s, asmname(s));
		}
		free(k);
	}
	free(ns);
}

Type *codetype(Type *ft)
{
	ft = tybase(ft);
	if (ft->type == Tycode)
		return ft;
	assert(ft->type == Tyfunc);
	ft = tydup(ft);
	ft->type = Tycode;
	return ft;
}

Type *closuretype(Type *ft)
{
	ft = tybase(ft);
	if (ft->type == Tyfunc)
		return ft;
	assert(ft->type == Tycode);
	ft = tydup(ft);
	ft->type = Tyfunc;
	return ft;
}

static int islocal(Node *dcl)
{
	if (dcl->decl.vis != Visintern)
		return 0;
	if (dcl->decl.isimport || dcl->decl.isextern)
		return 0;
	return 1;
}

char *genlocallblstr(char *buf, size_t sz)
{
	if (asmsyntax == Plan9)
		return genlblstr(buf, 128, "<>");
	else
		return genlblstr(buf, 128, "");
}

/* 
 * For x86, the assembly names are generated as follows:
 *      local symbols: .name
 *      un-namespaced symbols: <symprefix>name
 *      namespaced symbols: <symprefix>namespace$name
 *      local symbols on plan9 have the file-unique suffix '<>' appended
 */
char *asmname(Node *dcl)
{
	char buf[1024];
	char *vis, *pf, *ns, *name, *sep;
	Node *n;

	n = dcl->decl.name;
	pf = Symprefix;
	ns = n->name.ns;
	name = n->name.name;
	vis = "";
	sep = "";
	if (asmsyntax == Plan9)
		if (islocal(dcl))
			vis = "<>";
	if (!ns || !ns[0])
		ns = "";
	else
		sep = "$";
	if (name[0] == '.')
		pf = "";

	bprintf(buf, sizeof buf, "%s%s%s%s%s", pf, ns, sep, name, vis);
	return strdup(buf);
}

char *tydescid(char *buf, size_t bufsz, Type *ty)
{
	char *sep, *ns;
	char *p, *end;
	size_t i;

	sep = "";
	ns = "";
	p = buf;
	end = buf + bufsz;
	if (ty->type == Tyname) {
		if (ty->name->name.ns) {
			ns = ty->name->name.ns;
			sep = "$";
		}
		if (ty->vis == Visexport || ty->isimport)
			p += bprintf(p, end - p, "_tydesc$%s%s%s", ns, sep, ty->name->name.name);
		else
			p += bprintf(p, end - p, "_tydesc$%s%s%s$%d", ns, sep, ty->name->name.name, ty->tid);
		for (i = 0; i < ty->narg; i++)
			p += tyidfmt(p, end - p, ty->arg[i]);
	} else {
		if (file->file.globls->name) {
			ns = file->file.globls->name;
			sep = "$";
		}
		bprintf(buf, bufsz, "_tydesc%s%s$%d",sep, ns, ty->tid);
	}
	return buf;
}

void gen(Node *file, char *out)
{
	switch (asmsyntax) {
	case Plan9:	genp9(file, out);	break;
	case Gnugas:	gengas(file, out);	break;
	default:	die("unknown target");  break;
	}
}
