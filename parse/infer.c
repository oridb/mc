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

#include "parse.h"

typedef struct Inferstate Inferstate;
struct Inferstate {
    int ingeneric;
    int sawret;
    int indentdepth;
    int intype;
    Type *ret;

    /* bound by patterns turn into decls in the action block */
    Node **binds;
    size_t nbinds;
    /* nodes that need post-inference checking/unification */
    Node **postcheck;
    size_t npostcheck;
    Stab **postcheckscope;
    size_t npostcheckscope;
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

static void infernode(Inferstate *st, Node **np, Type *ret, int *sawret);
static void inferexpr(Inferstate *st, Node **np, Type *ret, int *sawret);
static void inferdecl(Inferstate *st, Node *n);
static void typesub(Inferstate *st, Node *n);
static void tybind(Inferstate *st, Type *t);
static Type *tyfix(Inferstate *st, Node *ctx, Type *orig, int noerr);
static void bind(Inferstate *st, Node *n);
static void tyunbind(Inferstate *st, Type *t);
static void unbind(Inferstate *st, Node *n);
static Type *unify(Inferstate *st, Node *ctx, Type *a, Type *b);
static Type *tf(Inferstate *st, Type *t);

static void ctxstrcall(char *buf, size_t sz, Inferstate *st, Node *n)
{
    char *p, *end, *sep, *t;
    size_t nargs, i;
    Node **args;
    Type *et;

    args = n->expr.args;
    nargs = n->expr.nargs;
    p = buf;
    end = buf + sz;
    sep = "";

    if (exprop(args[0]) == Ovar)
        p += snprintf(p, end - p, "%s(", namestr(args[0]->expr.args[0]));
    else
        p += snprintf(p, end - p, "<e>(");
    for (i = 1; i < nargs; i++) {
        et = tyfix(st, NULL, exprtype(args[i]), 1);
        if (et != NULL)
            t = tystr(et);
        else
            t = strdup("?");

        if (exprop(args[i]) == Ovar)
            p += snprintf(p, end - p, "%s%s:%s", sep, namestr(args[0]->expr.args[0]), t);
        else
            p += snprintf(p, end - p, "%s<e%zd>:%s", sep, i, t);
        sep = ", ";
        free(t);
    }
    t = tystr(tyfix(st, NULL, exprtype(args[0])->sub[0], 1));
    p += snprintf(p, end - p, "): %s", t);
    free(t);
}

static char *nodetystr(Inferstate *st, Node *n)
{
    Type *t;

    t = NULL;
    if (n->type == Nexpr && exprtype(n) != NULL)
        t = tyfix(st, NULL, exprtype(n), 1);
    else if (n->type == Ndecl && decltype(n) != NULL)
        t = tyfix(st, NULL, decltype(n), 1);

    if (t && tybase(t)->type != Tyvar)
        return tystr(t);
    else
        return strdup("unknown");
}

/* Tries to give a good string describing the context
 * for the sake of error messages. */
static char *ctxstr(Inferstate *st, Node *n)
{
    char *t, *t1, *t2, *t3;
    char *s, *d;
    size_t nargs;
    Node **args;
    char buf[512];

    switch (n->type) {
        default:
            s = strdup(nodestr[n->type]);
            break;
        case Ndecl:
            d = declname(n);
            t = nodetystr(st, n);
            snprintf(buf, sizeof buf, "%s:%s", d, t);
            s = strdup(buf);
            free(t);
            break;
        case Nname:
            s = strdup(namestr(n));
            break;
        case Nexpr:
            args = n->expr.args;
            nargs = n->expr.nargs;
            t1 = NULL;
            t2 = NULL;
            t3 = NULL;
            if (exprop(n) == Ovar)
                d = namestr(args[0]);
            else
                d = opstr[exprop(n)];
            t = nodetystr(st, n);
            if (nargs >= 1)
                t1 = nodetystr(st, args[0]);
            if (nargs >= 2)
                t2 = nodetystr(st, args[1]);
            if (nargs >= 3)
                t3 = nodetystr(st, args[2]);
            switch (opclass[exprop(n)]) {
                case OTbin:
                    snprintf(buf, sizeof buf, "<e1:%s> %s <e2:%s>", t1, oppretty[exprop(n)], t2);
                    break;
                case OTpre:
                    snprintf(buf, sizeof buf, "%s<e%s>", t1, oppretty[exprop(n)]);
                    break;
                case OTpost:
                    snprintf(buf, sizeof buf, "<e:%s>%s", t1, oppretty[exprop(n)]);
                    break;
                case OTzarg:
                    snprintf(buf, sizeof buf, "%s", oppretty[exprop(n)]);
                case OTmisc:
                    switch (exprop(n)) {
                        case Ovar:
                            snprintf(buf, sizeof buf, "%s:%s", namestr(args[0]), t);
                            break;
                        case Ocall:
                            ctxstrcall(buf, sizeof buf, st, n);
                            break;
                        case Oidx:
                            if (exprop(args[0]) == Ovar)
                                snprintf(buf, sizeof buf, "%s[<e1:%s>]", namestr(args[0]->expr.args[0]), t2);
                            else
                                snprintf(buf, sizeof buf, "<sl:%s>[<e1%s>]", t1, t2);
                            break;
                        case Oslice:
                            if (exprop(args[0]) == Ovar)
                                snprintf(buf, sizeof buf, "%s[<e1:%s>:<e2:%s>]", namestr(args[0]->expr.args[0]), t2, t3);
                            else
                                snprintf(buf, sizeof buf, "<sl:%s>[<e1%s>:<e2:%s>]", t1, t2, t3);
                            break;
                        case Omemb:
                            snprintf(buf, sizeof buf, "<%s>.%s", t1, namestr(args[1]));
                            break;
                        default:
                            snprintf(buf, sizeof buf, "%s:%s", d, t);
                            break;
                    }
            }
            free(t);
            free(t1);
            free(t2);
            free(t3);
            s = strdup(buf);
            break;
    }
    return s;
}

static void addspecialization(Inferstate *st, Node *n, Stab *stab)
{
    Node *dcl;

    dcl = decls[n->expr.did];
    lappend(&st->specializationscope, &st->nspecializationscope, stab);
    lappend(&st->specializations, &st->nspecializations, n);
    lappend(&st->genericdecls, &st->ngenericdecls, dcl);
}

static void delayedcheck(Inferstate *st, Node *n, Stab *s)
{
    lappend(&st->postcheck, &st->npostcheck, n);
    lappend(&st->postcheckscope, &st->npostcheckscope, s);
}

static void typeerror(Inferstate *st, Type *a, Type *b, Node *ctx, char *msg)
{
    char *t1, *t2, *c;

    t1 = tystr(tyfix(st, NULL, a, 1));
    t2 = tystr(tyfix(st, NULL, b, 1));
    c = ctxstr(st, ctx);
    if (msg)
        fatal(ctx, "type \"%s\" incompatible with \"%s\" near %s: %s", t1, t2, c, msg);
    else
        fatal(ctx, "type \"%s\" incompatible with \"%s\" near %s", t1, t2, c);
    free(t1);
    free(t2);
    free(c);
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


static int needfreshenrec(Inferstate *st, Type *t, Bitset *visited)
{
    size_t i;

    if (bshas(visited, t->tid))
        return 0;
    bsput(visited, t->tid);
    switch (t->type) {
        case Typaram:   return 1;
        case Tygeneric: return 1;
        case Tyname:
            for (i = 0; i < t->narg; i++)
                if (needfreshenrec(st, t->arg[i], visited))
                    return 1;
            return needfreshenrec(st, t->sub[0], visited);
        case Tystruct:
            for (i = 0; i < t->nmemb; i++)
                if (needfreshenrec(st, decltype(t->sdecls[i]), visited))
                    return 1;
            break;
        case Tyunion:
            for (i = 0; i < t->nmemb; i++)
                if (t->udecls[i]->etype && needfreshenrec(st, t->udecls[i]->etype, visited))
                    return 1;
            break;
        default:
            for (i = 0; i < t->nsub; i++)
                if (needfreshenrec(st, t->sub[i], visited))
                    return 1;
            break;
    }
    return 0;
}

static int needfreshen(Inferstate *st, Type *t)
{
    Bitset *visited;
    int ret;

    visited = mkbs();
    ret = needfreshenrec(st, t, visited);
    bsfree(visited);
    return ret;
}

/* Freshens the type of a declaration. */
static Type *tyfreshen(Inferstate *st, Htab *subst, Type *t)
{
    char *from, *to;

    if (!needfreshen(st, t)) {
        if (debugopt['u'])
            indentf(st->indentdepth, "%s isn't generic: skipping freshen\n", tystr(t));
        return t;
    }

    from = tystr(t);
    tybind(st, t);
    if (!subst) {
        subst = mkht(tyhash, tyeq);
        t = tyspecialize(t, subst, st->delayed);
        htfree(subst);
    } else {
        t = tyspecialize(t, subst, st->delayed);
    }
    tyunbind(st, t);
    if (debugopt['u']) {
        to = tystr(t);
        indentf(st->indentdepth, "Freshen %s => %s\n", from, to);
        free(from);
        free(to);
    }

    return t;
}


/* Resolves a type and all it's subtypes recursively.*/
static void tyresolve(Inferstate *st, Type *t)
{
    size_t i;
    Type *base;

    if (t->resolved)
        return;
    /* type resolution should never throw errors about non-generics
     * showing up within a generic type, so we push and pop a generic
     * around resolution */
    st->ingeneric++;
    t->resolved = 1;
    /* Walk through aggregate type members */
    if (t->type == Tystruct) {
        st->intype++;
        for (i = 0; i < t->nmemb; i++)
            infernode(st, &t->sdecls[i], NULL, NULL);
        st->intype--;
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
        if (!st->intype && !t->asize)
            lfatal(t->loc, "unsized array type outside of struct");
        infernode(st, &t->asize, NULL, NULL);
    }

    for (i = 0; i < t->nsub; i++)
        t->sub[i] = tf(st, t->sub[i]);
    base = tybase(t);
    /* no-ops if base == t */
    if (t->traits)
        bsunion(t->traits, base->traits);
    else
        t->traits = bsdup(base->traits);
    if (tyinfinite(st, t, NULL))
        lfatal(t->loc, "type %s includes itself", tystr(t));
    st->ingeneric--;
}

/* Look up the best type to date in the unification table, returning it */
Type *tysearch(Type *t)
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
                fatal(t->name, "could not resolve namespace \"%s\"", t->name->name.ns);
            if (!(lu = gettype(ns, t->name)))
                fatal(t->name, "could not resolve type %s", tystr(t));
            tytab[t->tid] = lu;
        }

