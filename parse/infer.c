#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "parse.h"

typedef struct Inferstate Inferstate;
struct Inferstate {
    int ingeneric;
    int sawret;
    Type *ret;

    /* bound by patterns turn into decls in the action block */
    Node **binds;
    size_t nbinds;
    /* nodes that need post-inference checking/unification */
    Node **postcheck;
    size_t npostcheck;
    /* the type params bound at the current point */
    Htab **tybindings;
    size_t ntybindings;
    /* generic declarations to be specialized */
    Node **genericdecls;
    size_t ngenericdecls;
    /* delayed unification -- we fall back to these types in a post pass if we
     * haven't unifed to something more specific */
    Htab *delayed;
    /* the nodes that we've specialized them to, and the scopes they
     * appear in */
    Node **specializations;
    size_t nspecializations;
    Stab **specializationscope;
    size_t nspecializationscope;
};

static void infernode(Inferstate *st, Node *n, Type *ret, int *sawret);
static void inferexpr(Inferstate *st, Node *n, Type *ret, int *sawret);
static void typesub(Inferstate *st, Node *n);
static void tybind(Inferstate *st, Type *t);
static void bind(Inferstate *st, Node *n);
static void tyunbind(Inferstate *st, Type *t);
static void unbind(Inferstate *st, Node *n);
static Type *unify(Inferstate *st, Node *ctx, Type *a, Type *b);
static Type *tf(Inferstate *st, Type *t);

/* Tries to give a good string describing the context
 * for the sake of error messages. */
static char *ctxstr(Inferstate *st, Node *n)
{
    char *s;
    char *t;
    char *u;
    char *idx;
    char buf[512];

    idx = NULL;
    switch (n->type) {
        default:
            s = nodestr(n->type);
            break;
        case Ndecl:
            u = declname(n);
            t = tystr(tf(st, decltype(n)));
            snprintf(buf, sizeof buf, "%s:%s", u, t);
            s = strdup(buf);
            free(t);
            break;
        case Nname:
            s = namestr(n);
            break;
        case Nexpr:
            if (n->expr.idx)
                idx = ctxstr(st, n->expr.idx);
            if (exprop(n) == Ovar)
                u = namestr(n->expr.args[0]);
            else
                u = opstr(exprop(n));
            if (exprtype(n))
                t = tystr(tf(st, exprtype(n)));
            else
                t = strdup("unknown");
            if (idx)
                snprintf(buf, sizeof buf, ".%s=%s:%s", idx, u, t);
            else
                snprintf(buf, sizeof buf, "%s:%s", u, t);
            free(idx);
            free(t);
            s = strdup(buf);
            break;
    }
    return s;
}


static int nameeq(Node *a, Node *b)
{
    if (a == b)
        return 1;
    return streq(a->name.ns, b->name.ns) && streq(a->name.name, b->name.name);
}

/* Set a scope's enclosing scope up correctly.
 * We don't do this in the parser for some reason. */
static void setsuper(Stab *st, Stab *super)
{
    Stab *s;

    /* verify that we don't accidentally create loops */
    for (s = super; s; s = s->super)
        assert(s->super != st);
    st->super = super;
}

/* If the current environment binds a type,
 * we return true */
static int isbound(Inferstate *st, Type *t)
{
    ssize_t i;
    Type *p;

    for (i = st->ntybindings - 1; i >= 0; i--) {
        p = htget(st->tybindings[i], t->pname);
        if (p == t)
            return 1;
    }
    return 0;
}

/* Checks if a type that directly contains itself.
 * Recursive types that contain themselves through
 * pointers or slices are fine, but any other self-inclusion
 * would lead to a value of infinite size */
static int tyinfinite(Inferstate *st, Type *t, Type *sub)
{
    size_t i;

    assert(t != NULL);
    if (t == sub) /* FIXME: is this actually right? */
        return 1;
    /* if we're on the first iteration, the subtype is the type
     * itself. The assignment must come after the equality check
     * for obvious reasons. */
    if (!sub)
        sub = t;

    switch (sub->type) {
        case Tystruct:
            for (i = 0; i < sub->nmemb; i++)
                if (tyinfinite(st, t, decltype(sub->sdecls[i])))
                    return 1;
            break;
        case Tyunion:
            for (i = 0; i < sub->nmemb; i++) {
                if (sub->udecls[i]->etype && tyinfinite(st, t, sub->udecls[i]->etype))
                    return 1;
            }
            break;

        case Typtr:
        case Tyslice:
            return 0;
        default:
            for (i = 0; i < sub->nsub; i++)
                if (tyinfinite(st, t, sub->sub[i]))
                    return 1;
            break;
    }
    return 0;
}


static int isgeneric(Inferstate *st, Type *t)
{
    size_t i;

    switch (t->type) {
        case Typaram:   return 1;
        case Tyname:    return t->isgeneric;
        case Tystruct:
            for (i = 0; i < t->nmemb; i++)
                if (isgeneric(st, decltype(t->sdecls[i])))
                    return 1;
            break;
        case Tyunion:
            for (i = 0; i < t->nmemb; i++)
                if (t->udecls[i]->etype && isgeneric(st, t->udecls[i]->etype))
                    return 1;
            break;
        default:
            for (i = 0; i < t->nsub; i++)
                if (isgeneric(st, t->sub[i]))
                    return 1;
            break;
    }
    return 0;
}

/* Freshens the type of a declaration. */
static Type *tyfreshen(Inferstate *st, Type *t)
{
    Htab *ht;

    if (!isgeneric(st, t)) {
        if (debugopt['u'])
            printf("%s isn't generic: skipping freshen\n", tystr(t));
        return t;
    }

    if (debugopt['u'])
        printf("Freshen %s => ", tystr(t));
    tybind(st, t);
    ht = mkht(tyhash, tyeq);
    t = tyspecialize(t, ht);
    htfree(ht);
    tyunbind(st, t);
    if (debugopt['u'])
        printf("%s\n", tystr(t));

    return t;
}


