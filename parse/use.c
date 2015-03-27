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

static void wrtype(FILE *fd, Type *val);
static void rdtype(FILE *fd, Type **dest);
static void wrstab(FILE *fd, Stab *val);
static Stab *rdstab(FILE *fd);
static void wrsym(FILE *fd, Node *val);
static Node *rdsym(FILE *fd, Trait *ctx);
static void pickle(FILE *fd, Node *n);
static Node *unpickle(FILE *fd);

/* type fixup list */
static Htab *tydedup;   /* map from name -> type, contains all Tynames loaded ever */
static Htab *tidmap;    /* map from tid -> type */
static Htab *trmap;     /* map from trait id -> trait */

#define Builtinmask (1 << 30)
static Type ***typefixdest;     /* list of types we need to replace */
static size_t ntypefixdest;     /* size of replacement list */
static intptr_t *typefixid;     /* list of types we need to replace */
static size_t ntypefixid; /* size of replacement list */

static Trait ***traitfixdest;   /* list of traits we need to replace */
static size_t ntraitfixdest;    /* size of replacement list */
static Type **traitfixtype;     /* list of types we need to set the trait on */
static size_t ntraitfixtype;    /* size of replacement list */
static intptr_t *traitfixid;    /* list of traits we need to replace */
static size_t ntraitfixid; /* size of replacement list */


/* Outputs a symbol table to file in a way that can be
 * read back usefully. Only writes declarations, types
 * and sub-namespaces. Captured variables are ommitted. */
static void wrstab(FILE *fd, Stab *val)
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
        pickle(fd, keys[i]); /* name */
        wrtype(fd, gettype(val, keys[i])); /* type */
    }
    free(keys);

    /* write stabs */
    keys = htkeys(val->ns, &n);
    wrint(fd, n);
    for (i = 0; i < n; i++)
        wrstab(fd, getns(val, keys[i]));
    free(keys);
}

/* Reads a symbol table from file. The converse
 * of wrstab. */
static Stab *rdstab(FILE *fd)
{
    Stab *st;
    Type *ty;
    Node *nm;
    int n;
    int i;

    /* read dcls */
    st = mkstab();
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

    /* read stabs */
    n = rdint(fd);
    for (i = 0; i < n; i++)
        putns(st, rdstab(fd));

    return st;
}

static void wrucon(FILE *fd, Ucon *uc)
{
    wrint(fd, uc->loc.line);
    wrint(fd, uc->id);
    wrbool(fd, uc->synth);
    pickle(fd, uc->name);
    wrbool(fd, uc->etype != NULL);
    if (uc->etype)
      wrtype(fd, uc->etype);
}

static Ucon *rducon(FILE *fd, Type *ut)
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
    uc->loc.file = file->file.nfiles - 1;
    if (rdbool(fd))
      rdtype(fd, &uc->etype);
    uc->id = id;
    uc->synth = synth;
    return uc;
}

/* Writes the name and type of a variable,
 * but only writes its intializer for things
 * we want to inline cross-file (currently,
 * the only cross-file inline is generics) */
static void wrsym(FILE *fd, Node *val)
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
    wrbool(fd, val->decl.isexportinit);
    if (val->decl.isexportinit) {
        pickle(fd, val->decl.init);
    }
}

static Node *rdsym(FILE *fd, Trait *ctx)
{
    int line;
    Node *name;
    Node *n;

    line = rdint(fd);
    name = unpickle(fd);
    n = mkdecl(Zloc, name, NULL);
    n->loc.line = line;
    n->loc.file = file->file.nfiles - 1;
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
    n->decl.isexportinit = rdbool(fd);
    if (n->decl.isexportinit)
        n->decl.init = unpickle(fd);
    return n;
}

/* Writes types to a file. Errors on
 * internal only types like Tyvar that
 * will not be meaningful in another file*/