        if (!tytab[t->tid])
            break;
        /* compress paths: shift the link up one level */
        if (tytab[tytab[t->tid]->tid])
            tytab[t->tid] = tytab[tytab[t->tid]->tid];
        t = tytab[t->tid];
    }
    return t;
}

static Type *tysubst(Inferstate *st, Type *t, Type *orig)
{
    Htab *subst;
    size_t i;

    subst = mkht(tyhash, tyeq);
    for (i = 0; i < t->ngparam; i++) {
        htput(subst, t->gparam[i], tf(st, orig->arg[i]));
    }
    t = tyfreshen(st, subst, t);
    htfree(subst);
    return t;
}

/* fixd the most accurate type mapping we have (ie,
 * the end of the unification chain */
static Type *tf(Inferstate *st, Type *orig)
{
    int isgeneric;
    Type *t;

    assert(orig != NULL);
    t = tysearch(orig);
    isgeneric = t->type == Tygeneric;
    st->ingeneric += isgeneric;
    tyresolve(st, t);
    /* If this is an instantiation of a generic type, we want the params to
     * match the instantiation */
    if (orig->type == Tyunres && t->type == Tygeneric) {
        if (t->ngparam != orig->narg) {
            lfatal(orig->loc, "%s incompatibly specialized with %s, declared on %s:%d",
                   tystr(orig), tystr(t), file->file.files[t->loc.file], t->loc.line);
        }
        t = tysubst(st, t, orig);
    }
    st->ingeneric -= isgeneric;
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
            die("untypable node %s", nodestr[n->type]);
            break;
    }
}

/* Gets the type of a literal value */
static Type *littype(Node *n)
{
    Type *t;

    if (!n->lit.type) {
        switch (n->lit.littype) {
            case Lchr:      t = mktype(n->loc, Tychar);                         break;
            case Lbool:     t = mktype(n->loc, Tybool);                         break;
            case Lint:      t = mktylike(n->loc, Tyint);                        break;
            case Lflt:      t = mktylike(n->loc, Tyflt64);                      break;
            case Lstr:      t = mktyslice(n->loc, mktype(n->loc, Tybyte));      break;
            case Llbl:      t = mktyptr(n->loc, mktype(n->loc, Tyvoid));        break;
            case Lfunc:     t = n->lit.fnval->func.type;                        break;
        }
        n->lit.type = t;
    }
    return n->lit.type;
}

static Type *delayeducon(Inferstate *st, Type *fallback)
{
    Type *t;
    char *from, *to;

    if (fallback->type != Tyunion)
        return fallback;
    t = mktylike(fallback->loc, fallback->type);
    htput(st->delayed, t, fallback);
    if (debugopt['u']) {
        from = tystr(t);
        to = tystr(fallback);
        indentf(st->indentdepth, "Delay %s -> %s\n", from, to);
        free(from);
        free(to);
    }
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
        die("untypeable node %s", nodestr[n->type]);
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
        fatal(n, "no namespace %s\n", args[0]->name.ns);
    uc = getucon(ns, args[0]);
    if (!uc)
        fatal(n, "no union constructor `%s", ctxstr(st, args[0]));
    if (!uc->etype && n->expr.nargs > 1)
        fatal(n, "nullary union constructor `%s passed arg ", ctxstr(st, args[0]));
    else if (uc->etype && n->expr.nargs != 2)
        fatal(n, "union constructor `%s needs arg ", ctxstr(st, args[0]));
    return uc;
}

/* Binds the type parameters present in the
 * current type into the type environment */
static void putbindings(Inferstate *st, Htab *bt, Type *t)
{
    size_t i;
    char *s;

    if (!t)
        return;
    if (t->type != Typaram)
        return;

    if (debugopt['u']) {
        s = tystr(t);
        indentf(st->indentdepth, "Bind %s\n", s);
        free(s);
    }
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
    char *s;

    if (t->type != Tygeneric)
        return;
    if (debugopt['u']) {
        s = tystr(t);
        indentf(st->indentdepth, "Binding %s\n", s);
        free(s);
    }
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
        fatal(n, "generic %s has no initializer", n->decl);

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
    if (t->type != Tygeneric)
        return;
    htfree(st->tybindings[st->ntybindings - 1]);
    lpop(&st->tybindings, &st->ntybindings);
}