/* Resolves a type and all it's subtypes recursively.*/
static void tyresolve(Inferstate *st, Type *t)
{
    size_t i;
    Type *base;

    if (t->resolved)
        return;
    t->resolved = 1;
    /* Walk through aggregate type members */
    if (t->type == Tystruct) {
        for (i = 0; i < t->nmemb; i++)
            infernode(st, t->sdecls[i], NULL, NULL);
    } else if (t->type == Tyunion) {
        for (i = 0; i < t->nmemb; i++) {
            t->udecls[i]->utype = t;
            t->udecls[i]->utype = tf(st, t->udecls[i]->utype);
            if (t->udecls[i]->etype) {
                tyresolve(st, t->udecls[i]->etype);
                t->udecls[i]->etype = tf(st, t->udecls[i]->etype);
            }
        }
    } else if (t->type == Tyarray) {
        infernode(st, t->asize, NULL, NULL);
    }

    for (i = 0; i < t->nsub; i++)
        t->sub[i] = tf(st, t->sub[i]);
    base = tybase(t);
    /* no-ops if base == t */
    if (t->cstrs)
        bsunion(t->cstrs, base->cstrs);
    else
        t->cstrs = bsdup(base->cstrs);
    if (tyinfinite(st, t, NULL))
        fatal(t->line, "Type %s includes itself", tystr(t));
}

/* Look up the best type to date in the unification table, returning it */
static Type *tysearch(Inferstate *st, Type *t)
{
    Type *lu;
    Stab *ns;

    assert(t != NULL);
    lu = NULL;
    while (1) {
        if (!tytab[t->tid] && t->type == Tyunres) {
            ns = curstab();
            if (t->name->name.ns) {
                ns = getns_str(ns, t->name->name.ns);
            }
            if (!ns)
                fatal(t->name->line, "Could not resolve namespace \"%s\"", t->name->name.ns);
            if (!(lu = gettype(ns, t->name)))
                fatal(t->name->line, "Could not resolve type %s", namestr(t->name));
            tytab[t->tid] = lu;
        }

        if (!tytab[t->tid])
            break;
        t = tytab[t->tid];
    }
    return t;
}

/* fixd the most accurate type mapping we have (ie,
 * the end of the unification chain */
static Type *tf(Inferstate *st, Type *orig)
{
    Type *t;
    size_t i;

    t = tysearch(st, orig);
    st->ingeneric += t->isgeneric;
    tyresolve(st, t);
    /* If this is an instantiation of a generic type, we want the params to
     * match the instantiation */
    if (orig->type == Tyunres && t->isgeneric) {
        t = tyfreshen(st, t);
        for (i = 0; i < t->narg; i++)
             unify(st, NULL, t->arg[i], orig->arg[i]);
    }
    st->ingeneric -= t->isgeneric;
    return t;
}

/* set the type of any typable node */
static void settype(Inferstate *st, Node *n, Type *t)
{
    t = tf(st, t);
    switch (n->type) {
        case Nexpr:     n->expr.type = t;       break;
        case Ndecl:     n->decl.type = t;       break;
        case Nlit:      n->lit.type = t;        break;
        case Nfunc:     n->func.type = t;       break;
        default:
            die("untypable node %s", nodestr(n->type));
            break;
    }
}

/* Gets the type of a literal value */
static Type *littype(Node *n)
{
    if (n->lit.type)
        return n->lit.type;
    switch (n->lit.littype) {
        case Lchr:      return mktype(n->line, Tychar);                         break;
        case Lbool:     return mktype(n->line, Tybool);                         break;
        case Lint:      return mktylike(n->line, Tyint);                        break;
        case Lflt:      return mktylike(n->line, Tyfloat64);                    break;
        case Lstr:      return mktyslice(n->line, mktype(n->line, Tybyte));     break;
        case Llbl:      return mktyptr(n->line, mktype(n->line, Tyvoid));       break;
        case Lfunc:     return n->lit.fnval->func.type;                         break;
    };
    die("Bad lit type %d", n->lit.littype);
    return NULL;
}

static Type *delayed(Inferstate *st, Type *fallback)
{
    Type *t;

    t = mktylike(fallback->line, fallback->type);
    htput(st->delayed, t, fallback);
    return t;
}

/* Finds the type of any typable node */
static Type *type(Inferstate *st, Node *n)
{
    Type *t;

    switch (n->type) {
      case Nlit:        t = littype(n);         break;
      case Nexpr:       t = n->expr.type;       break;
      case Ndecl:       t = decltype(n);        break;
      case Nfunc:       t = n->func.type;       break;
      default:
        t = NULL;
        die("untypeable node %s", nodestr(n->type));
        break;
    };
    return tf(st, t);
}

static Ucon *uconresolve(Inferstate *st, Node *n)
{
    Ucon *uc;
    Node **args;
    Stab *ns;

    args = n->expr.args;
    ns = curstab();
    if (args[0]->name.ns)
        ns = getns_str(ns, args[0]->name.ns);
    if (!ns)
        fatal(n->line, "No namespace %s\n", args[0]->name.ns);
    uc = getucon(ns, args[0]);
    if (!uc)
        fatal(n->line, "no union constructor `%s", ctxstr(st, args[0]));
    if (!uc->etype && n->expr.nargs > 1)
        fatal(n->line, "nullary union constructor `%s passed arg ", ctxstr(st, args[0]));
    else if (uc->etype && n->expr.nargs != 2)
        fatal(n->line, "union constructor `%s needs arg ", ctxstr(st, args[0]));
    return uc;
}

/* Binds the type parameters present in the
 * current type into the type environment */
static void putbindings(Inferstate *st, Htab *bt, Type *t)
{
    size_t i;

    if (!t)
        return;
    if (t->type != Typaram)
        return;

    if (hthas(bt, t->pname))
        unify(st, NULL, htget(bt, t->pname), t);
    else if (isbound(st, t))
        return;

    htput(bt, t->pname, t);
    for (i = 0; i < t->narg; i++)
        putbindings(st, bt, t->arg[i]);
}

static void tybind(Inferstate *st, Type *t)
{
    Htab *bt;

    if (t->type != Tyname && !t->isgeneric)
        return;
    bt = mkht(strhash, streq);
    lappend(&st->tybindings, &st->ntybindings, bt);
    putbindings(st, bt, t);
}

/* Binds the type parameters in the
 * declaration into the type environment */
static void bind(Inferstate *st, Node *n)
{
    Htab *bt;

    assert(n->type == Ndecl);
    if (!n->decl.isgeneric)
        return;
    if (!n->decl.init)
        fatal(n->line, "generic %s has no initializer", n->decl);

    st->ingeneric++;
    bt = mkht(strhash, streq);
    lappend(&st->tybindings, &st->ntybindings, bt);

    putbindings(st, bt, n->decl.type);
    putbindings(st, bt, n->decl.init->expr.type);
}

