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
    int inpat;
    int ingeneric;
    int sawret;
    Type *ret;

    /* bound by patterns turn into decls in the action block */
    Node **binds;
    size_t nbinds;
    /* nodes that need post-inference checking/unification */
    Node **postcheck;
    size_t npostcheck;
    /* the type parmas bound at the current point */
    Htab **tybindings;
    size_t ntybindings;
    /* generic declarations to be specialized */
    Node **genericdecls;
    size_t ngenericdecls;
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
static Type *tf(Inferstate *st, Type *t);

static void setsuper(Stab *st, Stab *super)
{
    Stab *s;

    /* verify that we don't accidentally create loops */
    for (s = super; s; s = s->super)
        assert(s->super != st);
    st->super = super;
}

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

static Type *tyfreshen(Inferstate *st, Htab *ht, Type *t)
{
    Type *ret;
    size_t i;

    t = tf(st, t);
    if (t->type != Typaram && t->nsub == 0)
        return t;

    if (t->type == Typaram) {
        if (hthas(ht, t->pname))
            return htget(ht, t->pname);
        ret = mktyvar(t->line);
        htput(ht, t->pname, ret);
        return ret;
    }

    ret = tydup(t);
    for (i = 0; i < t->nsub; i++)
        ret->sub[i] = tyfreshen(st, ht, t->sub[i]);
    return ret;
}

static Type *freshen(Inferstate *st, Type *t)
{
    Htab *ht;

    ht = mkht(strhash, streq);
    t = tyfreshen(st, ht, t);
    htfree(ht);
    return t;
}

