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

/* outputs a fully qualified name */
static void
outname(Node *n, FILE *fd)
{
	if (n->name.ns)
		fprintf(fd, "%s.", n->name.ns);
	fprintf(fd, "%s", n->name.name);
}

/* outputs a sym in a one-line short form (ie,
 * the initializer is not printed, and the node is not
 * expressed in indented tree. */
static void
outsym(Node *s, FILE *fd, int depth)
{
	char buf[1024];

	if (s->decl.isconst)
		findentf(fd, depth, "const ");
	else
		findentf(fd, depth, "var ");
	outname(s->decl.name, fd);
	fprintf(fd, " : %s\n", tyfmt(buf, 1024, s->decl.type));
}

void
dumpsym(Node *s, FILE *fd) { outsym(s, fd, 0); }

/* Outputs a symbol table, and it's sub-table
 * recursively, with a sigil describing the symbol
 * type, as follows:
 *      T       type
 *      S       symbol
 *      N       namespace
 *
 * Does not print captured variables.
 */
static void
outstab(Stab *st, FILE *fd, int depth)
{
	size_t i, n;
	char *name;
	void **k;
	char *ty;
	Type *t;

	name = "";
	if (st->name)
		name = st->name;
	findentf(fd, depth, "Stab %p (super = %p, name=\"%s\")\n", st, st->super, name);
	if (!st)
		return;

	/* print types */
	k = htkeys(st->ty, &n);
	for (i = 0; i < n; i++) {
		findentf(fd, depth + 1, "T ");
		/* already indented */
		outname(k[i], fd);
		t = gettype(st, k[i]);
		if (t->nsub)
			ty = tystr(t->sub[0]);
		else
			ty = strdup("none");
		fprintf(fd, " = %s [tid=%d]\n", ty, t->tid);
		free(ty);
	}
	free(k);

	/* dump declarations */
	k = htkeys(st->dcl, &n);
	for (i = 0; i < n; i++) {
		findentf(fd, depth + 1, "S ");
		/* already indented */
		outsym(getdcl(st, k[i]), fd, 0);
	}
	free(k);

	/* dump closure */
	if (st->env) {
		k = htkeys(st->env, &n);
		for (i = 0; i < n; i++) {
			findentf(fd, depth + 1, "U ");
			/* already indented */
			outsym(getclosed(st, k[i]), fd, 0);
		}
		free(k);
	}
}

static void
outenv(Tyenv *e, FILE *fd, int depth)
{
	size_t n, i;
	void **k;
	Type *t;
	char *s;

	k = htkeys(e->tab, &n);
	for (i = 0; i < n; i++) {
		t = htget(e->tab, k[i]);
		s = tystr(t);
		findentf(fd, depth + 1, "B %s\n", s);
		free(s);
	}
	free(k);
}

void
dumpstab(Stab *st, FILE *fd)
{
	outstab(st, fd, 0);
}

void
dumpenv(Tyenv *e, FILE *fd)
{
	outenv(e, fd, 0);
}

void
dumpfilestabs(int depth, FILE *fd)
{
	size_t nk, i;
	void **k;

	k = htkeys(file.ns, &nk);
	for (i = 0; i < nk; i++)
		outstab(htget(file.ns, k[i]), fd, depth);
	free(k);
}


/* Outputs a node in indented tree form. This i
 * not a full serialization, but mainly an aid for
 * understanding and debugging. */