/* Rolls back the binding of type parameters in
 * the type environment */
static void unbind(Inferstate *st, Node *n)
{
    if (!n->decl.isgeneric)
        return;
    htfree(st->tybindings[st->ntybindings - 1]);
    lpop(&st->tybindings, &st->ntybindings);
    st->ingeneric--;
}

static void tyunbind(Inferstate *st, Type *t)
{
    if (t->type != Tyname && !t->isgeneric)
        return;
    htfree(st->tybindings[st->ntybindings - 1]);
    lpop(&st->tybindings, &st->ntybindings);
    st->ingeneric--;
}

/* Constrains a type to implement the required constraints. On
 * type variables, the constraint is added to the required
 * constraint list. Otherwise, the type is checked to see
 * if it has the required constraint */
static void constrain(Inferstate *st, Node *ctx, Type *a, Cstr *c)
{
    if (a->type == Tyvar) {
        if (!a->cstrs)
            a->cstrs = mkbs();
        setcstr(a, c);
    } else if (!a->cstrs || !bshas(a->cstrs, c->cid)) {
            fatal(ctx->line, "%s needs %s near %s", tystr(a), c->name, ctxstr(st, ctx));
    }
}

/* does b satisfy all the constraints of a? */
static int cstrcheck(Type *a, Type *b)
{
    /* a has no cstrs to satisfy */
    if (!a->cstrs)
        return 1;
    /* b satisfies no cstrs; only valid if a requires none */
    if (!b->cstrs)
        return bscount(a->cstrs) == 0;
    /* if a->cstrs is a subset of b->cstrs, all of
     * a's constraints are satisfied by b. */
    return bsissubset(a->cstrs, b->cstrs);
}

/* Merges the constraints on types */
static void mergecstrs(Inferstate *st, Node *ctx, Type *a, Type *b)
{
    if (b->type == Tyvar) {
        /* make sure that if a = b, both have same cstrs */
        if (a->cstrs && b->cstrs)
            bsunion(b->cstrs, a->cstrs);
        else if (a->cstrs)
            b->cstrs = bsdup(a->cstrs);
        else if (b->cstrs)
            a->cstrs = bsdup(b->cstrs);
    } else {
        if (!cstrcheck(a, b)) {
            /* FIXME: say WHICH constraints we're missing */
            fatal(ctx->line, "%s missing constraints for %s near %s", tystr(b), tystr(a), ctxstr(st, ctx));
        }
    }
}

/* Tells us if we have an index hack on the type */
static int idxhacked(Type *a, Type *b)
{
    return (a->type == Tyvar && a->nsub > 0) || a->type == Tyarray || a->type == Tyslice;
}

/* prevents types that contain themselves in the unification;
 * eg @a U (@a -> foo) */
static int occurs(Type *a, Type *b)
{
    size_t i;

    if (a == b)
        return 1;
    for (i = 0; i < b->nsub; i++)
        if (occurs(a, b->sub[i]))
            return 1;
    return 0;
}

/* Computes the 'rank' of the type; ie, in which
 * direction should we unify. A lower ranked type
 * should be mapped to the higher ranked (ie, more
 * specific) type. */
static int tyrank(Type *t)
{
    /* plain tyvar */
    if (t->type == Tyvar && t->nsub == 0)
        return 0;
    /* parameterized tyvar */
    if (t->type == Tyvar && t->nsub > 0)
        return 1;
    /* concrete type */
    return 2;
}

static int hasparam(Type *t)
{
    return t->type == Tyname && t->narg > 0;
}

/* Unifies two types, or errors if the types are not unifiable. */
static Type *unify(Inferstate *st, Node *ctx, Type *u, Type *v)
{
    Type *t, *r;
    Type *a, *b;
    char *from, *to;
    size_t i;

    /* a ==> b */
    a = tf(st, u);
    b = tf(st, v);
    if (a == b)
        return a;

    /* we unify from lower to higher ranked types */
    if (tyrank(b) < tyrank(a)) {
        t = a;
        a = b;
        b = t;
    }

    if (debugopt['u']) {
        from = tystr(a);
        to = tystr(b);
        printf("Unify %s => %s\n", from, to);
        free(from);
        free(to);
    }

    r = NULL;
    if (a->type == Tyvar) {
        tytab[a->tid] = b;
        r = b;
    }

    /* Disallow recursive types */
    if (a->type == Tyvar && b->type != Tyvar) 
        if (occurs(a, b))
            fatal(ctx->line, "Infinite type %s in %s near %s",
                  tystr(a), tystr(b), ctxstr(st, ctx));

    /* if the tyrank of a is 0 (ie, a raw tyvar), just unify.
     * Otherwise, match up subtypes. */
    if ((a->type == b->type || idxhacked(a, b)) && tyrank(a) != 0) {
        if (a->nsub != b->nsub)
            fatal(ctx->line, "%s has wrong subtype count for %s (got %d, expected %d) near %s",
                  tystr(a), tystr(b), a->nsub, b->nsub, ctxstr(st, ctx));
        for (i = 0; i < b->nsub; i++)
            unify(st, ctx, a->sub[i], b->sub[i]);
        r = b;
    } else if (hasparam(a) && hasparam(b)) {
        /* Only Tygeneric and Tyname should be able to unify. And they
         * should have the same names for this to be true. */
        if (!nameeq(a->name, b->name))
            fatal(ctx->line, "%s incompatible with %s near %s",
                  tystr(a), tystr(b), ctxstr(st, ctx));
        if (a->narg != b->narg)
            fatal(ctx->line, "%s has wrong parameter list for %s near %s",
                  tystr(a), tystr(b), ctxstr(st, ctx));
        for (i = 0; i < a->narg; i++)
            unify(st, ctx, a->arg[i], b->arg[i]);
    } else if (a->type != Tyvar) {
        fatal(ctx->line, "%s incompatible with %s near %s",
              tystr(a), tystr(b), ctxstr(st, ctx));
    }
    mergecstrs(st, ctx, a, b);

    if (hthas(st->delayed, u))
	u = htget(st->delayed, u);
    u = tybase(u);
    if (hthas(st->delayed, v))
	v = htget(st->delayed, v);
    v = tybase(v);
    if (u->type == Tyunion && v->type == Tyunion && u != v) {
	assert(u->nmemb = v->nmemb);
	for (i = 0; i < v->nmemb; i++) {
	    if (u->udecls[i]->etype)
		unify(st, NULL, u->udecls[i]->etype, v->udecls[i]->etype);
	}
    } else if (u->type == Tystruct && v->type == Tystruct && u != v) {
	assert(u->nmemb = v->nmemb);
	for (i = 0; i < v->nmemb; i++) {
	    unify(st, NULL, type(st, u->sdecls[i]), type(st, v->sdecls[i]));
	}
    }

    return r;
}