/* prevents types that directly contain themselves. */
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
            for (i = 0; i < t->nmemb; i++) {
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

static void tyresolve(Inferstate *st, Type *t)
{
    size_t i;
    Type *base;

    if (t->resolved)
        return;
    t->resolved = 1;
    if (t->type == Tystruct) {
        for (i = 0; i < t->nmemb; i++)
            infernode(st, t->sdecls[i], NULL, NULL);
    } else if (t->type == Tyunion) {
        for (i = 0; i < t->nmemb; i++) {
            tyresolve(st, t->udecls[i]->utype);
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

/* fixd the most accurate type mapping we have */
static Type *tf(Inferstate *st, Type *t)
{
    Type *lu;

    assert(t != NULL);
    lu = NULL;
    while (1) {
        if (!tytab[t->tid] && t->type == Tyunres) {
            if (!(lu = gettype(curstab(), t->name)))
                fatal(t->name->line, "Could not fixed type %s", namestr(t->name));
            tytab[t->tid] = lu;
        }

        if (!tytab[t->tid])
            break;
        t = tytab[t->tid];
    }
    tyresolve(st, t);
    return t;
}

static void loaduses(Node *n)
{
    size_t i;

    /* uses only allowed at top level. Do we want to keep it this way? */
    for (i = 0; i < n->file.nuses; i++)
        readuse(n->file.uses[i], n->file.globls);
}

static void settype(Inferstate *st, Node *n, Type *t)
{
    t = tf(st, t);
    switch (n->type) {
        case Nexpr:     n->expr.type = t;       break;
        case Ndecl:     n->decl.type = t;       break;
        case Nlit:      n->lit.type = t;        break;
        case Nfunc:     n->func.type = t;       break;
        default:
            die("can't set type of %s", nodestr(n->type));
            break;
    }
}

static Type *littype(Node *n)
{
    if (n->lit.type)
        return n->lit.type;
    switch (n->lit.littype) {
        case Lchr:      return mkty(n->line, Tychar);                           break;
        case Lbool:     return mkty(n->line, Tybool);                           break;
        case Lint:      return tylike(mktyvar(n->line), Tyint);                 break;
        case Lflt:      return tylike(mktyvar(n->line), Tyfloat32);             break;
        case Lstr:      return mktyslice(n->line, mkty(n->line, Tybyte));       break;
        case Lfunc:     return n->lit.fnval->func.type;                         break;
        case Lseq:      return NULL; break;
    };
    die("Bad lit type %d", n->lit.littype);
    return NULL;
}

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
        die("untypeable %s", nodestr(n->type));
        break;
    };
    return tf(st, t);
}

static char *ctxstr(Inferstate *st, Node *n)
{
    char *s;
    char *t;
    char *u;
    char buf[512];

    switch (n->type) {
        default:
            s = nodestr(n->type);
            break;
        case Ndecl:
            u = declname(n);
            t = tystr(tf(st, decltype(n)));
            snprintf(buf, 512, "%s:%s", u, t);
            s = strdup(buf);
            free(t);
            break;
        case Nname:
            s = namestr(n);
            break;
        case Nexpr:
            if (exprop(n) == Ovar)
                u = namestr(n->expr.args[0]);
            else
                u = opstr(exprop(n));
            if (exprtype(n))
                t = tystr(tf(st, exprtype(n)));
            else
                t = strdup("unknown");
            snprintf(buf, 512, "%s:%s", u, t);
            s = strdup(buf);
            free(t);
            break;
    }
    return s;
}

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
    return bsissubset(b->cstrs, a->cstrs);
}

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

static int tyrank(Type *t)
{
    if (t->type == Tyvar && t->nsub == 0)
        return 0;
    if (t->type == Tyvar && t->nsub > 0)
        return 1;
    else
        return 2;
}

static Type *unify(Inferstate *st, Node *ctx, Type *a, Type *b)
{
    Type *t;
    Type *r;
    size_t i;

    /* a ==> b */
    a = tf(st, a);
    b = tf(st, b);
    if (a == b)
        return a;
    if (tyrank(b) < tyrank(a)) {
        t = a;
        a = b;
        b = t;
    }

    r = NULL;
    mergecstrs(st, ctx, a, b);
    if (a->type == Tyvar) {
        tytab[a->tid] = b;
        r = b;
    }
    if (a->type == Tyvar && b->type != Tyvar) 
        if (occurs(a, b))
            fatal(ctx->line, "Infinite type %s in %s near %s",
                  tystr(a), tystr(b), ctxstr(st, ctx));

    /* if the tyrank of a is 0 (ie, a raw tyvar), we just unify, and don't
     * try to match up the nonexistent subtypes */
    if ((a->type == b->type || idxhacked(a, b)) && tyrank(a) != 0) {
        for (i = 0; i < b->nsub; i++) {
            /* types must have same arity */
            if (i >= a->nsub)
                fatal(ctx->line, "%s has wrong subtypes for %s near %s",
                      tystr(a), tystr(b), ctxstr(st, ctx));

            unify(st, ctx, a->sub[i], b->sub[i]);
        }
        r = b;
    } else if (a->type != Tyvar) {
        fatal(ctx->line, "%s incompatible with %s near %s",
              tystr(a), tystr(b), ctxstr(st, ctx));
    }
    return r;
}

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
        if (ft->sub[i]->type == Tyvalist) {
            i++; /* to prevent triggering the arity mismatch on exit */
            break;
        }
        inferexpr(st, n->expr.args[i], NULL, NULL);
        unify(st, n->expr.args[0], ft->sub[i], type(st, n->expr.args[i]));
    }
    if (i < ft->nsub)
        fatal(n->line, "%s arity mismatch (expected %zd args, got %zd)",
              ctxstr(st, n->expr.args[0]), ft->nsub - 1, n->expr.nargs - 1);
    settype(st, n, ft->sub[0]);
}

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
        settype(st, var, freshen(st, s->decl.type));
    else
        settype(st, var, s->decl.type);
    if (s->decl.isgeneric) {
        lappend(&st->specializationscope, &st->nspecializationscope, curstab());
        lappend(&st->specializations, &st->nspecializations, var);
        lappend(&st->genericdecls, &st->ngenericdecls, s);
    }
    *ret = var;
}

static void inferseq(Inferstate *st, Node *n)
{
    size_t i;

    for (i = 0; i < n->lit.nelt; i++) {
        infernode(st, n->lit.seqval[i], NULL, NULL);
        unify(st, n, type(st, n->lit.seqval[0]), type(st, n->lit.seqval[i]));
    }
    settype(st, n, mktyarray(n->line, type(st, n->lit.seqval[0]), mkintlit(n->line, n->lit.nelt)));
}