/* Constrains a type to implement the required constraints. On
 * type variables, the constraint is added to the required
 * constraint list. Otherwise, the type is checked to see
 * if it has the required constraint */
static void constrain(Inferstate *st, Node *ctx, Type *a, Trait *c)
{
    if (a->type == Tyvar) {
        if (!a->traits)
            a->traits = mkbs();
        settrait(a, c);
    } else if (!a->traits || !bshas(a->traits, c->uid)) {
        fatal(ctx, "%s needs %s near %s", tystr(a), namestr(c->name), ctxstr(st, ctx));
    }
}

/* does b satisfy all the constraints of a? */
static int checktraits(Type *a, Type *b)
{
    /* a has no traits to satisfy */
    if (!a->traits)
        return 1;
    /* b satisfies no traits; only valid if a requires none */
    if (!b->traits)
        return bscount(a->traits) == 0;
    /* if a->traits is a subset of b->traits, all of
     * a's constraints are satisfied by b. */
    return bsissubset(a->traits, b->traits);
}

/* Merges the constraints on types */
static void mergetraits(Inferstate *st, Node *ctx, Type *a, Type *b)
{
    size_t i, n;
    char *sep;
    char traitbuf[1024], abuf[1024], bbuf[1024];

    if (b->type == Tyvar) {
        /* make sure that if a = b, both have same traits */
        if (a->traits && b->traits)
            bsunion(b->traits, a->traits);
        else if (a->traits)
            b->traits = bsdup(a->traits);
        else if (b->traits)
            a->traits = bsdup(b->traits);
    } else {
        if (!checktraits(a, b)) {
            sep = "";
            n = 0;
            for (i = 0; bsiter(a->traits, &i); i++) {
                if (!b->traits || !bshas(b->traits, i))
                    n += snprintf(traitbuf + n, sizeof(traitbuf) - n, "%s%s", sep, namestr(traittab[i]->name));
                sep = ",";
            }
            tyfmt(abuf, sizeof abuf, a);
            tyfmt(bbuf, sizeof bbuf, b);
            fatal(ctx, "%s missing traits %s for %s near %s", bbuf, traitbuf, abuf, ctxstr(st, ctx));
        }
    }
}

/* Tells us if we have an index hack on the type */
static int idxhacked(Type *a, Type *b)
{
    if (a->type == Tyvar && a->nsub > 0)
        return 1;
    if (a->type == Tyarray || a->type == Tyslice)
        return a->type == b->type;
    return 0;
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

static void unionunify(Inferstate *st, Node *ctx, Type *u, Type *v)
{
    size_t i, j;
    int found;

    if (u->nmemb != v->nmemb)
        fatal(ctx, "can't unify %s and %s near %s\n", tystr(u), tystr(v), ctxstr(st, ctx));

    for (i = 0; i < u->nmemb; i++) {
        found = 0;
        for (j = 0; j < v->nmemb; j++) {
            if (strcmp(namestr(u->udecls[i]->name), namestr(v->udecls[i]->name)) != 0)
                continue;
            found = 1;
            if (u->udecls[i]->etype == NULL && v->udecls[i]->etype == NULL)
                continue;
            else if (u->udecls[i]->etype && v->udecls[i]->etype)
                unify(st, ctx, u->udecls[i]->etype, v->udecls[i]->etype);
            else
                fatal(ctx, "can't unify %s and %s near %s\n", tystr(u), tystr(v), ctxstr(st, ctx));
        }
        if (!found)
            fatal(ctx, "can't unify %s and %s near %s\n", tystr(u), tystr(v), ctxstr(st, ctx));
    }
}

static void structunify(Inferstate *st, Node *ctx, Type *u, Type *v)
{
    size_t i, j;
    int found;

    if (u->nmemb != v->nmemb)
        fatal(ctx, "can't unify %s and %s near %s\n", tystr(u), tystr(v), ctxstr(st, ctx));

    for (i = 0; i < u->nmemb; i++) {
        found = 0;
        for (j = 0; j < v->nmemb; j++) {
            if (strcmp(namestr(u->sdecls[i]->decl.name), namestr(v->sdecls[i]->decl.name)) != 0)
                continue;
            found = 1;
            unify(st, u->sdecls[i], type(st, u->sdecls[i]), type(st, v->sdecls[i]));
        }
        if (!found)
            fatal(ctx, "can't unify %s and %s near %s\n", tystr(u), tystr(v), ctxstr(st, ctx));
    }
}

static void membunify(Inferstate *st, Node *ctx, Type *u, Type *v) {
    if (hthas(st->delayed, u))
        u = htget(st->delayed, u);
    u = tybase(u);
    if (hthas(st->delayed, v))
        v = htget(st->delayed, v);
    v = tybase(v);
    if (u->type == Tyunion && v->type == Tyunion && u != v)
        unionunify(st, ctx, u, v);
    else if (u->type == Tystruct && v->type == Tystruct && u != v)
        structunify(st, ctx, u, v);
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
        indentf(st->indentdepth, "Unify %s => %s\n", from, to);
        free(from);
        free(to);
    }

    r = NULL;
    if (a->type == Tyvar) {
        tytab[a->tid] = b;
        r = b;
    }

    /* Disallow recursive types */
   if (a->type == Tyvar && b->type != Tyvar)  {
        if (occurs(a, b))
            typeerror(st, a, b, ctx, "Infinite type\n");
   }

    /* if the tyrank of a is 0 (ie, a raw tyvar), just unify.
     * Otherwise, match up subtypes. */
    if ((a->type == b->type || idxhacked(a, b)) && tyrank(a) != 0) {
        if (a->type == Tyname && !nameeq(a->name, b->name))
            typeerror(st, a, b, ctx, NULL);
        if (a->nsub != b->nsub) {
            if (tybase(a)->type == Tyfunc)
                typeerror(st, a, b, ctx, "function arity mismatch");
            else
                typeerror(st, a, b, ctx, "subtype counts incompatible");
        }
        for (i = 0; i < b->nsub; i++)
            unify(st, ctx, a->sub[i], b->sub[i]);
        r = b;
    } else if (hasparam(a) && hasparam(b)) {
        /* Only Tygeneric and Tyname should be able to unify. And they
         * should have the same names for this to be true. */
        if (!nameeq(a->name, b->name))
            typeerror(st, a, b, ctx, NULL);
        if (a->narg != b->narg)
            typeerror(st, a, b, ctx, "Incompatible parameter lists");
        for (i = 0; i < a->narg; i++)
            unify(st, ctx, a->arg[i], b->arg[i]);
        r = b;
    } else if (a->type != Tyvar) {
        typeerror(st, a, b, ctx, NULL);
    }
    mergetraits(st, ctx, a, b);
    if (a->isreflect || b->isreflect)
        r->isreflect = a->isreflect = b->isreflect = 1;
    membunify(st, ctx, a, b);

    /* if we have delayed types for a tyvar, transfer it over. */
    if (a->type == Tyvar && b->type == Tyvar) {
        if (hthas(st->delayed, a) && !hthas(st->delayed, b))
            htput(st->delayed, b, htget(st->delayed, a));
        else if (hthas(st->delayed, b) && !hthas(st->delayed, a))
            htput(st->delayed, a, htget(st->delayed, b));
    } else if (hthas(st->delayed, a)) {
        unify(st, ctx, htget(st->delayed, a), tybase(b));
    }

    return r;
}