/* Applies unifications to function calls.
 * Funciton application requires a slightly
 * different approach to unification. */
static void unifycall(Inferstate *st, Node *n)
{
    size_t i;
    Type *ft;

    ft = type(st, n->expr.args[0]);
    if (ft->type == Tyvar) {
        /* the first arg is the function itself, so it shouldn't be counted */
        ft = mktyfunc(n->line, &n->expr.args[1], n->expr.nargs - 1, mktyvar(n->line));
        unify(st, n, ft, type(st, n->expr.args[0]));
    }
    for (i = 1; i < n->expr.nargs; i++) {
        if (i == ft->nsub)
            fatal(n->line, "%s arity mismatch (expected %zd args, got %zd)",
                  ctxstr(st, n->expr.args[0]), ft->nsub - 1, n->expr.nargs - 1);

        if (ft->sub[i]->type == Tyvalist)
            break;
        inferexpr(st, n->expr.args[i], NULL, NULL);
        unify(st, n->expr.args[0], ft->sub[i], type(st, n->expr.args[i]));
    }
    if (i < ft->nsub && ft->sub[i]->type != Tyvalist)
        fatal(n->line, "%s arity mismatch (expected %zd args, got %zd)",
              ctxstr(st, n->expr.args[0]), ft->nsub - 1, i - 1);
    settype(st, n, ft->sub[0]);
}

static void unifyparams(Inferstate *st, Node *ctx, Type *a, Type *b)
{
    size_t i;

    /* The only types with unifiable params are Tyunres and Tyname.
     * Tygeneric should always be freshened, and no other types have
     * parameters attached. 
     *
     * FIXME: Is it possible to have parameterized typarams? */
    if (a->type != Tyunres && a->type != Tyname)
        return;
    if (b->type != Tyunres && b->type != Tyname)
        return;

    if (a->narg != b->narg)
        fatal(ctx->line, "Mismatched parameter list sizes: %s with %s near %s", tystr(a), tystr(b), ctxstr(st, ctx));
    for (i = 0; i < a->narg; i++)
        unify(st, ctx, a->arg[i], b->arg[i]);
}

static void loaduses(Node *n)
{
    size_t i;

    /* uses only allowed at top level. Do we want to keep it this way? */
    for (i = 0; i < n->file.nuses; i++)
        readuse(n->file.uses[i], n->file.globls);
}

/* The exports in package declarations
 * need to be merged with the declarations
 * at the global scope. Declarations in
 * one may set the type of the other,
 * so this should be done early in the
 * process */
static void mergeexports(Inferstate *st, Node *file)
{
    Stab *exports, *globls;
    size_t i, nk;
    void **k;
    /* export, global version */
    Node *nx, *ng;
    Type *tx, *tg;
    Ucon *ux, *ug;

    exports = file->file.exports;
    globls = file->file.globls;

    pushstab(globls);
    k = htkeys(exports->ty, &nk);
    for (i = 0; i < nk; i++) {
        tx = gettype(exports, k[i]);
        nx = k[i];
        if (tx) {
            tg = gettype(globls, nx);
            if (!tg)
                puttype(globls, nx, tx);
            else
                fatal(nx->line, "Exported type %s already declared on line %d", namestr(nx), tg->line);
        } else {
            tg = gettype(globls, nx);
            if (tg)
                updatetype(exports, nx, tf(st, tg));
            else
                fatal(nx->line, "Exported type %s not declared", namestr(nx));
        }
    }
    free(k);

    k = htkeys(exports->dcl, &nk);
    for (i = 0; i < nk; i++) {
        nx = getdcl(exports, k[i]);
        ng = getdcl(globls, k[i]);
        /* if an export has an initializer, it shouldn't be declared in the
         * body */
        if (nx->decl.init && ng)
            fatal(nx->line, "Export %s double-defined on line %d", ctxstr(st, nx), ng->line);
        if (!ng)
            putdcl(globls, nx);
        else
            unify(st, nx, type(st, ng), type(st, nx));
    }
    free(k);


    k = htkeys(exports->uc, &nk);
    for (i = 0; i < nk; i++) {
        ux = getucon(exports, k[i]);
        ug = getucon(globls, k[i]);
        /* if an export has an initializer, it shouldn't be declared in the
         * body */
        if (ux && ug)
            fatal(ux->line, "Union constructor double defined on %d", ux->line);
        else if (!ug)
          putucon(globls, ux);
        else
            putucon(exports, ug);
    }
    free(k);

    popstab();
}

/* Finds out if the member reference is actually
 * referring to a namespaced name, instead of a struct
 * member. If it is, it transforms it into the variable
 * reference we should have, instead of the Omemb expr
 * that we do have */
static void checkns(Inferstate *st, Node *n, Node **ret)
{
    Node *var, *name, *nsname;
    Node **args;
    Stab *stab;
    Node *s;

    /* check that this is a namespaced declaration */
    if (n->type != Nexpr)
        return;
    if (!n->expr.nargs)
        return;
    args = n->expr.args;
    if (args[0]->type != Nexpr || exprop(args[0]) != Ovar)
        return;
    name = args[0]->expr.args[0];
    stab = getns(curstab(), name);
    if (!stab)
        return;

    /* substitute the namespaced name */
    nsname = mknsname(n->line, namestr(name), namestr(args[1]));
    s = getdcl(stab, args[1]);
    if (!s)
        fatal(n->line, "Undeclared var %s.%s", nsname->name.ns, nsname->name.name);
    var = mkexpr(n->line, Ovar, nsname, NULL);
    var->expr.did = s->decl.did;
    if (s->decl.isgeneric)
        settype(st, var, tyfreshen(st, s->decl.type));
    else
        settype(st, var, s->decl.type);
    if (s->decl.isgeneric && !st->ingeneric) {
        lappend(&st->specializationscope, &st->nspecializationscope, curstab());
        lappend(&st->specializations, &st->nspecializations, var);
        lappend(&st->genericdecls, &st->ngenericdecls, s);
    }
    *ret = var;
}