static void typickle(FILE *fd, Type *ty)
{
    size_t i;

    if (!ty) {
        die("trying to pickle null type\n");
        return;
    }
    wrbyte(fd, ty->type);
    wrbyte(fd, ty->vis);
    /* tid is generated; don't write */
    /* FIXME: since we only support hardcoded traits, we just write
     * out the set of them. we should write out the trait list as
     * well */
    if (!ty->traits) {
        wrint(fd, 0);
    } else {
        wrint(fd, bscount(ty->traits));
        for (i = 0; bsiter(ty->traits, &i); i++) {
            if (i < Ntraits)
                wrint(fd, i | Builtinmask);
            else
                wrint(fd, i);
        }
    }
    wrint(fd, ty->nsub);
    switch (ty->type) {
        case Tyunres:
            pickle(fd, ty->name);
            break;
        case Typaram:
            wrstr(fd, ty->pname);
            break;
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
        case Tyslice:
            wrtype(fd, ty->sub[0]);
            break;
        case Tyvar:
            die("Attempting to pickle %s. This will not work.\n", tystr(ty));
            break;
        case Tyname:
            pickle(fd, ty->name);
            wrbool(fd, ty->issynth);
            wrint(fd, ty->narg);
            for (i = 0; i < ty->narg; i++)
                wrtype(fd, ty->arg[i]);
            wrtype(fd, ty->sub[0]);
            break;
        case Tygeneric:
            pickle(fd, ty->name);
            wrbool(fd, ty->issynth);
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

static void traitpickle(FILE *fd, Trait *tr)
{
    size_t i;

    wrint(fd, tr->uid);
    wrbool(fd, tr->ishidden);
    pickle(fd, tr->name);
    typickle(fd, tr->param);
    wrint(fd, tr->nmemb);
    for (i = 0; i < tr->nmemb; i++)
        wrsym(fd, tr->memb[i]);
    wrint(fd, tr->nfuncs);
    for (i = 0; i < tr->nfuncs; i++)
        wrsym(fd, tr->funcs[i]);
}

static void wrtype(FILE *fd, Type *ty)
{
    if (ty->tid >= Builtinmask)
        die("Type id %d for %s too big", ty->tid, tystr(ty));
    if (ty->vis == Visbuiltin)
        wrint(fd, ty->type | Builtinmask);
    else
        wrint(fd, ty->tid);
}

static void rdtype(FILE *fd, Type **dest)
{
    uintptr_t tid;

    tid = rdint(fd);
    if (tid & Builtinmask) {
        *dest = mktype(Zloc, tid & ~Builtinmask);
    } else {
        lappend(&typefixdest, &ntypefixdest, dest);
        lappend(&typefixid, &ntypefixid, itop(tid));
    }
}

static void rdtrait(FILE *fd, Trait **dest, Type *ty)
{
    uintptr_t tid;

    tid = rdint(fd);
    if (tid & Builtinmask) {
        if (dest)
            *dest = traittab[tid & ~Builtinmask];
        if (ty)
            settrait(ty, traittab[tid & ~Builtinmask]);
    } else {
        lappend(&traitfixdest, &ntraitfixdest, dest);
        lappend(&traitfixtype, &ntraitfixtype, ty);
        lappend(&traitfixid, &ntraitfixid, itop(tid));
    }
}

/* Writes types to a file. Errors on
 * internal only types like Tyvar that
 * will not be meaningful in another file */
static Type *tyunpickle(FILE *fd)
{
    size_t i, n;
    Type *ty;
    Ty t;

    t = rdbyte(fd);
    ty = mktype(Zloc, t);
    ty->isimport = 1;
    if (rdbyte(fd) == Vishidden)
        ty->ishidden = 1;
    /* tid is generated; don't write */
    n = rdint(fd);
    for (i = 0; i < n; i++)
        rdtrait(fd, NULL, ty);
    ty->nsub = rdint(fd);
    if (ty->nsub > 0)
        ty->sub = zalloc(ty->nsub * sizeof(Type*));
    switch (ty->type) {
        case Tyunres:
            ty->name = unpickle(fd);
            break;
        case Typaram:
            ty->pname = rdstr(fd);
            break;
        case Tystruct:
            ty->nmemb = rdint(fd);
            ty->sdecls = zalloc(ty->nmemb * sizeof(Node*));
            for (i = 0; i < ty->nmemb; i++)
                ty->sdecls[i] = unpickle(fd);
            break;
        case Tyunion:
            ty->nmemb = rdint(fd);
            ty->udecls = zalloc(ty->nmemb * sizeof(Node*));
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
            ty->issynth = rdbool(fd);
            ty->narg = rdint(fd);
            ty->arg = zalloc(ty->narg * sizeof(Type *));
            for (i = 0; i < ty->narg; i++)
                rdtype(fd, &ty->arg[i]);
            rdtype(fd, &ty->sub[0]);
            break;
        case Tygeneric:
            ty->name = unpickle(fd);
            ty->issynth = rdbool(fd);
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

Trait *traitunpickle(FILE *fd)
{
    Trait *tr;
    size_t i, n;
    intptr_t uid;

    /* create an empty trait */
    tr = mktrait(Zloc, NULL, NULL, NULL, 0, NULL, 0, 0);
    uid = rdint(fd);
    tr->ishidden = rdbool(fd);
    tr->name = unpickle(fd);
    tr->param = tyunpickle(fd);
    n = rdint(fd);
    for (i = 0; i < n; i++)
        lappend(&tr->memb, &tr->nmemb, rdsym(fd, tr));
    n = rdint(fd);
    for (i = 0; i < n; i++)
        lappend(&tr->funcs, &tr->nfuncs, rdsym(fd, tr));
    htput(trmap, itop(uid), tr);
    return tr;
}

/* Pickles a node to a file.  The format
 * is more or less equivalen to to
 * simplest serialization of the
 * in-memory representation. Minimal
 * checking is done, so a bad type can
 * crash the compiler */
static void pickle(FILE *fd, Node *n)
{
    size_t i;

    if (!n) {
        wrbyte(fd, Nnone);
        return;
    }
    wrbyte(fd, n->type);
    wrint(fd, n->loc.line);
    switch (n->type) {
        case Nfile:
            wrstr(fd, n->file.files[0]);
            wrint(fd, n->file.nuses);
            for (i = 0; i < n->file.nuses; i++)
                pickle(fd, n->file.uses[i]);
            wrint(fd, n->file.nstmts);
            for (i = 0; i < n->file.nstmts; i++)
                pickle(fd, n->file.stmts[i]);
            wrstab(fd, n->file.globls);
            break;

        case Nexpr:
            wrbyte(fd, n->expr.op);
            wrtype(fd, n->expr.type);
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
                case Lchr:      wrint(fd, n->lit.chrval);       break;
                case Lint:      wrint(fd, n->lit.intval);       break;
                case Lflt:      wrflt(fd, n->lit.fltval);       break;
                case Lstr:      wrstrbuf(fd, n->lit.strval);    break;
                case Llbl:      wrstr(fd, n->lit.lblval);       break;
                case Lbool:     wrbool(fd, n->lit.boolval);     break;
                case Lfunc:     pickle(fd, n->lit.fnval);       break;
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
            wrint(fd, n->impl.ndecls);
            for (i = 0; i < n->impl.ndecls; i++)
                wrsym(fd, n->impl.decls[i]);
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
}

/* Unpickles a node from a file. Minimal checking
 * is done. Specifically, no checks are done for
 * sane arities, a bad file can crash the compiler */
static Node *unpickle(FILE *fd)
{
    size_t i;
    Ntype type;
    Node *n;

    type = rdbyte(fd);
    if (type == Nnone)
        return NULL;
    n = mknode(Zloc, type);
    n->loc.line = rdint(fd);
    n->loc.file = file->file.nfiles - 1;
    switch (n->type) {
        case Nfile:
            lappend(&n->file.files, &n->file.nfiles, rdstr(fd));
            n->file.nuses = rdint(fd);
            n->file.uses = zalloc(sizeof(Node*)*n->file.nuses);
            for (i = 0; i < n->file.nuses; i++)
                n->file.uses[i] = unpickle(fd);
            n->file.nstmts = rdint(fd);
            n->file.stmts = zalloc(sizeof(Node*)*n->file.nstmts);
            for (i = 0; i < n->file.nstmts; i++)
                n->file.stmts[i] = unpickle(fd);
            n->file.globls = rdstab(fd);
            break;

        case Nexpr:
            n->expr.op = rdbyte(fd);
            rdtype(fd, &n->expr.type);
            n->expr.isconst = rdbool(fd);
            n->expr.idx = unpickle(fd);
            n->expr.nargs = rdint(fd);
            n->expr.args = zalloc(sizeof(Node *)*n->expr.nargs);
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
                case Lchr:      n->lit.chrval = rdint(fd);       break;
                case Lint:      n->lit.intval = rdint(fd);       break;
                case Lflt:      n->lit.fltval = rdflt(fd);       break;
                case Lstr:      rdstrbuf(fd, &n->lit.strval);    break;
                case Llbl:      n->lit.lblval = rdstr(fd);       break;
                case Lbool:     n->lit.boolval = rdbool(fd);     break;
                case Lfunc:     n->lit.fnval = unpickle(fd);     break;
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
            n->matchstmt.matches = zalloc(sizeof(Node *)*n->matchstmt.nmatches);
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
            n->block.scope = rdstab(fd);
            n->block.nstmts = rdint(fd);
            n->block.stmts = zalloc(sizeof(Node *)*n->block.nstmts);
            n->block.scope->super = curstab();
            pushstab(n->func.scope->super);
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
            n->func.scope = rdstab(fd);
            n->func.nargs = rdint(fd);
            n->func.args = zalloc(sizeof(Node *)*n->func.nargs);
            n->func.scope->super = curstab();
            pushstab(n->func.scope->super);
            for (i = 0; i < n->func.nargs; i++)
                n->func.args[i] = unpickle(fd);
            n->func.body = unpickle(fd);
            popstab();
            break;
        case Nimpl:
            n->impl.traitname = unpickle(fd);
            i = rdint(fd);
            rdtrait(fd, &n->impl.trait, NULL);
            rdtype(fd, &n->impl.type);
            n->impl.ndecls = rdint(fd);
            n->impl.decls = zalloc(sizeof(Node *)*n->impl.ndecls);
            for (i = 0; i < n->impl.ndecls; i++)
                n->impl.decls[i] = rdsym(fd, n->impl.trait);
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
    return n;
}

static Stab *findstab(Stab *st, char *pkg)
{
    Stab *s;

    if (!pkg) {
        if (!st->name)
            return st;
        else
            return NULL;
    }

    s = getns_str(st, pkg);
    if (!s) {
        s = mkstab();
        s->name = strdup(pkg);
        putns(st, s);
    }
    return s;
}

static void fixtypemappings(Stab *st)
{
    size_t i;
    Type *t, *old;

    /*
     * merge duplicate definitions.
     * This allows us to compare named types by id, instead
     * of doing a deep walk through the type. This ability is
     * depended on when we do type inference.
     */
    for (i = 0; i < ntypefixdest; i++) {
        t = htget(tidmap, itop(typefixid[i]));
        if (!t)
            die("Unable to find type for id %zd\n", typefixid[i]);
        if ((t->type == Tyname || t->type == Tygeneric) && !t->issynth) {
            t = htget(tydedup, t);
        }
        *typefixdest[i] = t;
    }
    /* check for duplicate type definitions */
    for (i = 0; i < ntypefixdest; i++) {
        t = htget(tidmap, itop(typefixid[i]));
        if ((t->type != Tyname && t->type != Tygeneric) || t->issynth)
            continue;
        old = htget(tydedup, t);
        if (old && !tyeq(t, old))
            lfatal(t->loc, "Duplicate definition of type %s on %s:%d", tystr(old), file->file.files[old->loc.file], old->loc.line);
    }
    lfree(&typefixdest, &ntypefixdest);
    lfree(&typefixid, &ntypefixid);
}

static void fixtraitmappings(Stab *st)
{
    size_t i;
    Trait *t;

    /*
     * merge duplicate definitions.
     * This allows us to compare named types by id, instead
     * of doing a deep walk through the type. This ability is
     * depended on when we do type inference.
     */
    for (i = 0; i < ntraitfixdest; i++) {
        t = htget(trmap, itop(traitfixid[i]));
        if (!t)
            die("Unable to find trait for id %zd\n", traitfixid[i]);
        if (traitfixdest[i])
            *traitfixdest[i] = t;
        if (traitfixtype[i])
            settrait(traitfixtype[i], t);
    }

    lfree(&traitfixdest, &ntraitfixdest);
    lfree(&traitfixid, &ntraitfixid);
}

ulong tdhash(void *pt)
{
    Type *t;

    t = pt;
    return t->type * namehash(t->name);
}

int tdeq(void *pa, void *pb)
{
    Type *a, *b;

    a = pa;
    b = pb;
    return a->type == b->type && nameeq(a->name, b->name);
}

/* Usefile format:
 *     U<pkgname>
 *     T<pickled-type>
 *     R<picled-trait>
 *     I<pickled-impl>
 *     D<picled-decl>
 *     G<pickled-decl><pickled-initializer>
 */
int loaduse(FILE *f, Stab *st, Vis vis)
{
    intptr_t tid;
    size_t i;
    char *pkg;
    Node *dcl, *impl;
    Stab *s;
    Type *ty;
    Trait *tr;
    char *lib;
    int c;

    pushstab(file->file.globls);
    if (!tydedup)
        tydedup = mkht(tdhash, tdeq);
    if (fgetc(f) != 'U')
        return 0;
    if (rdint(f) != Abiversion) {
        fprintf(stderr, "usefile version mismatch. try rebuilding your deps.\n");
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
    if (!s) {
        printf("could not find matching package for merge: %s\n", st->name);
        exit(1);
    }
    tidmap = mkht(ptrhash, ptreq);
    trmap = mkht(ptrhash, ptreq);
    /* builtin traits */
    for (i = 0; i < Ntraits; i++)
        htput(trmap, itop(i), traittab[i]);
    while ((c = fgetc(f)) != EOF) {
        switch(c) {
            case 'L':
                lib = rdstr(f);
                for (i = 0; i < file->file.nlibdeps; i++)
                    if (!strcmp(file->file.libdeps[i], lib))
                        /* break out of both loop and switch */
                        goto foundlib;
                lappend(&file->file.libdeps, &file->file.nlibdeps, lib);
foundlib:
                break;
            case 'F':
                lappend(&file->file.files, &file->file.nfiles, rdstr(f));
                break;
            case 'G':
            case 'D':
                dcl = rdsym(f, NULL);
                dcl->decl.vis = vis;
                putdcl(s, dcl);
                break;
            case 'R':
                tr = traitunpickle(f);
                tr->vis = vis;
                puttrait(s, tr->name, tr);
                for (i = 0; i < tr->nfuncs; i++)
                    putdcl(s, tr->funcs[i]);
                break;
            case 'T':
                tid = rdint(f);
                ty = tyunpickle(f);
                if(!ty->ishidden)
                    ty->vis = vis;
                htput(tidmap, itop(tid), ty);
                /* fix up types */
                if (ty->type == Tyname || ty->type == Tygeneric) {
                    if (ty->issynth)
                        break;
                    if (!gettype(s, ty->name) && !ty->ishidden)
                        puttype(s, ty->name, ty);
                    if (!hthas(tydedup, ty))
                        htput(tydedup, ty, ty);
                } else if (ty->type == Tyunion)  {
                    for (i = 0; i < ty->nmemb; i++)
                        if (!getucon(s, ty->udecls[i]->name) && !ty->udecls[i]->synth)
                            putucon(s, ty->udecls[i]);
                }
                break;
            case 'I':
                impl = unpickle(f);
                putimpl(s, impl);
                /* specialized declarations always go into the global stab */
                for (i = 0; i < impl->impl.ndecls; i++)
                    putdcl(file->file.globls, impl->impl.decls[i]);
                break;
            case EOF:
                break;
        }
    }
    fixtypemappings(s);
    fixtraitmappings(s);
    htfree(tidmap);
    popstab();
    return 1;
}

void readuse(Node *use, Stab *st, Vis vis)
{
    size_t i;
    FILE *fd;
    char *p, *q;

    /* local (quoted) uses are always relative to the cwd */
    fd = NULL;
    if (use->use.islocal) {
        fd = fopen(use->use.name, "r");
    /* nonlocal (barename) uses are always searched on the include path */
    } else {
        for (i = 0; i < nincpaths; i++) {
            p = strjoin(incpaths[i], "/");
            q = strjoin(p, use->use.name);
            fd = fopen(q, "r");
            if (fd) {
                free(p);
                free(q);
                break;
            }
        }
    }
    if (!fd)
        fatal(use, "Could not open %s", use->use.name);

    if (!loaduse(fd, st, vis))
        die("Could not load usefile %s", use->use.name);
}

/* Usefile format:
 * U<pkgname>
 * T<pickled-type>
 * D<picled-decl>
 * G<pickled-decl><pickled-initializer>
 * Z
 */
void writeuse(FILE *f, Node *file)
{
    Stab *st;
    void **k;
    Node *s, *u;
    size_t i, n;

    assert(file->type == Nfile);
    st = file->file.globls;

    /* usefile name */
    wrbyte(f, 'U');
    wrint(f, Abiversion);        /* use version */
    if (st->name)
        wrstr(f, st->name);
    else
        wrstr(f, NULL);

    /* library deps */
    for (i = 0; i < file->file.nuses; i++) {
        u = file->file.uses[i];
        if (!u->use.islocal) {
            wrbyte(f, 'L');
            wrstr(f, u->use.name);
        }
    }
    for (i = 0; i < file->file.nlibdeps; i++) {
        wrbyte(f, 'L');
        wrstr(f, file->file.libdeps[i]);
    }

    /* source file name */
    wrbyte(f, 'F');
    wrstr(f, file->file.files[0]);

    for (i = 0; i < ntypes; i++) {
        if (types[i]->vis == Visexport || types[i]->vis == Vishidden) {
            wrbyte(f, 'T');
            wrint(f, types[i]->tid);
            typickle(f, types[i]);
        }
    }

    for (i = 0; i < ntraittab; i++) {
        if (traittab[i]->vis == Visexport || traittab[i]->vis == Vishidden) {
            wrbyte(f, 'R');
            traitpickle(f, traittab[i]);
        }
    }

    k = htkeys(st->impl, &n);
    for (i = 0; i < n; i++) {
        /* merging during inference should remove all protos */
        s = getimpl(st, k[i]);
        assert(!s->impl.isproto);
        if (s->impl.vis == Visexport || s->impl.vis == Vishidden) {
            wrbyte(f, 'I');
            pickle(f, s);
        }
    }
    free(k);

    k = htkeys(st->dcl, &n);
    for (i = 0; i < n; i++) {
        s = getdcl(st, k[i]);
        if (s->decl.vis == Visintern || s->decl.vis == Visbuiltin)
            continue;
        /* trait functions get written out with their traits */
        if (s->decl.trait)
            continue;
        if (s && s->decl.isgeneric)
            wrbyte(f, 'G');
        else
            wrbyte(f, 'D');
        wrsym(f, s);
    }
    free(k);
}