static void markvatypes(Type **types, size_t ntypes)
{
    size_t i;

    for (i = 0; i < ntypes; i++)
        types[i]->isreflect = 1;
}

/* Applies unifications to function calls.
 * Funciton application requires a slightly
 * different approach to unification. */
static void unifycall(Inferstate *st, Node *n)
{
    size_t i;
    Type *ft;
    char *ret, *ctx;

    ft = type(st, n->expr.args[0]);

    if (ft->type == Tyvar) {
        /* the first arg is the function itself, so it shouldn't be counted */
        ft = mktyfunc(n->loc, &n->expr.args[1], n->expr.nargs - 1, mktyvar(n->loc));
        unify(st, n, ft, type(st, n->expr.args[0]));
    } else if (tybase(ft)->type != Tyfunc) {
        fatal(n, "calling uncallable type %s", tystr(ft));
    }
    /* first arg: function itself */
    for (i = 1; i < n->expr.nargs; i++)
        if (exprtype(n->expr.args[i])->type == Tyvoid)
            fatal(n, "void passed where value expected, near %s", ctxstr(st, n));
    for (i = 1; i < n->expr.nargs; i++) {
        if (i == ft->nsub)
            fatal(n, "%s arity mismatch (expected %zd args, got %zd)",
                  ctxstr(st, n->expr.args[0]), ft->nsub - 1, n->expr.nargs - 1);

        if (ft->sub[i]->type == Tyvalist) {
            markvatypes(&ft->sub[i], ft->nsub - i);
            break;
        }
        inferexpr(st, &n->expr.args[i], NULL, NULL);
        unify(st, n->expr.args[0], ft->sub[i], type(st, n->expr.args[i]));
    }
    if (i < ft->nsub && ft->sub[i]->type != Tyvalist)
        fatal(n, "%s arity mismatch (expected %zd args, got %zd)",
              ctxstr(st, n->expr.args[0]), ft->nsub - 1, i - 1);
    if (debugopt['u']) {
        ret = tystr(ft->sub[0]);
        ctx = ctxstr(st, n->expr.args[0]);
        indentf(st->indentdepth, "Call of %s returns %s\n", ctx, ret);
        free(ctx);
        free(ret);
    }

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
        fatal(ctx, "mismatched arg list sizes: %s with %s near %s", tystr(a), tystr(b), ctxstr(st, ctx));
    for (i = 0; i < a->narg; i++)
        unify(st, ctx, a->arg[i], b->arg[i]);
}

static void loaduses(Node *n)
{
    size_t i;

    /* uses only allowed at top level. Do we want to keep it this way? */
    for (i = 0; i < n->file.nuses; i++)
        readuse(n->file.uses[i], n->file.globls, Visintern);
}

static Type *initvar(Inferstate *st, Node *n, Node *s)
{
    Type *t;

    if (s->decl.ishidden)
        fatal(n, "attempting to refer to hidden decl %s", ctxstr(st, n));
    if (s->decl.isgeneric)
        t = tysubst(st, tf(st, s->decl.type), s->decl.type);
    else
        t = s->decl.type;
    n->expr.did = s->decl.did;
    n->expr.isconst = s->decl.isconst;
    if (s->decl.isgeneric && !st->ingeneric) {
        t = tyfreshen(st, NULL, t);
        addspecialization(st, n, curstab());
        if (t->type == Tyvar) {
            settype(st, n, mktyvar(n->loc));
            delayedcheck(st, n, curstab());
        } else {
            settype(st, n, t);
        }
    } else { 
        settype(st, n, t);
    }
    return t;
}

/* Finds out if the member reference is actually
 * referring to a namespaced name, instead of a struct
 * member. If it is, it transforms it into the variable
 * reference we should have, instead of the Omemb expr
 * that we do have */
static Node *checkns(Inferstate *st, Node *n, Node **ret)
{
    Node *var, *name, *nsname;
    Node **args;
    Stab *stab;
    Node *s;

    /* check that this is a namespaced declaration */
    if (n->type != Nexpr)
        return n;
    if (exprop(n) != Omemb)
        return n;
    if (!n->expr.nargs)
        return n;
    args = n->expr.args;
    if (args[0]->type != Nexpr || exprop(args[0]) != Ovar)
        return n;
    name = args[0]->expr.args[0];
    stab = getns(curstab(), name);
    if (!stab)
        return n;

    /* substitute the namespaced name */
    nsname = mknsname(n->loc, namestr(name), namestr(args[1]));
    s = getdcl(stab, args[1]);
    if (!s)
        fatal(n, "undeclared var %s.%s", nsname->name.ns, nsname->name.name);
    var = mkexpr(n->loc, Ovar, nsname, NULL);
    var->expr.idx = n->expr.idx;
    initvar(st, var, s);
    *ret = var;
    return var;
}

static void inferstruct(Inferstate *st, Node *n, int *isconst)
{
    size_t i;

    *isconst = 1;
    for (i = 0; i < n->expr.nargs; i++) {
        infernode(st, &n->expr.args[i], NULL, NULL);
        if (!n->expr.args[i]->expr.isconst)
            *isconst = 0;
    }
    settype(st, n, mktyvar(n->loc));
    delayedcheck(st, n, curstab());
}