static void
outnode(Node *n, FILE *fd, int depth)
{
	size_t i;
	char *ty;
	char *tr;
	int tid;
	char buf[1024];

	if (!n) {
		findentf(fd, depth, "Nil\n");
		return;
	}
	findentf(fd, depth, "%s.%zd@%i", nodestr[n->type], n->nid, lnum(n->loc));
	switch (n->type) {
	case Ndecl:
		tr = "";
		if (n->decl.trait)
			tr = namestr(n->decl.trait->name);
		fprintf(fd, "(did = %zd, trait=%s, vis = %d)\n", n->decl.did, tr, n->decl.vis);
		findentf(fd, depth + 1, "isglobl=%d\n", n->decl.isglobl);
		findentf(fd, depth + 1, "isconst=%d\n", n->decl.isconst);
		findentf(fd, depth + 1, "isgeneric=%d\n", n->decl.isgeneric);
		findentf(fd, depth + 1, "isextern=%d\n", n->decl.isextern);
		findentf(fd, depth + 1, "ispkglocal=%d\n", n->decl.ispkglocal);
		findentf(fd, depth + 1, "ishidden=%d\n", n->decl.ishidden);
		findentf(fd, depth + 1, "isimport=%d\n", n->decl.isimport);
		findentf(fd, depth + 1, "isnoret=%d\n", n->decl.isnoret);
		findentf(fd, depth + 1, "isexportval=%d\n", n->decl.isexportval);
		findentf(fd, depth, ")\n");
		outsym(n, fd, depth + 1);
		outnode(n->decl.init, fd, depth + 1);
		break;
	case Nblock:
		fprintf(fd, "\n");
		outstab(n->block.scope, fd, depth + 1);
		for (i = 0; i < n->block.nstmts; i++)
			outnode(n->block.stmts[i], fd, depth + 1);
		break;
	case Nifstmt:
		fprintf(fd, "\n");
		outnode(n->ifstmt.cond, fd, depth + 1);
		outnode(n->ifstmt.iftrue, fd, depth + 1);
		outnode(n->ifstmt.iffalse, fd, depth + 1);
		break;
	case Nloopstmt:
		fprintf(fd, "\n");
		outnode(n->loopstmt.init, fd, depth + 1);
		outnode(n->loopstmt.cond, fd, depth + 1);
		outnode(n->loopstmt.step, fd, depth + 1);
		outnode(n->loopstmt.body, fd, depth + 1);
		break;
	case Niterstmt:
		fprintf(fd, "\n");
		outnode(n->iterstmt.elt, fd, depth + 1);
		outnode(n->iterstmt.seq, fd, depth + 1);
		outnode(n->iterstmt.body, fd, depth + 1);
		break;
	case Nmatchstmt:
		fprintf(fd, "\n");
		outnode(n->matchstmt.val, fd, depth + 1);
		for (i = 0; i < n->matchstmt.nmatches; i++)
			outnode(n->matchstmt.matches[i], fd, depth + 1);
		break;
	case Nmatch:
		fprintf(fd, "\n");
		outnode(n->match.pat, fd, depth + 1);
		outnode(n->match.block, fd, depth + 1);
		break;
	case Nuse:	fprintf(fd, " (name = %s, islocal = %d)\n", n->use.name, n->use.islocal);	break;
	case Nexpr:
		   if (exprop(n) == Ovar)
			   assert(decls[n->expr.did]->decl.did == n->expr.did);
		   ty = tystr(n->expr.type);
		   if (n->expr.type)
			   tid = n->expr.type->tid;
		   else
			   tid = -1;
		   fprintf(fd, " (type = %s [tid %d], op = %s, isconst = %d, did=%zd)\n", ty, tid,
				   opstr[n->expr.op], n->expr.isconst, n->expr.did);
		   free(ty);
		   outnode(n->expr.idx, fd, depth + 1);
		   for (i = 0; i < n->expr.nargs; i++)
			   outnode(n->expr.args[i], fd, depth + 1);
		   break;
	case Nlit:
		   switch (n->lit.littype) {
		   case Lvoid:	fprintf(fd, " Lvoid\n");	break;
		   case Lchr:	fprintf(fd, " Lchr %c\n", n->lit.chrval);	break;
		   case Lbool:	fprintf(fd, " Lbool %s\n", n->lit.boolval ? "true" : "false");	break;
		   case Lint:	fprintf(fd, " Lint %llu\n", n->lit.intval);	break;
		   case Lflt:	fprintf(fd, " Lflt %lf\n", n->lit.fltval);	break;
		   case Llbl:	fprintf(fd, " Llbl %s\n", n->lit.lblval);	break;
		   case Lstr:
				fprintf(fd, " Lstr %.*s\n", (int)n->lit.strval.len, n->lit.strval.buf);
				break;
		   case Lfunc:
				fprintf(fd, " lfunc\n");
				outnode(n->lit.fnval, fd, depth + 1);
				break;
		   }
		   break;
	case Nfunc:
		   fprintf(fd, " (args =\n");
		   for (i = 0; i < n->func.nargs; i++)
			   outnode(n->func.args[i], fd, depth + 1);
		   findentf(fd, depth, ")\n");
		   outstab(n->func.scope, fd, depth + 1);
		   outnode(n->func.body, fd, depth + 1);
		   break;
	case Nname:
		   fprintf(fd, "(");
		   if (n->name.ns)
			   fprintf(fd, "%s.", n->name.ns);
		   fprintf(fd, "%s", n->name.name);
		   fprintf(fd, ")\n");
		   break;
	case Nimpl:
		   fprintf(fd, "(name = %s, type = %s)\n", namestr(n->impl.traitname),
				   tyfmt(buf, sizeof buf, n->impl.type));
		   findentf(fd, depth, "");
		   outnode(n->impl.traitname, fd, depth + 1);
		   for (i = 0; i < n->impl.ndecls; i++)
			   outnode(n->impl.decls[i], fd, depth + 1);
		   break;
	case Nnone:
		   fprintf(stderr, "Nnone not a real node type!");
		   fprintf(fd, "Nnone\n");
		   break;
	}
}

void
dump(FILE *fd)
{
	size_t i;

	fprintf(fd, "(name = %s)\n", file.files[0]);
	dumpfilestabs(1, fd);
	for (i = 0; i < file.nuses; i++)
		outnode(file.uses[i], fd, 1);
	for (i = 0; i < file.nstmts; i++)
		outnode(file.stmts[i], fd, 1);
}

void
dumpn(Node *n, FILE *fd) {
	outnode(n, fd, 0);
}