static void inferexpr(Inferstate *st, Node *n, Type *ret, int *sawret)
{
    Node **args;
    Type **types;
    size_t i, nargs;
    Ucon *uc;
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
            settype(st, n, mkty(-1, Tybool));
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
            settype(st, n, tylike(mktyvar(n->line), Tyuint));
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
                t =  unify(st, n, mkty(-1, Tyvoid), ret);
            settype(st, n, t);
            break;
        case Ojmp:     /* goto void* -> void */
            settype(st, n, mkty(-1, Tyvoid));
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
                t = freshen(st, s->decl.type);
            else
                t = s->decl.type;
            settype(st, n, t);
            n->expr.did = s->decl.did;
            if (s->decl.isgeneric) {
                lappend(&st->specializationscope, &st->nspecializationscope, curstab());
                lappend(&st->specializations, &st->nspecializations, n);
                lappend(&st->genericdecls, &st->ngenericdecls, s);
            }
            break;
        case Ocons:
            uc = getucon(curstab(), args[0]);
            if (!uc)
                fatal(n->line, "No union constructor %s", ctxstr(st, args[0]));
            if (!uc->etype && n->expr.nargs > 1)
                fatal(n->line, "nullary union constructor %s passed arg ", ctxstr(st, args[0]));
            else if (uc->etype && n->expr.nargs != 2)
                fatal(n->line, "union constructor %s needs arg ", ctxstr(st, args[0]));
            else if (uc->etype)
                unify(st, n, uc->etype, type(st, args[1]));
            settype(st, n, uc->utype);
            break;
        case Otup:
            types = xalloc(sizeof(Type *)*n->expr.nargs);
            for (i = 0; i < n->expr.nargs; i++)
                types[i] = type(st, n->expr.args[i]);
            settype(st, n, mktytuple(n->line, types, n->expr.nargs));
            break;
        case Oarr:
            for (i = 0; i < n->expr.nargs; i++)
                unify(st, n, type(st, n->expr.args[0]), type(st, n->expr.args[i]));
            settype(st, n, mktyarray(n->line, type(st, n->expr.args[0]), mkintlit(n->line, n->expr.nargs)));
            break;
        case Olit:      /* <lit>:@a::tyclass -> @a */
            switch (args[0]->lit.littype) {
                case Lfunc:     infernode(st, args[0]->lit.fnval, NULL, NULL); break;
                case Lseq:      inferseq(st, args[0]);                         break;
                default:        /* pass */                                 break;
            }
            settype(st, n, type(st, args[0]));
            break;
        case Olbl:      /* :lbl -> void* */
            settype(st, n, mktyptr(n->line, mkty(-1, Tyvoid)));
        case Obad: case Ocjmp:
        case Oload: case Oset:
        case Oslbase: case Osllen:
        case Oblit: case Numops:
        case Otrunc: case Oswiden: case Ozwiden:
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
        unify(st, n, type(st, n)->sub[0], mkty(-1, Tyvoid));
}

static void inferdecl(Inferstate *st, Node *n)
{
    Type *t;

    t = tf(st, decltype(n));
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
        t = tf(st, gettype(s, k[i]));
        updatetype(s, k[i], t);
    }
    free(k);
}

static void tybind(Inferstate *st, Htab *bt, Type *t)
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
    for (i = 0; i < t->nsub; i++)
        tybind(st, bt, t->sub[i]);
}

static void bind(Inferstate *st, Node *n)
{
    Htab *bt;

    if (!n->decl.isgeneric)
        return;
    if (!n->decl.init)
        fatal(n->line, "generic %s has no initializer", n->decl);

    bt = mkht(strhash, streq);
    lappend(&st->tybindings, &st->ntybindings, bt);

    tybind(st, bt, n->decl.type);
    tybind(st, bt, n->decl.init->expr.type);
}

static void unbind(Inferstate *st, Node *n)
{
    if (!n->decl.isgeneric)
        return;
    htfree(st->tybindings[st->ntybindings - 1]);
    lpop(&st->tybindings, &st->ntybindings);
}

static void infernode(Inferstate *st, Node *n, Type *ret, int *sawret)
{
    size_t i;
    Node *d;
    Node *s;

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
            if (n->decl.isgeneric)
                st->ingeneric++;
            bind(st, n);
            inferdecl(st, n);
            unbind(st, n);
            if (type(st, n)->type == Typaram && !st->ingeneric)
                fatal(n->line, "Generic type %s in non-generic near %s\n", tystr(type(st, n)), ctxstr(st, n));
            if (n->decl.isgeneric)
                st->ingeneric--;
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
            st->inpat++;
            infernode(st, n->match.pat, NULL, sawret);
            st->inpat--;
            infernode(st, n->match.block, ret, sawret);
            break;
        case Nexpr:
            inferexpr(st, n, ret, sawret);
            break;
        case Nfunc:
            setsuper(n->func.scope, curstab());
            if (st->ntybindings > 0)
                for (i = 0; i < n->func.nargs; i++)
                    tybind(st, st->tybindings[st->ntybindings - 1], n->func.args[i]->decl.type);
            pushstab(n->func.scope);
            inferstab(st, n->block.scope);
            inferfunc(st, n);
            popstab();
            break;
        case Nname:
        case Nlit:
        case Nuse:
        case Nlbl:
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
}

static void checkcast(Inferstate *st, Node *n)
{
    /* FIXME: actually verify the casts */
}

/* returns the final type for t, after all unifications
 * and default constraint selections */