static void inferarray(Inferstate *st, Node *n, int *isconst)
{
    size_t i;
    Type *t;
    Node *len;

    *isconst = 1;
    len = mkintlit(n->loc, n->expr.nargs);
    t = mktyarray(n->loc, mktyvar(n->loc), len);
    for (i = 0; i < n->expr.nargs; i++) {
        infernode(st, &n->expr.args[i], NULL, NULL);
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

    *isconst = 1;
    types = xalloc(sizeof(Type *)*n->expr.nargs);
    for (i = 0; i < n->expr.nargs; i++) {
        infernode(st, &n->expr.args[i], NULL, NULL);
        n->expr.isconst = n->expr.isconst && n->expr.args[i]->expr.isconst;
        types[i] = type(st, n->expr.args[i]);
    }
    *isconst = n->expr.isconst;
    settype(st, n, mktytuple(n->loc, types, n->expr.nargs));
}

static void inferucon(Inferstate *st, Node *n, int *isconst)
{
    Ucon *uc;
    Type *t;

    *isconst = 1;
    uc = uconresolve(st, n);
    t = tysubst(st, tf(st, uc->utype), uc->utype);
    uc = tybase(t)->udecls[uc->id];
    if (uc->etype) {
        inferexpr(st, &n->expr.args[1], NULL, NULL);
        unify(st, n, uc->etype, type(st, n->expr.args[1]));
        *isconst = n->expr.args[1]->expr.isconst;
    }
    settype(st, n, delayeducon(st, t));
}

static void inferpat(Inferstate *st, Node **np, Node *val, Node ***bind, size_t *nbind)
{
    size_t i;
    Node **args;
    Node *s, *n;
    Stab *ns;
    Type *t;

    n = *np;
    n = checkns(st, n, np);
    args = n->expr.args;
    for (i = 0; i < n->expr.nargs; i++)
        if (args[i]->type == Nexpr)
            inferpat(st, &args[i], val, bind, nbind);
    switch (exprop(n)) {
        case Otup:
        case Ostruct:
        case Oarr:
        case Olit:
        case Omemb:
            infernode(st, np, NULL, NULL);
            break;
        /* arithmetic expressions just need to be constant */
        case Oneg:
        case Oadd:
        case Osub:
        case Omul:
        case Odiv:
        case Obsl:
        case Obsr:
        case Oband:
        case Obor:
        case Obxor:
        case Obnot:
            infernode(st, np, NULL, NULL);
            if (!n->expr.isconst)
                fatal(n, "matching against non-constant expression near %s", ctxstr(st, n));
            break;
        case Oucon:     inferucon(st, n, &n->expr.isconst);     break;
        case Ovar:
            ns = curstab();
            if (args[0]->name.ns)
                ns = getns_str(ns, args[0]->name.ns);
            s = getdcl(ns, args[0]);
            if (s && !s->decl.ishidden) {
                if (s->decl.isgeneric)
                    t = tysubst(st, s->decl.type, s->decl.type);
                else if (s->decl.isconst)
                    t = s->decl.type;
                else
                    fatal(n, "pattern shadows variable declared on %s:%d near %s", fname(s->loc), lnum(s->loc), ctxstr(st, s));
            } else {
                t = mktyvar(n->loc);
                s = mkdecl(n->loc, n->expr.args[0], t);
                s->decl.init = val;
                settype(st, n, t);
                lappend(bind, nbind, s);
            }
            settype(st, n, t);
            n->expr.did = s->decl.did;
            break;
        default:
            fatal(n, "invalid pattern");
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

static void infersub(Inferstate *st, Node *n, Type *ret, int *sawret, int *exprconst)
{
    Node **args;
    size_t i, nargs;
    int isconst;

    args = n->expr.args;
    nargs = n->expr.nargs;
    isconst = 1;
    for (i = 0; i < nargs; i++) {
        /* Nlit, Nvar, etc should not be inferred as exprs */
        if (args[i]->type == Nexpr) {
            /* Omemb can sometimes resolve to a namespace. We have to check
             * this. Icky. */
            checkns(st, args[i], &args[i]);
            inferexpr(st, &args[i], ret, sawret);
            isconst = isconst && args[i]->expr.isconst;
        }
    }
    if (exprop(n) == Ovar)
        n->expr.isconst = decls[n->expr.did]->decl.isconst;
    else if (opispure[exprop(n)])
        n->expr.isconst = isconst;
    *exprconst = n->expr.isconst;
}

static void inferexpr(Inferstate *st, Node **np, Type *ret, int *sawret)
{
    Node **args;
    size_t i, nargs;
    Node *s, *n;
    Type *t;
    int isconst;

    n = *np;
    assert(n->type == Nexpr);
    args = n->expr.args;
    nargs = n->expr.nargs;
    infernode(st, &n->expr.idx, NULL, NULL);
    n = checkns(st, n, np);
    switch (exprop(n)) {
        /* all operands are same type */
        case Oadd:      /* @a + @a -> @a */
        case Osub:      /* @a - @a -> @a */
        case Omul:      /* @a * @a -> @a */
        case Odiv:      /* @a / @a -> @a */
        case Oneg:      /* -@a -> @a */
            infersub(st, n, ret, sawret, &isconst);
            t = type(st, args[0]);
            constrain(st, n, type(st, args[0]), traittab[Tcnum]);
            isconst = args[0]->expr.isconst;
            for (i = 1; i < nargs; i++) {
                isconst = isconst && args[i]->expr.isconst;
                t = unify(st, n, t, type(st, args[i]));
            }
            n->expr.isconst = isconst;
            settype(st, n, t);
            break;
        case Omod:      /* @a % @a -> @a */
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
            infersub(st, n, ret, sawret, &isconst);
            t = type(st, args[0]);
            constrain(st, n, type(st, args[0]), traittab[Tcnum]);
            constrain(st, n, type(st, args[0]), traittab[Tcint]);
            isconst = args[0]->expr.isconst;
            for (i = 1; i < nargs; i++) {
                isconst = isconst && args[i]->expr.isconst;
                t = unify(st, n, t, type(st, args[i]));
            }
            n->expr.isconst = isconst;
            settype(st, n, t);
            break;
        case Oasn:      /* @a = @a -> @a */
            infersub(st, n, ret, sawret, &isconst);
            t = type(st, args[0]);
            for (i = 1; i < nargs; i++)
                t = unify(st, n, t, type(st, args[i]));
            settype(st, n, t);
            if (args[0]->expr.isconst)
                fatal(n, "attempting to assign constant \"%s\"", ctxstr(st, args[0]));
            break;

        /* operands same type, returning bool */
        case Olor:      /* @a || @b -> bool */
        case Oland:     /* @a && @b -> bool */
        case Oeq:       /* @a == @a -> bool */
        case One:       /* @a != @a -> bool */
        case Ogt:       /* @a > @a -> bool */
        case Oge:       /* @a >= @a -> bool */
        case Olt:       /* @a < @a -> bool */
        case Ole:       /* @a <= @b -> bool */
            infersub(st, n, ret, sawret, &isconst);
            t = type(st, args[0]);
            for (i = 1; i < nargs; i++)
                unify(st, n, t, type(st, args[i]));
            settype(st, n, mktype(Zloc, Tybool));
            break;

        case Olnot:     /* !bool -> bool */
            infersub(st, n, ret, sawret, &isconst);
            t = unify(st, n, type(st, args[0]), mktype(Zloc, Tybool));
            settype(st, n, t);
            break;

        /* reach into a type and pull out subtypes */
        case Oaddr:     /* &@a -> @a* */
            infersub(st, n, ret, sawret, &isconst);
            settype(st, n, mktyptr(n->loc, type(st, args[0])));
            break;
        case Oderef:    /* *@a* ->  @a */
            infersub(st, n, ret, sawret, &isconst);
            t = unify(st, n, type(st, args[0]), mktyptr(n->loc, mktyvar(n->loc)));
            settype(st, n, t->sub[0]);
            break;
        case Oidx:      /* @a[@b::tcint] -> @a */
            infersub(st, n, ret, sawret, &isconst);
            t = mktyidxhack(n->loc, mktyvar(n->loc));
            unify(st, n, type(st, args[0]), t);
            constrain(st, n, type(st, args[0]), traittab[Tcidx]);
            constrain(st, n, type(st, args[1]), traittab[Tcint]);
            settype(st, n, t->sub[0]);
            break;
        case Oslice:    /* @a[@b::tcint,@b::tcint] -> @a[,] */
            infersub(st, n, ret, sawret, &isconst);
            t = mktyidxhack(n->loc, mktyvar(n->loc));
            unify(st, n, type(st, args[0]), t);
            constrain(st, n, type(st, args[1]), traittab[Tcint]);
            constrain(st, n, type(st, args[2]), traittab[Tcint]);
            settype(st, n, mktyslice(n->loc, t->sub[0]));
            break;

        /* special cases */
        case Omemb:     /* @a.Ident -> @b, verify type(@a.Ident)==@b later */
            infersub(st, n, ret, sawret, &isconst);
            settype(st, n, mktyvar(n->loc));
            delayedcheck(st, n, curstab());
            break;
        case Osize:     /* sizeof @a -> size */
            infersub(st, n, ret, sawret, &isconst);
            settype(st, n, mktylike(n->loc, Tyuint));
            break;
        case Ocall:     /* (@a, @b, @c, ... -> @r)(@a,@b,@c, ... -> @r) -> @r */
            infersub(st, n, ret, sawret, &isconst);
            unifycall(st, n);
            break;
        case Ocast:     /* cast(@a, @b) -> @b */
            infersub(st, n, ret, sawret, &isconst);
            delayedcheck(st, n, curstab());
            break;
        case Oret:      /* -> @a -> void */
            infersub(st, n, ret, sawret, &isconst);
            if (sawret)
                *sawret = 1;
            if (!ret)
                fatal(n, "returns are not valid near %s", ctxstr(st, n));
            if (nargs)
                t = unify(st, n, ret, type(st, args[0]));
            else
                t =  unify(st, n, mktype(Zloc, Tyvoid), ret);
            settype(st, n, t);
            break;
        case Obreak:
        case Ocontinue:
            /* nullary: nothing to infer. */
            settype(st, n, mktype(Zloc, Tyvoid));
            break;
        case Ojmp:     /* goto void* -> void */
            infersub(st, n, ret, sawret, &isconst);
            settype(st, n, mktype(Zloc, Tyvoid));
            break;
        case Ovar:      /* a:@a -> @a */
            infersub(st, n, ret, sawret, &isconst);
            /* if we created this from a namespaced var, the type should be
             * set, and the normal lookup is expected to fail. Since we're
             * already done with this node, we can just return. */
            if (n->expr.type)
                return;
            s = getdcl(curstab(), args[0]);
            if (!s)
                fatal(n, "undeclared var %s", ctxstr(st, args[0]));
            initvar(st, n, s);
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
            infersub(st, n, ret, sawret, &isconst);
            switch (args[0]->lit.littype) {
                case Lfunc:
                    infernode(st, &args[0]->lit.fnval, NULL, NULL); break;
                    /* FIXME: env capture means this is non-const */
                    n->expr.isconst = 1;
                default:
                    n->expr.isconst = 1;
                    break;
            }
            settype(st, n, type(st, args[0]));
            break;
        case Oundef:
            infersub(st, n, ret, sawret, &isconst);
            settype(st, n, mktype(n->loc, Tyvoid));
            break;
        case Odef:
        case Odead:
            n->expr.type = mktype(n->loc, Tyvoid);
            break;
        case Obad: case Ocjmp: case Ojtab: case Oset:
        case Oslbase: case Osllen: case Outag:
        case Oblit: case  Oclear: case Oudata:
        case Otrunc: case Oswiden: case Ozwiden:
        case Oint2flt: case Oflt2int: case Oflt2flt:
        case Ofadd: case Ofsub: case Ofmul: case Ofdiv: case Ofneg:
        case Ofeq: case Ofne: case Ofgt: case Ofge: case Oflt: case Ofle:
        case Oueq: case Oune: case Ougt: case Ouge: case Oult: case Oule:
        case Numops:
            die("Should not see %s in fe", opstr[exprop(n)]);
            break;
    }
}

static void inferfunc(Inferstate *st, Node *n)
{
    size_t i;
    int sawret;

    sawret = 0;
    for (i = 0; i < n->func.nargs; i++)
        infernode(st, &n->func.args[i], NULL, NULL);
    infernode(st, &n->func.body, n->func.type->sub[0], &sawret);
    /* if there's no return stmt in the function, assume void ret */
    if (!sawret)
        unify(st, n, type(st, n)->sub[0], mktype(Zloc, Tyvoid));
}

static void specializeimpl(Inferstate *st, Node *n)
{
    Node *dcl, *proto, *name;
    Htab *ht;
    Trait *t;
    Type *ty;
    size_t i, j;

    t = gettrait(curstab(), n->impl.traitname);
    if (!t)
        fatal(n, "no trait %s\n", namestr(n->impl.traitname));
    n->impl.trait = t;

    dcl = NULL;
    proto = NULL;
    for (i = 0; i < n->impl.ndecls; i++) {
        /* look up the prototype */
        proto = NULL;
        dcl = n->impl.decls[i];

        /*
           since the decls in an impl are not installed in a namespace, their names
           are not updated when we call updatens() on the symbol table. Because we need
           to do namespace dependent comparisons for specializing, we need to set the
           namespace here.
         */
        if (file->file.globls->name)
            setns(dcl->decl.name, file->file.globls->name);
        for (j = 0; j < t->nfuncs; j++) {
            if (nameeq(dcl->decl.name, t->funcs[j]->decl.name)) {
                proto = t->funcs[j];
                break;
            }
        }
        if (!proto)
            fatal(n, "declaration %s missing in %s, near %s",
                  namestr(dcl->decl.name), namestr(t->name), ctxstr(st, n));

        /* infer and unify types */
        if (n->impl.type->type == Tygeneric || n->impl.type->type == Typaram)
            fatal(n, "trait specialization requires concrete type, got %s", tystr(n->impl.type));
        checktraits(t->param, n->impl.type);
        ht = mkht(tyhash, tyeq);
        htput(ht, t->param, n->impl.type);
        ty = tyspecialize(type(st, proto), ht, st->delayed);
        htfree(ht);

        inferdecl(st, dcl);
        unify(st, n, type(st, dcl), ty);

        /* and put the specialization into the global stab */
        name = genericname(proto, ty);
        dcl->decl.name = name;
        putdcl(file->file.globls, dcl);
        if (debugopt['S'])
            printf("specializing trait [%d]%s:%s => %s:%s\n",
                   n->loc.line, namestr(proto->decl.name), tystr(type(st, proto)), namestr(name), tystr(ty));
        dcl->decl.vis = t->vis;
        lappend(&file->file.stmts, &file->file.nstmts, dcl);
    }
}

static void inferdecl(Inferstate *st, Node *n)
{
    Type *t;

    t = tf(st, decltype(n));
    if (t->type == Tygeneric && !n->decl.isgeneric) {
        t = tyfreshen(st, NULL, t);
        unifyparams(st, n, t, decltype(n));
    }
    settype(st, n, t);
    if (n->decl.init) {
        inferexpr(st, &n->decl.init, NULL, NULL);
        unify(st, n, type(st, n), type(st, n->decl.init));
        if (n->decl.isconst && !n->decl.init->expr.isconst)
            fatal(n, "non-const initializer for \"%s\"", ctxstr(st, n));
    } else {
        if ((n->decl.isconst || n->decl.isgeneric) && !n->decl.isextern)
            fatal(n, "non-extern \"%s\" has no initializer", ctxstr(st, n));
    }
}

static void inferstab(Inferstate *st, Stab *s)
{
    void **k;
    size_t n, i;
    Type *t;

    k = htkeys(s->ty, &n);
    for (i = 0; i < n; i++) {
        t = gettype(s, k[i]);
        if (!t)
            fatal(k[i], "undefined type %s", namestr(k[i]));
        t = tysearch(t);
        updatetype(s, k[i], t);
    }
    free(k);
}

static void infernode(Inferstate *st, Node **np, Type *ret, int *sawret)
{
    size_t i, nbound;
    Node **bound, *n;
    Type *t;

    n = *np;
    if (!n)
        return;
    switch (n->type) {
        case Nfile:
            pushstab(n->file.globls);
            inferstab(st, n->file.globls);
            for (i = 0; i < n->file.nstmts; i++)
                infernode(st, &n->file.stmts[i], NULL, sawret);
            popstab();
            break;
        case Ndecl:
            if (debugopt['u'])
                indentf(st->indentdepth, "--- infer %s ---\n", declname(n));
            st->indentdepth++;
            bind(st, n);
            inferdecl(st, n);
            if (type(st, n)->type == Typaram && !st->ingeneric)
                fatal(n, "generic type %s in non-generic near %s", tystr(type(st, n)), ctxstr(st, n));
            unbind(st, n);
            st->indentdepth--;
            if (debugopt['u'])
                indentf(st->indentdepth, "--- done ---\n");
            break;
        case Nblock:
            setsuper(n->block.scope, curstab());
            pushstab(n->block.scope);
            inferstab(st, n->block.scope);
            for (i = 0; i < n->block.nstmts; i++) {
                infernode(st, &n->block.stmts[i], ret, sawret);
            }
            popstab();
            break;
        case Nifstmt:
            infernode(st, &n->ifstmt.cond, NULL, sawret);
            infernode(st, &n->ifstmt.iftrue, ret, sawret);
            infernode(st, &n->ifstmt.iffalse, ret, sawret);
            unify(st, n, type(st, n->ifstmt.cond), mktype(n->loc, Tybool));
            break;
        case Nloopstmt:
            infernode(st, &n->loopstmt.init, ret, sawret);
            infernode(st, &n->loopstmt.cond, NULL, sawret);
            infernode(st, &n->loopstmt.step, ret, sawret);
            infernode(st, &n->loopstmt.body, ret, sawret);
            unify(st, n, type(st, n->loopstmt.cond), mktype(n->loc, Tybool));
            break;
        case Niterstmt:
            bound = NULL;
            nbound = 0;

            inferpat(st, &n->iterstmt.elt, NULL, &bound, &nbound);
            addbindings(st, n->iterstmt.body, bound, nbound);

            infernode(st, &n->iterstmt.seq, NULL, sawret);
            infernode(st, &n->iterstmt.body, ret, sawret);

            t = mktyidxhack(n->loc, mktyvar(n->loc));
            constrain(st, n, type(st, n->iterstmt.seq), traittab[Tcidx]);
            unify(st, n, type(st, n->iterstmt.seq), t);
            unify(st, n, type(st, n->iterstmt.elt), t->sub[0]);
            break;
        case Nmatchstmt:
            infernode(st, &n->matchstmt.val, NULL, sawret);
            if (tybase(type(st, n->matchstmt.val))->type == Tyvoid)
                fatal(n, "can't match against a void type near %s", ctxstr(st, n->matchstmt.val));
            for (i = 0; i < n->matchstmt.nmatches; i++) {
                infernode(st, &n->matchstmt.matches[i], ret, sawret);
                unify(st, n->matchstmt.matches[i]->match.pat, type(st, n->matchstmt.val), type(st, n->matchstmt.matches[i]->match.pat));
            }
            break;
        case Nmatch:
            bound = NULL;
            nbound = 0;
            inferpat(st, &n->match.pat, NULL, &bound, &nbound);
            addbindings(st, n->match.block, bound, nbound);
            infernode(st, &n->match.block, ret, sawret);
            break;
        case Nexpr:
            inferexpr(st, np, ret, sawret);
            break;
        case Nfunc:
            setsuper(n->func.scope, curstab());
            if (st->ntybindings > 0)
                for (i = 0; i < n->func.nargs; i++)
                    putbindings(st, st->tybindings[st->ntybindings - 1], n->func.args[i]->decl.type);
            pushstab(n->func.scope);
            inferstab(st, n->func.scope);
            inferfunc(st, n);
            popstab();
            break;
        case Nimpl:
            specializeimpl(st, n);
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
static Type *tyfix(Inferstate *st, Node *ctx, Type *orig, int noerr)
{
    static Type *tyint, *tyflt;
    Type *t, *delayed;
    char *from, *to;
    size_t i;
    char buf[1024];

    if (!tyint)
        tyint = mktype(Zloc, Tyint);
    if (!tyflt)
        tyflt = mktype(Zloc, Tyflt64);

    t = tysearch(orig);
    if (orig->type == Tyvar && hthas(st->delayed, orig)) {
        delayed = htget(st->delayed, orig);
        if (t->type == Tyvar)
            t = delayed;
        else if (tybase(t)->type != delayed->type && !noerr)
            fatal(ctx, "type %s not compatible with %s near %s\n", tystr(t), tystr(delayed), ctxstr(st, ctx));
    }
    if (t->type == Tyvar) {
        if (hastrait(t, traittab[Tcint]) && checktraits(t, tyint))
            t = tyint;
        if (hastrait(t, traittab[Tcfloat]) && checktraits(t, tyflt))
            t = tyflt;
    } else if (!t->fixed) {
        t->fixed = 1;
        if (t->type == Tyarray) {
            typesub(st, t->asize);
        } else if (t->type == Tystruct) {
            st->intype++;
            for (i = 0; i < t->nmemb; i++)
                typesub(st, t->sdecls[i]);
            st->intype--;
        } else if (t->type == Tyunion) {
            for (i = 0; i < t->nmemb; i++) {
                if (t->udecls[i]->etype) {
                    tyresolve(st, t->udecls[i]->etype);
                    t->udecls[i]->etype = tyfix(st, ctx, t->udecls[i]->etype, noerr);
                }
            }
        } else if (t->type == Tyname) {
            for (i = 0; i < t->narg; i++)
                t->arg[i] = tyfix(st, ctx, t->arg[i], noerr);
        }
        for (i = 0; i < t->nsub; i++)
            t->sub[i] = tyfix(st, ctx, t->sub[i], noerr);
    }

    if (t->type == Tyvar && !noerr) {
        if (debugopt['T'])
            dump(file, stdout);
        fatal(ctx, "underconstrained type %s near %s", tyfmt(buf, 1024, t), ctxstr(st, ctx));
    }

    if (debugopt['u'] && !tyeq(orig, t)) {
        from = tystr(orig);
        to = tystr(t);
        indentf(st->indentdepth, "subst %s => %s\n", from, to);
        free(from);
        free(to);
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
            constrain(st, n, type(st, n), traittab[Tcnum]);
            constrain(st, n, type(st, n), traittab[Tcint]);
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
        if (tybase(t)->type == Typtr)
            t = tybase(tf(st, t->sub[0]));
        if (tybase(t)->type != Tystruct) {
            if (tybase(t)->type == Tyvar)
                fatal(n, "underspecified type defined on %s:%d used near %s", fname(t->loc), lnum(t->loc), ctxstr(st, n));
            else
                fatal(n, "type %s does not support member operators near %s", tystr(t), ctxstr(st, n));
        }
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
        fatal(aggr, "type %s has no member \"%s\" near %s",
              tystr(type(st, aggr)), ctxstr(st, memb), ctxstr(st, aggr));
}

static void checkstruct(Inferstate *st, Node *n)
{
    Type *t, *et;
    Node *val, *name;
    size_t i, j;

    t = tybase(tf(st, n->lit.type));
    if (t->type != Tystruct) {
        /*
         * If we haven't inferred the type, and it's inside another struct,
         * we'll eventually get to it.
         *
         * If, on the other hand, it is genuinely underspecified, we'll give
         * a better error on it later.
         */
        return;
    }

    for (i = 0; i < n->expr.nargs; i++) {
        val = n->expr.args[i];
        name = val->expr.idx;

        et = NULL;
        for (j = 0; j < t->nmemb; j++) {
            if (!strcmp(namestr(t->sdecls[j]->decl.name), namestr(name))) {
                et = type(st, t->sdecls[j]);
                break;
            }
        }

        if (!et)
            fatal(n, "could not find member %s in struct %s, near %s",
                  namestr(name), tystr(t), ctxstr(st, n));

        unify(st, val, et, type(st, val));
    }
}

static void checkvar(Inferstate *st, Node *n)
{
    Node *dcl;

    dcl = decls[n->expr.did];
    unify(st, n, type(st, n), tyfreshen(st, NULL, type(st, dcl)));
}

static void postcheck(Inferstate *st, Node *file)
{
    size_t i;
    Node *n;

    for (i = 0; i < st->npostcheck; i++) {
        n = st->postcheck[i];
        pushstab(st->postcheckscope[i]);
        if (n->type == Nexpr && exprop(n) == Omemb)
            infercompn(st, n);
        else if (n->type == Nexpr && exprop(n) == Ocast)
            checkcast(st, n);
        else if (n->type == Nexpr && exprop(n) == Ostruct)
            checkstruct(st, n);
        else if (n->type == Nexpr && exprop(n) == Ovar)
            checkvar(st, n);
        else
            die("Thing we shouldn't be checking in postcheck\n");
        popstab();
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
        t = tysearch(gettype(s, k[i]));
        updatetype(s, k[i], t);
        tyfix(st, k[i], t, 0);
    }
    free(k);

    k = htkeys(s->dcl, &n);
    for (i = 0; i < n; i++) {
        d = getdcl(s, k[i]);
        if (d)
            d->decl.type = tyfix(st, d, d->decl.type, 0);
    }
    free(k);
}

static void checkrange(Inferstate *st, Node *n)
{
    Type *t;
    int64_t sval;
    uint64_t uval;
    static const int64_t svranges[][2] = {
        /* signed ints */
        [Tyint8]  = {-128LL, 127LL},
        [Tyint16] = {-32768LL, 32767LL},
        [Tyint32] = {-2147483648LL, 2*2147483647LL}, /* FIXME: this has been doubled allow for uints... */
        [Tyint]   = {-2147483648LL, 2*2147483647LL},
        [Tyint64] = {-9223372036854775808ULL, 9223372036854775807LL},
        [Tylong]  = {-9223372036854775808ULL, 9223372036854775807LL},
    };

    static const uint64_t uvranges[][2] = {
        [Tybyte]   = {0, 255ULL},
        [Tyuint8]  = {0, 255ULL},
        [Tyuint16] = {0, 65535ULL},
        [Tyuint32] = {0, 4294967295ULL},
        [Tyuint64] = {0, 18446744073709551615ULL},
        [Tyulong]  = {0, 18446744073709551615ULL},
        [Tychar]   = {0, 4294967295ULL},
    };

    /* signed types */
    t = type(st, n);
    if (t->type >= Tyint8 && t->type <= Tylong) {
        sval = n->lit.intval;
        if (sval < svranges[t->type][0] || sval > svranges[t->type][1])
            fatal(n, "literal value %lld out of range for type \"%s\"", sval, tystr(t));
    } else if ((t->type >= Tybyte && t->type <= Tyulong) || t->type == Tychar) {
        uval = n->lit.intval;
        if (uval < uvranges[t->type][0] || uval > uvranges[t->type][1])
            fatal(n, "literal value %llu out of range for type \"%s\"", uval, tystr(t));
    }
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
            for (i = 0; i < n->file.nstmts; i++)
                typesub(st, n->file.stmts[i]);
            popstab();
            break;
        case Ndecl:
            settype(st, n, tyfix(st, n, type(st, n), 0));
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
        case Niterstmt:
            typesub(st, n->iterstmt.elt);
            typesub(st, n->iterstmt.seq);
            typesub(st, n->iterstmt.body);
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
            settype(st, n, tyfix(st, n, type(st, n), 0));
            typesub(st, n->expr.idx);
            if (exprop(n) == Ocast && exprop(n->expr.args[0]) == Olit && n->expr.args[0]->expr.args[0]->lit.littype == Lint) {
                settype(st, n->expr.args[0], exprtype(n));
                settype(st, n->expr.args[0]->expr.args[0], exprtype(n));
            }
            for (i = 0; i < n->expr.nargs; i++)
                typesub(st, n->expr.args[i]);
            break;
        case Nfunc:
            pushstab(n->func.scope);
            settype(st, n, tyfix(st, n, n->func.type, 0));
            for (i = 0; i < n->func.nargs; i++)
                typesub(st, n->func.args[i]);
            typesub(st, n->func.body);
            popstab();
            break;
        case Nlit:
            settype(st, n, tyfix(st, n, type(st, n), 0));
            switch (n->lit.littype) {
                case Lfunc:
                    typesub(st, n->lit.fnval); break;
                case Lint:
                    checkrange(st, n);
                default:        break;
            }
            break;
        case Nimpl:
        case Nname:
        case Nuse:
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
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
            t->isreflect = 1;
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
            for (i = 0; i < n->impl.ndecls; i++) {
                n->impl.decls[i]->decl.vis = Vishidden;
                nodetag(st, n->impl.decls[i], 0, hidelocal);
            }
            break;
        case Nuse: case Nname:
            break;
        case Nfile: case Nnone:
            die("Invalid node for type export\n");
            break;
    }
}

void tagexports(Stab *st, int hidelocal)
{
    void **k;
    Node *s;
    Type *t;
    Trait *tr;
    size_t i, j, n;

    k = htkeys(st->dcl, &n);
    for (i = 0; i < n; i++) {
        s = getdcl(st, k[i]);
        if (s->decl.vis == Visexport)
            nodetag(st, s, 0, hidelocal);
    }
    free(k);

    k = htkeys(st->impl, &n);
    for (i = 0; i < n; i++) {
        s = getimpl(st, k[i]);
        if (s->impl.vis == Visexport)
            nodetag(st, s, 0, hidelocal);
    }
    free(k);

    k = htkeys(st->tr, &n);
    for (i = 0; i < n; i++) {
        tr = gettrait(st, k[i]);
        if (tr->vis == Visexport) {
            tr->param->vis = Visexport;
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

    /* get the explicitly exported symbols */
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
            t->isreflect = 1;
            for (j = 0; j < t->narg; j++)
                taghidden(t->arg[j]);
        } else if (t->type == Tygeneric) {
            for (j = 0; j < t->ngparam; j++)
                taghidden(t->gparam[j]);
        }
    }
    free(k);

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

        /* we need to sub in default types in the specialization, so call
         * typesub on the specialized function */
        typesub(st, d);
        popstab();
    }
}

void applytraits(Inferstate *st, Node *f)
{
    size_t i;
    Node *n;
    Trait *trait;
    Type *ty;

    pushstab(f->file.globls);
    /* for now, traits can only be declared globally */
    for (i = 0; i < f->file.nstmts; i++) {
        if (f->file.stmts[i]->type == Nimpl) {
            n = f->file.stmts[i];
            trait = gettrait(f->file.globls, n->impl.traitname);
            if (!trait)
                fatal(n, "trait %s does not exist near %s", namestr(n->impl.traitname), ctxstr(st, n));
            ty = tf(st, n->impl.type);
            settrait(ty, trait);
        }
    }
    popstab();
}

void infer(Node *file)
{
    Inferstate st = {0,};

    assert(file->type == Nfile);
    st.delayed = mkht(tyhash, tyeq);
    /* set up the symtabs */
    loaduses(file);
    //mergeexports(&st, file);

    /* do the inference */
    applytraits(&st, file);
    infernode(&st, &file, NULL, NULL);
    postcheck(&st, file);

    /* and replace type vars with actual types */
    typesub(&st, file);
    specialize(&st, file);
}