static void inferstruct(Inferstate *st, Node *n, int *isconst)
{
    size_t i;

    *isconst = 1;
    for (i = 0; i < n->expr.nargs; i++) {
        infernode(st, n->expr.args[i], NULL, NULL);
        if (!n->expr.args[i]->expr.isconst)
            *isconst = 0;
    }
    settype(st, n, mktyvar(n->line));
    lappend(&st->postcheck, &st->npostcheck, n);
}

static void inferarray(Inferstate *st, Node *n, int *isconst)
{
    size_t i;
    Type *t;
    Node *len;

    len = mkintlit(n->line, n->expr.nargs);
    t = mktyarray(n->line, mktyvar(n->line), len);
    for (i = 0; i < n->expr.nargs; i++) {
        infernode(st, n->expr.args[i], NULL, NULL);
        unify(st, n, t->sub[0], type(st, n->expr.args[i]));
        if (!n->expr.args[i]->expr.isconst)
            *isconst = 0;
    }
    settype(st, n, t);
}

static void infertuple(Inferstate *st, Node *n, int *isconst)
{
    Type **types;
    size_t i;

    types = xalloc(sizeof(Type *)*n->expr.nargs);
    for (i = 0; i < n->expr.nargs; i++) {
        n->expr.isconst = n->expr.isconst && n->expr.args[i]->expr.isconst;
        types[i] = type(st, n->expr.args[i]);
    }
    *isconst = n->expr.isconst;
    settype(st, n, mktytuple(n->line, types, n->expr.nargs));
}

static void inferucon(Inferstate *st, Node *n, int *isconst)
{
    Ucon *uc;
    Type *t;

    uc = uconresolve(st, n);
    t = tyfreshen(st, tf(st, uc->utype));
    uc = tybase(t)->udecls[uc->id];
    if (uc->etype)
        unify(st, n, uc->etype, type(st, n->expr.args[1]));
    *isconst = n->expr.args[0]->expr.isconst;
    settype(st, n, delayed(st, t));
}

static void inferpat(Inferstate *st, Node *n, Node *val, Node ***bind, size_t *nbind)
{
    size_t i;
    Node **args;
    Node *s;
    Type *t;

    args = n->expr.args;
    for (i = 0; i < n->expr.nargs; i++)
        if (args[i]->type == Nexpr)
            inferpat(st, args[i], val, bind, nbind);
    switch (exprop(n)) {
        case Oucon:
            inferucon(st, n, &n->expr.isconst);
            break;
        case Ovar:
            s = getdcl(curstab(), args[0]);
            if (s) {
                if (s->decl.isgeneric)
                    t = tyfreshen(st, s->decl.type);
                else if (s->decl.isconst)
                    t = s->decl.type;
                else
                    fatal(n->line, "Can't match against non-constant variables near %s", ctxstr(st, n));
            } else {
                t = mktyvar(n->line);
                s = mkdecl(n->line, n->expr.args[0], t);
                s->decl.init = val;
                settype(st, n, t);
                lappend(bind, nbind, s);
            }
            settype(st, n, t);
            n->expr.did = s->decl.did;
            break;
        case Olit:
            infernode(st, n, NULL, NULL);
            break;
        case Omemb:
            infernode(st, n, NULL, NULL);
            break;
        default:
            die("Bad pattern to match against");
            break;
    }
}

void addbindings(Inferstate *st, Node *n, Node **bind, size_t nbind)
{
    size_t i;

    /* order of binding shouldn't matter, so push them into the block
     * in reverse order. */
    for (i = 0; i < nbind; i++) {
        putdcl(n->block.scope, bind[i]);
        linsert(&n->block.stmts, &n->block.nstmts, 0, bind[i]);
    }
}