static Type *tyfix(Inferstate *st, Node *ctx, Type *t)
{
    static Type *tyint, *tyflt;
    size_t i;
    char buf[1024];

    if (!tyint)
        tyint = mkty(-1, Tyint);
    if (!tyflt)
        tyflt = mkty(-1, Tyfloat64);

    t = tf(st, t);
    if (t->type == Tyvar) {
        if (hascstr(t, cstrtab[Tcint]) && cstrcheck(t, tyint))
            return tyint;
        if (hascstr(t, cstrtab[Tcfloat]) && cstrcheck(t, tyflt))
            return tyint;
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
        }
        for (i = 0; i < t->nsub; i++)
            t->sub[i] = tyfix(st, ctx, t->sub[i]);
    }
    if (t->type == Tyvar) {
        fatal(t->line, "underconstrained type %s near %s", tyfmt(buf, 1024, t), ctxstr(st, ctx));
    }

    return t;
}

static void infercompn(Inferstate *st, Node *file)
{
    size_t i, j;
    Node *aggr;
    Node *memb;
    Node *n;
    Node **nl;
    Type *t;
    int found;

    for (i = 0; i < st->npostcheck; i++) {
        n = st->postcheck[i];
        if (exprop(n) != Omemb)
            continue;
        aggr = st->postcheck[i]->expr.args[0];
        memb = st->postcheck[i]->expr.args[1];

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
            for (j = 0; j < t->nmemb; j++) {
                if (!strcmp(namestr(memb), declname(nl[j]))) {
                    unify(st, n, type(st, n), decltype(nl[j]));
                    found = 1;
                    break;
                }
            }
        }
        if (!found)
            fatal(aggr->line, "Type %s has no member \"%s\" near %s",
                  tystr(type(st, aggr)), ctxstr(st, memb), ctxstr(st, aggr));
    }
}

static void stabsub(Inferstate *st, Stab *s)
{
    void **k;
    size_t n, i;
    Type *t;
    Node *d;

    k = htkeys(s->ty, &n);
    for (i = 0; i < n; i++) {
        t = tf(st, gettype(s, k[i]));
        updatetype(s, k[i], t);
    }
    free(k);

    k = htkeys(s->dcl, &n);
    for (i = 0; i < n; i++) {
        d = getdcl(s, k[i]);
        d->decl.type = tyfix(st, d, d->decl.type);
    }
    free(k);
}

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
            for (i = 0; i < n->matchstmt.nmatches; i++)
                typesub(st, n->matchstmt.matches[i]);
            break;
        case Nmatch:
            typesub(st, n->match.pat);
            typesub(st, n->match.block);
            break;
        case Nexpr:
            settype(st, n, tyfix(st, n, type(st, n)));
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
                case Lfunc:     typesub(st, n->lit.fnval); break;
                case Lseq:
                    for (i = 0; i < n->lit.nelt; i++)
                        typesub(st, n->lit.seqval[i]);
                    break;
                default:        break;
            }
            break;
        case Nname:
        case Nuse:
        case Nlbl:
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
}

static void mergeexports(Inferstate *st, Node *file)
{
    Stab *exports, *globls;
    size_t i, nk;
    void **k;
    /* local, global version */
    Node *nl, *ng;
    Type *tl, *tg;

    exports = file->file.exports;
    globls = file->file.globls;

    pushstab(globls);
    k = htkeys(exports->ty, &nk);
    for (i = 0; i < nk; i++) {
        tl = gettype(exports, k[i]);
        nl = k[i];
        if (tl) {
            tg = gettype(globls, nl);
            if (!tg)
                puttype(globls, nl, tl);
            else
                fatal(nl->line, "Exported type %s already declared on line %d", namestr(nl), tg->line);
        } else {
            tg = gettype(globls, nl);
            if (tg)
                updatetype(exports, nl, tf(st, tg));
            else
                fatal(nl->line, "Exported type %s not declared", namestr(nl));
        }
    }
    free(k);

    k = htkeys(exports->dcl, &nk);
    for (i = 0; i < nk; i++) {
        nl = getdcl(exports, k[i]);
        ng = getdcl(globls, k[i]);
        /* if an export has an initializer, it shouldn't be declared in the
         * body */
        if (nl->decl.init && ng)
            fatal(nl->line, "Export %s double-defined on line %d", ctxstr(st, nl), ng->line);
        if (!ng)
            putdcl(globls, nl);
        else
            unify(st, nl, type(st, ng), type(st, nl));
    }
    free(k);
    popstab();
}

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
    loaduses(file);
    mergeexports(&st, file);
    infernode(&st, file, NULL, NULL);
    infercompn(&st, file);
    checkcast(&st, file);
    typesub(&st, file);
    specialize(&st, file);
}