static void inferexpr(Inferstate *st, Node *n, Type *ret, int *sawret)
{
    Node **args;
    size_t i, nargs;
    Node *s;
    Type *t;

    assert(n->type == Nexpr);
    args = n->expr.args;
    nargs = n->expr.nargs;
    for (i = 0; i < nargs; i++) {
        /* Nlit, Nvar, etc should not be inferred as exprs */
        if (args[i]->type == Nexpr) {
            /* Omemb can sometimes resolve to a namespace. We have to check
             * this. Icky. */
            if (exprop(args[i]) == Omemb)
                checkns(st, args[i], &args[i]);
            inferexpr(st, args[i], ret, sawret);
        }
    }
    infernode(st, n->expr.idx, NULL, NULL);
    switch (exprop(n)) {
        /* all operands are same type */
        case Oadd:      /* @a + @a -> @a */
        case Osub:      /* @a - @a -> @a */
        case Omul:      /* @a * @a -> @a */
        case Odiv:      /* @a / @a -> @a */
        case Omod:      /* @a % @a -> @a */
        case Oneg:      /* -@a -> @a */
        case Obor:      /* @a | @a -> @a */
        case Oband:     /* @a & @a -> @a */
        case Obxor:     /* @a ^ @a -> @a */
        case Obsl:      /* @a << @a -> @a */
        case Obsr:      /* @a >> @a -> @a */
        case Obnot:     /* ~@a -> @a */
        case Opreinc:   /* ++@a -> @a */
        case Opredec:   /* --@a -> @a */
        case Opostinc:  /* @a++ -> @a */
        case Opostdec:  /* @a-- -> @a */
        case Oasn:      /* @a = @a -> @a */
        case Oaddeq:    /* @a += @a -> @a */
        case Osubeq:    /* @a -= @a -> @a */
        case Omuleq:    /* @a *= @a -> @a */
        case Odiveq:    /* @a /= @a -> @a */
        case Omodeq:    /* @a %= @a -> @a */
        case Oboreq:    /* @a |= @a -> @a */
        case Obandeq:   /* @a &= @a -> @a */
        case Obxoreq:   /* @a ^= @a -> @a */
        case Obsleq:    /* @a <<= @a -> @a */
        case Obsreq:    /* @a >>= @a -> @a */
            t = type(st, args[0]);
            for (i = 1; i < nargs; i++)
                t = unify(st, n, t, type(st, args[i]));
            settype(st, n, tf(st, t));
            break;

        /* operands same type, returning bool */
        case Olor:      /* @a || @b -> bool */
        case Oland:     /* @a && @b -> bool */
        case Olnot:     /* !@a -> bool */
        case Oeq:       /* @a == @a -> bool */
        case One:       /* @a != @a -> bool */
        case Ogt:       /* @a > @a -> bool */
        case Oge:       /* @a >= @a -> bool */
        case Olt:       /* @a < @a -> bool */
        case Ole:       /* @a <= @b -> bool */
            t = type(st, args[0]);
            for (i = 1; i < nargs; i++)
                unify(st, n, t, type(st, args[i]));
            settype(st, n, mktype(-1, Tybool));
            break;

        /* reach into a type and pull out subtypes */
        case Oaddr:     /* &@a -> @a* */
            settype(st, n, mktyptr(n->line, type(st, args[0])));
            break;
        case Oderef:    /* *@a* ->  @a */
            t = unify(st, n, type(st, args[0]), mktyptr(n->line, mktyvar(n->line)));
            settype(st, n, t->sub[0]);
            break;
        case Oidx:      /* @a[@b::tcint] -> @a */
            t = mktyidxhack(n->line, mktyvar(n->line));
            unify(st, n, type(st, args[0]), t);
            constrain(st, n, type(st, args[1]), cstrtab[Tcint]);
            settype(st, n, tf(st, t->sub[0]));
            break;
        case Oslice:    /* @a[@b::tcint,@b::tcint] -> @a[,] */
            t = mktyidxhack(n->line, mktyvar(n->line));
            unify(st, n, type(st, args[0]), t);
            constrain(st, n, type(st, args[1]), cstrtab[Tcint]);
            constrain(st, n, type(st, args[2]), cstrtab[Tcint]);
            settype(st, n, mktyslice(n->line, tf(st, t->sub[0])));
            break;

        /* special cases */
        case Omemb:     /* @a.Ident -> @b, verify type(@a.Ident)==@b later */
            settype(st, n, mktyvar(n->line));
            lappend(&st->postcheck, &st->npostcheck, n);
            break;
        case Osize:     /* sizeof @a -> size */
            settype(st, n, mktylike(n->line, Tyuint));
            break;
        case Ocall:     /* (@a, @b, @c, ... -> @r)(@a,@b,@c, ... -> @r) -> @r */
            unifycall(st, n);
            break;
        case Ocast:     /* cast(@a, @b) -> @b */
            lappend(&st->postcheck, &st->npostcheck, n);
            break;
        case Oret:      /* -> @a -> void */
            if (sawret)
                *sawret = 1;
            if (!ret)
                fatal(n->line, "Not allowed to return value here");
            if (nargs)
                t = unify(st, n, ret, type(st, args[0]));
            else
                t =  unify(st, n, mktype(-1, Tyvoid), ret);
            settype(st, n, t);
            break;
        case Ojmp:     /* goto void* -> void */
            settype(st, n, mktype(-1, Tyvoid));
            break;
        case Ovar:      /* a:@a -> @a */
            /* if we created this from a namespaced var, the type should be
             * set, and the normal lookup is expected to fail. Since we're
             * already done with this node, we can just return. */
            if (n->expr.type)
                return;
            s = getdcl(curstab(), args[0]);
            if (!s)
                fatal(n->line, "Undeclared var %s", ctxstr(st, args[0]));

            if (s->decl.isgeneric)
                t = tyfreshen(st, tf(st, s->decl.type));
            else
                t = s->decl.type;
            settype(st, n, t);
            n->expr.did = s->decl.did;
            if (s->decl.isgeneric && !st->ingeneric) {
                lappend(&st->specializationscope, &st->nspecializationscope, curstab());
                lappend(&st->specializations, &st->nspecializations, n);
                lappend(&st->genericdecls, &st->ngenericdecls, s);
            }
            break;
        case Oucon:
            inferucon(st, n, &n->expr.isconst);
            break;
        case Otup:
            infertuple(st, n, &n->expr.isconst);
            break;
        case Ostruct:
            inferstruct(st, n, &n->expr.isconst);
            break;
        case Oarr:
            inferarray(st, n, &n->expr.isconst);
            break;
        case Olit:      /* <lit>:@a::tyclass -> @a */
            switch (args[0]->lit.littype) {
                case Lfunc:
                    infernode(st, args[0]->lit.fnval, NULL, NULL); break;
                    /* FIXME: env capture means this is non-const */
                    n->expr.isconst = 1;
                default:
                    n->expr.isconst = 1;
                    break;
            }
            settype(st, n, type(st, args[0]));
            break;
        case Olbl:      /* :lbl -> void* */
            settype(st, n, mktyptr(n->line, mktype(-1, Tyvoid)));
        case Obad: case Ocjmp: case Oset:
        case Oslbase: case Osllen:
        case Oblit: case Numops:
        case Otrunc: case Oswiden: case Ozwiden:
        case Ouget:
            die("Should not see %s in fe", opstr(exprop(n)));
            break;
    }
}

static void inferfunc(Inferstate *st, Node *n)
{
    size_t i;
    int sawret;

    sawret = 0;
    for (i = 0; i < n->func.nargs; i++)
        infernode(st, n->func.args[i], NULL, NULL);
    infernode(st, n->func.body, n->func.type->sub[0], &sawret);
    /* if there's no return stmt in the function, assume void ret */
    if (!sawret)
        unify(st, n, type(st, n)->sub[0], mktype(-1, Tyvoid));
}

static void inferdecl(Inferstate *st, Node *n)
{
    Type *t;

    t = tf(st, decltype(n));
    if (t->type == Tyname && t->isgeneric && !n->decl.isgeneric) {
        t = tyfreshen(st, t);
        unifyparams(st, n, t, decltype(n));
    }
    settype(st, n, t);
    if (n->decl.init) {
        inferexpr(st, n->decl.init, NULL, NULL);
        unify(st, n, type(st, n), type(st, n->decl.init));
    } else {
        if (n->decl.isconst && !n->decl.isextern)
            fatal(n->line, "non-extern \"%s\" has no initializer", ctxstr(st, n));
    }
}

static void inferstab(Inferstate *st, Stab *s)
{
    void **k;
    size_t n, i;
    Type *t;

    k = htkeys(s->ty, &n);
    for (i = 0; i < n; i++) {
        t = tysearch(st, gettype(s, k[i]));
        updatetype(s, k[i], t);
    }
    free(k);
}

static void infernode(Inferstate *st, Node *n, Type *ret, int *sawret)
{
    size_t i;
    Node *d;
    Node *s;
    size_t nbound;
    Node **bound;

    if (!n)
        return;
    switch (n->type) {
        case Nfile:
            pushstab(n->file.globls);
            inferstab(st, n->file.globls);
            inferstab(st, n->file.exports);
            for (i = 0; i < n->file.nstmts; i++) {
                d  = n->file.stmts[i];
                infernode(st, d, NULL, sawret);
                /* exports allow us to specify types later in the body, so we
                 * need to patch the types in if they don't have a definition */
                if (d->type == Ndecl)  {
                    s = getdcl(file->file.exports, d->decl.name);
                    if (s) {
                        d->decl.isexport = 1;
                        s->decl.isexport = 1;
                        s->decl.init = d->decl.init;
                        unify(st, d, type(st, d), s->decl.type);
                    }
                }
            }
            popstab();
            break;
        case Ndecl:
            bind(st, n);
            inferdecl(st, n);
            unbind(st, n);
            if (type(st, n)->type == Typaram && !st->ingeneric)
                fatal(n->line, "Generic type %s in non-generic near %s\n", tystr(type(st, n)), ctxstr(st, n));
            break;
        case Nblock:
            setsuper(n->block.scope, curstab());
            pushstab(n->block.scope);
            inferstab(st, n->block.scope);
            for (i = 0; i < n->block.nstmts; i++) {
                checkns(st, n->block.stmts[i], &n->block.stmts[i]);
                infernode(st, n->block.stmts[i], ret, sawret);
            }
            popstab();
            break;
        case Nifstmt:
            infernode(st, n->ifstmt.cond, NULL, sawret);
            infernode(st, n->ifstmt.iftrue, ret, sawret);
            infernode(st, n->ifstmt.iffalse, ret, sawret);
            constrain(st, n, type(st, n->ifstmt.cond), cstrtab[Tctest]);
            break;
        case Nloopstmt:
            infernode(st, n->loopstmt.init, ret, sawret);
            infernode(st, n->loopstmt.cond, NULL, sawret);
            infernode(st, n->loopstmt.step, ret, sawret);
            infernode(st, n->loopstmt.body, ret, sawret);
            constrain(st, n, type(st, n->loopstmt.cond), cstrtab[Tctest]);
            break;
        case Nmatchstmt:
            infernode(st, n->matchstmt.val, NULL, sawret);
            for (i = 0; i < n->matchstmt.nmatches; i++) {
                infernode(st, n->matchstmt.matches[i], ret, sawret);
                unify(st, n, type(st, n->matchstmt.val), type(st, n->matchstmt.matches[i]->match.pat));
            }
            break;
        case Nmatch:
            bound = NULL;
            nbound = 0;
            inferpat(st, n->match.pat, NULL, &bound, &nbound);
            addbindings(st, n->match.block, bound, nbound);
            infernode(st, n->match.block, ret, sawret);
            break;
        case Nexpr:
            inferexpr(st, n, ret, sawret);
            break;
        case Nfunc:
            setsuper(n->func.scope, curstab());
            if (st->ntybindings > 0)
                for (i = 0; i < n->func.nargs; i++)
                    putbindings(st, st->tybindings[st->ntybindings - 1], n->func.args[i]->decl.type);
            pushstab(n->func.scope);
            inferstab(st, n->block.scope);
            inferfunc(st, n);
            popstab();
            break;
        case Nname:
        case Nlit:
        case Nuse:
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
}

/* returns the final type for t, after all unifications
 * and default constraint selections */
static Type *tyfix(Inferstate *st, Node *ctx, Type *t)
{
    static Type *tyint, *tyflt;
    size_t i;
    char buf[1024];

    if (!tyint)
        tyint = mktype(-1, Tyint);
    if (!tyflt)
        tyflt = mktype(-1, Tyfloat64);

    t = tysearch(st, t);
    if (hthas(st->delayed, t))
        t = htget(st->delayed, t);
    if (t->type == Tyvar) {
        if (hascstr(t, cstrtab[Tcint]) && cstrcheck(t, tyint))
            return tyint;
        if (hascstr(t, cstrtab[Tcfloat]) && cstrcheck(t, tyflt))
            return tyflt;
    } else if (!t->fixed) {
        t->fixed = 1;
        if (t->type == Tyarray) {
            typesub(st, t->asize);
        } else if (t->type == Tystruct) {
            for (i = 0; i < t->nmemb; i++)
                typesub(st, t->sdecls[i]);
        } else if (t->type == Tyunion) {
            for (i = 0; i < t->nmemb; i++) {
                if (t->udecls[i]->etype)
                    t->udecls[i]->etype = tyfix(st, ctx, t->udecls[i]->etype);
            }
        } else if (t->type == Tyname) {
            for (i = 0; i < t->narg; i++)
                t->arg[i] = tyfix(st, ctx, t->arg[i]);
        }
        for (i = 0; i < t->nsub; i++)
            t->sub[i] = tyfix(st, ctx, t->sub[i]);
    }
    if (t->type == Tyvar) {
        if (debugopt['T'])
            dump(file, stdout);
        fatal(t->line, "underconstrained type %s near %s", tyfmt(buf, 1024, t), ctxstr(st, ctx));
    }

    return t;
}

static void checkcast(Inferstate *st, Node *n)
{
    /* FIXME: actually verify the casts. Right now, it's ok to leave this
     * unimplemented because bad casts get caught by the backend. */
}

static void infercompn(Inferstate *st, Node *n)
{
    Node *aggr;
    Node *memb;
    Node **nl;
    Type *t;
    size_t i;
    int found;

    aggr = n->expr.args[0];
    memb = n->expr.args[1];

    found = 0;
    t = tybase(tf(st, type(st, aggr)));
    /* all array-like types have a fake "len" member that we emulate */
    if (t->type == Tyslice || t->type == Tyarray) {
        if (!strcmp(namestr(memb), "len")) {
            constrain(st, n, type(st, n), cstrtab[Tcnum]);
            constrain(st, n, type(st, n), cstrtab[Tcint]);
            constrain(st, n, type(st, n), cstrtab[Tctest]);
            found = 1;
        }
        /* otherwise, we search aggregate types for the member, and unify
         * the expression with the member type; ie:
         *
         *     x: aggrtype    y : memb in aggrtype
         *     ---------------------------------------
         *               x.y : membtype
         */
    } else {
        if (t->type == Typtr)
            t = tybase(tf(st, t->sub[0]));
        nl = t->sdecls;
        for (i = 0; i < t->nmemb; i++) {
            if (!strcmp(namestr(memb), declname(nl[i]))) {
                unify(st, n, type(st, n), decltype(nl[i]));
                found = 1;
                break;
            }
        }
    }
    if (!found)
        fatal(aggr->line, "Type %s has no member \"%s\" near %s",
              tystr(type(st, aggr)), ctxstr(st, memb), ctxstr(st, aggr));
}

static void checkstruct(Inferstate *st, Node *n)
{
    Type *t, *et;
    Node *val, *name;
    size_t i, j;

    t = tybase(tf(st, n->lit.type));
    if (t->type != Tystruct)
        fatal(n->line, "Type %s for struct literal is not struct near %s", tystr(t), ctxstr(st, n));

    for (i = 0; i < n->expr.nargs; i++) {
        val = n->expr.args[i];
        name = val->expr.idx;

        et = NULL;
        for (j = 0; j < t->nmemb; j++) {
            if (!strcmp(namestr(t->sdecls[i]->decl.name), namestr(name))) {
                et = type(st, t->sdecls[i]);
                break;
            }
        }

        if (!et)
            fatal(n->line, "Could not find member %s in struct %s, near %s",
                  namestr(name), tystr(t), ctxstr(st, n));

        unify(st, val, et, type(st, val));
    }
}

static void postcheck(Inferstate *st, Node *file)
{
    size_t i;
    Node *n;

    for (i = 0; i < st->npostcheck; i++) {
        n = st->postcheck[i];
        if (n->type == Nexpr && exprop(n) == Omemb)
            infercompn(st, n);
        else if (n->type == Nexpr && exprop(n) == Ocast)
            checkcast(st, n);
        else if (n->type == Nexpr && exprop(n) == Ostruct)
            checkstruct(st, n);
        else
            die("Thing we shouldn't be checking in postcheck\n");
    }
}

/* After inference, replace all
 * types in symbol tables with
 * the final computed types */
static void stabsub(Inferstate *st, Stab *s)
{
    void **k;
    size_t n, i;
    Type *t;
    Node *d;

    k = htkeys(s->ty, &n);
    for (i = 0; i < n; i++) {
        t = tysearch(st, gettype(s, k[i]));
        updatetype(s, k[i], t);
        tyfix(st, k[i], t);
    }
    free(k);

    k = htkeys(s->dcl, &n);
    for (i = 0; i < n; i++) {
        d = getdcl(s, k[i]);
        d->decl.type = tyfix(st, d, d->decl.type);
    }
    free(k);
}

/* After type inference, replace all types
 * with the final computed type */
static void typesub(Inferstate *st, Node *n)
{
    size_t i;

    if (!n)
        return;
    switch (n->type) {
        case Nfile:
            pushstab(n->file.globls);
            stabsub(st, n->file.globls);
            stabsub(st, n->file.exports);
            for (i = 0; i < n->file.nstmts; i++)
                typesub(st, n->file.stmts[i]);
            popstab();
            break;
        case Ndecl:
            settype(st, n, tyfix(st, n, type(st, n)));
            if (n->decl.init)
                typesub(st, n->decl.init);
            break;
        case Nblock:
            pushstab(n->block.scope);
            for (i = 0; i < n->block.nstmts; i++)
                typesub(st, n->block.stmts[i]);
            popstab();
            break;
        case Nifstmt:
            typesub(st, n->ifstmt.cond);
            typesub(st, n->ifstmt.iftrue);
            typesub(st, n->ifstmt.iffalse);
            break;
        case Nloopstmt:
            typesub(st, n->loopstmt.cond);
            typesub(st, n->loopstmt.init);
            typesub(st, n->loopstmt.step);
            typesub(st, n->loopstmt.body);
            break;
        case Nmatchstmt:
            typesub(st, n->matchstmt.val);
	    for (i = 0; i < n->matchstmt.nmatches; i++) {
                typesub(st, n->matchstmt.matches[i]);
	    }
            break;
        case Nmatch:
            typesub(st, n->match.pat);
            typesub(st, n->match.block);
            break;
        case Nexpr:
            settype(st, n, tyfix(st, n, type(st, n)));
            typesub(st, n->expr.idx);
            for (i = 0; i < n->expr.nargs; i++)
                typesub(st, n->expr.args[i]);
            break;
        case Nfunc:
            pushstab(n->func.scope);
            settype(st, n, tyfix(st, n, n->func.type));
            for (i = 0; i < n->func.nargs; i++)
                typesub(st, n->func.args[i]);
            typesub(st, n->func.body);
            popstab();
            break;
        case Nlit:
            settype(st, n, tyfix(st, n, type(st, n)));
            switch (n->lit.littype) {
                case Lfunc:
                    typesub(st, n->lit.fnval); break;
                default:        break;
            }
            break;
        case Nname:
        case Nuse:
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
}

/* Take generics and build new versions of them
 * with the type parameters replaced with the
 * specialized types */
static void specialize(Inferstate *st, Node *f)
{
    Node *d, *name;
    size_t i;

    for (i = 0; i < st->nspecializations; i++) {
        pushstab(st->specializationscope[i]);
        d = specializedcl(st->genericdecls[i], st->specializations[i]->expr.type, &name);
        st->specializations[i]->expr.args[0] = name;
        st->specializations[i]->expr.did = d->decl.did;
        popstab();
    }
}

void infer(Node *file)
{
    Inferstate st = {0,};

    assert(file->type == Nfile);
    st.delayed = mkht(tyhash, tyeq);
    loaduses(file);
    mergeexports(&st, file);
    infernode(&st, file, NULL, NULL);
    postcheck(&st, file);
    typesub(&st, file);
    specialize(&st, file);
}
