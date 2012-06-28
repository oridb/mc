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

static Node **postcheck;
static size_t npostcheck;
static Htab **tybindings;
static size_t ntybindings;
static Node **genericdecls;
static size_t ngenericdecls;
static Node **specializations;
static Node **specializations;
static size_t nspecializations;
static Stab **specializationscope;
static size_t nspecializationscope;

static void infernode(Node *n, Type *ret, int *sawret);
static void inferexpr(Node *n, Type *ret, int *sawret);
static void typesub(Node *n);
static Type *tf(Type *t);

static void setsuper(Stab *st, Stab *super)
{
    Stab *s;

    /* verify that we don't accidentally create loops */
    for (s = super; s; s = s->super)
        assert(s->super != st);
    st->super = super;
}

static int isbound(Type *t)
{
    ssize_t i;
    Type *p;

    for (i = ntybindings - 1; i >= 0; i--) {
        p = htget(tybindings[i], t->pname);
        if (p == t)
            return 1;
    }
    return 0;
}

static Type *tyfreshen(Htab *ht, Type *t)
{
    Type *ret;
    size_t i;

    t = tf(t);
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
        ret->sub[i] = tyfreshen(ht, t->sub[i]);
    return ret;
}

static Type *freshen(Type *t)
{
    Htab *ht;

    ht = mkht(strhash, streq);
    t = tyfreshen(ht, t);
    htfree(ht);
    return t;
}

static void tyresolve(Type *t)
{
    size_t i;
    Type *base;

    if (t->resolved)
        return;
    t->resolved = 1;
    if (t->type == Tystruct) {
        for (i = 0; i < t->nmemb; i++)
            infernode(t->sdecls[i], NULL, NULL);
    } else if (t->type == Tyunion) {
        for (i = 0; i < t->nmemb; i++) {
            tyresolve(t->udecls[i]->utype);
            t->udecls[i]->utype = tf(t->udecls[i]->utype);
            if (t->udecls[i]->etype) {
                tyresolve(t->udecls[i]->etype);
                t->udecls[i]->etype = tf(t->udecls[i]->etype);
            }
        }
    } else if (t->type == Tyarray) {
        infernode(t->asize, NULL, NULL);
    }

    for (i = 0; i < t->nsub; i++)
        t->sub[i] = tf(t->sub[i]);
    base = tybase(t);
    /* no-ops if base == t */
    if (t->cstrs)
        bsunion(t->cstrs, base->cstrs);
    else
        t->cstrs = bsdup(base->cstrs);
}

/* fixd the most accurate type mapping we have */
static Type *tf(Type *t)
{
    Type *lu;

    assert(t != NULL);
    lu = NULL;
    while (1) {
        if (!tytab[t->tid] && t->type == Tyname) {
            if (!(lu = gettype(curstab(), t->name)))
                fatal(t->name->line, "Could not fixd type %s", namestr(t->name));
            tytab[t->tid] = lu;
        }

        if (!tytab[t->tid])
            break;
        t = tytab[t->tid];
    }
    tyresolve(t);
    return t;
}

static void loaduses(Node *n)
{
    size_t i;

    /* uses only allowed at top level. Do we want to keep it this way? */
    for (i = 0; i < n->file.nuses; i++)
        readuse(n->file.uses[i], n->file.globls);
}

static void settype(Node *n, Type *t)
{
    t = tf(t);
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
    switch (n->lit.littype) {
        case Lchr:      return mkty(n->line, Tychar);                           break;
        case Lbool:     return mkty(n->line, Tybool);                           break;
        case Lint:      return tylike(mktyvar(n->line), Tyint);                 break;
        case Lflt:      return tylike(mktyvar(n->line), Tyfloat32);             break;
        case Lstr:      return mktyslice(n->line, mkty(n->line, Tychar));       break;
        case Lfunc:     return n->lit.fnval->func.type;                         break;
        case Larray:    return NULL; break;
    };
    return NULL;
}

static Type *type(Node *n)
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
    return tf(t);
}

static char *ctxstr(Node *n)
{
    char *s;
    switch (n->type) {
        default:        s = nodestr(n->type);   break;
        case Ndecl:     s = declname(n);        break;
        case Nname:     s = namestr(n);         break;
        case Nexpr:
            if (exprop(n) == Ovar)
                s = namestr(n->expr.args[0]);
            else
                s = opstr(exprop(n));
            break;
    }
    return s;
}

static void constrain(Node *ctx, Type *a, Cstr *c)
{
    if (a->type == Tyvar) {
        if (!a->cstrs)
            a->cstrs = mkbs();
        setcstr(a, c);
    } else if (!bshas(a->cstrs, c->cid)) {
            fatal(ctx->line, "%s needs %s near %s", tystr(a), c->name, ctxstr(ctx));
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

static void mergecstrs(Node *ctx, Type *a, Type *b)
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
            dump(file, stdout);
            fatal(ctx->line, "%s missing constraints for %s near %s", tystr(b), tystr(a), ctxstr(ctx));
        }
    }
}

static int idxhacked(Type **pa, Type **pb)
{
    Type *a, *b;

    a = *pa;
    b = *pb;
    /* we want to unify Tyidxhack => concrete indexable. Flip 
     * to make this happen, if needed */
    if (b->type == Tyvar && b->nsub > 0) {
        *pb = a;
        *pa = b;
    }
    return (a->type == Tyvar && a->nsub > 0) || a->type == Tyarray || a->type == Tyslice;
}

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

static Type *unify(Node *ctx, Type *a, Type *b)
{
    Type *t;
    Type *r;
    size_t i;

    /* a ==> b */
    a = tf(a);
    b = tf(b);
    if (a == b)
        return a;
    if (b->type == Tyvar) {
        t = a;
        a = b;
        b = t;
    }

    r = NULL;
    mergecstrs(ctx, a, b);
    if (a->type == Tyvar) {
        tytab[a->tid] = b;
        r = b;
    }
    if (a->type == Tyvar && b->type != Tyvar) 
        if (occurs(a, b))
            fatal(ctx->line, "Infinite type %s in %s near %s", tystr(a), tystr(b), ctxstr(ctx));

    if (a->type == b->type || idxhacked(&a, &b)) {
        for (i = 0; i < b->nsub; i++) {
            /* types must have same arity */
            if (i >= a->nsub)
                fatal(ctx->line, "%s has wrong subtypes for %s near %s", tystr(a), tystr(b), ctxstr(ctx));

            unify(ctx, a->sub[i], b->sub[i]);
        }
        r = b;
    } else if (a->type != Tyvar) {
        fatal(ctx->line, "%s incompatible with %s near %s", tystr(a), tystr(b), ctxstr(ctx));
    }
    return r;
}

static void unifycall(Node *n)
{
    size_t i;
    Type *ft;

    ft = type(n->expr.args[0]);
    if (ft->type == Tyvar) {
        /* the first arg is the function itself, so it shouldn't be counted */
        ft = mktyfunc(n->line, &n->expr.args[1], n->expr.nargs - 1, mktyvar(n->line));
    }
    for (i = 1; i < n->expr.nargs; i++) {
        if (ft->sub[i]->type == Tyvalist)
            break;
        inferexpr(n->expr.args[i], NULL, NULL);
        unify(n, ft->sub[i], type(n->expr.args[i]));
    }
    settype(n, ft->sub[0]);
}

static void checkns(Node *n, Node **ret)
{
    Node *var, *name, *nsname;
    Node **args;
    Stab *st;
    Node *s;

    if (n->type != Nexpr)
        return;
    if (!n->expr.nargs)
        return;
    args = n->expr.args;
    if (args[0]->type != Nexpr || exprop(args[0]) != Ovar)
        return;
    name = args[0]->expr.args[0];
    st = getns(curstab(), name);
    if (!st)
        return;
    nsname = mknsname(n->line, namestr(name), namestr(args[1]));
    s = getdcl(st, args[1]);
    if (!s)
        fatal(n->line, "Undeclared var %s.%s", nsname->name.ns, nsname->name.name);
    var = mkexpr(n->line, Ovar, nsname, NULL);
    var->expr.did = s->decl.did;
    settype(var, s->decl.type);
    *ret = var;
}

static void inferexpr(Node *n, Type *ret, int *sawret)
{
    Node **args;
    Ucon *uc;
    int nargs;
    Node *s;
    Type *t;
    int i;

    assert(n->type == Nexpr);
    args = n->expr.args;
    nargs = n->expr.nargs;
    for (i = 0; i < nargs; i++) {
        /* Nlit, Nvar, etc should not be inferred as exprs */
        if (args[i]->type == Nexpr) {
            /* Omemb can sometimes resolve to a namespace. We have to check
             * this. Icky. */
            if (exprop(args[i]) == Omemb)
                checkns(args[i], &args[i]);
            inferexpr(args[i], ret, sawret);
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
            t = type(args[0]);
            for (i = 1; i < nargs; i++)
                t = unify(n, t, type(args[i]));
            settype(n, tf(t));
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
            t = type(args[0]);
            for (i = 1; i < nargs; i++)
                unify(n, t, type(args[i]));
            settype(n, mkty(-1, Tybool));
            break;

        /* reach into a type and pull out subtypes */
        case Oaddr:     /* &@a -> @a* */
            settype(n, mktyptr(n->line, type(args[0])));
            break;
        case Oderef:    /* *@a* ->  @a */
            t = unify(n, type(args[0]), mktyptr(n->line, mktyvar(n->line)));
            settype(n, t);
            break;
        case Oidx:      /* @a[@b::tcint] -> @a */
            t = mktyidxhack(n->line, mktyvar(n->line));
            unify(n, type(args[0]), t);
            constrain(n, type(args[1]), cstrtab[Tcint]);
            settype(n, tf(t->sub[0]));
            break;
        case Oslice:    /* @a[@b::tcint,@b::tcint] -> @a[,] */
            t = mktyidxhack(n->line, type(args[1]));
            unify(n, type(args[0]), t);
            settype(n, mktyslice(n->line, type(args[1])));
            break;

        /* special cases */
        case Omemb:     /* @a.Ident -> @b, verify type(@a.Ident)==@b later */
            settype(n, mktyvar(n->line));
            lappend(&postcheck, &npostcheck, n);
            break;
        case Osize:     /* sizeof @a -> size */
            settype(n, mkty(n->line, Tyuint));
            break;
        case Ocall:     /* (@a, @b, @c, ... -> @r)(@a,@b,@c, ... -> @r) -> @r */
            unifycall(n);
            break;
        case Ocast:     /* cast(@a, @b) -> @b */
            lappend(&postcheck, &npostcheck, n);
            break;
        case Oret:      /* -> @a -> void */
            if (sawret)
                *sawret = 1;
            if (!ret)
                fatal(n->line, "Not allowed to return value here");
            if (nargs)
                t = unify(n, ret, type(args[0]));
            else
                t =  unify(n, mkty(-1, Tyvoid), ret);
            settype(n, t);
            break;
        case Ojmp:     /* goto void* -> void */
            settype(n, mkty(-1, Tyvoid));
            break;
        case Ovar:      /* a:@a -> @a */
            /* if we created this from a namespaced var, the type should be
             * set, and the normal lookup is expected to fail. Since we're
             * already done with this node, we can just return. */
            if (n->expr.type)
                return;
            s = getdcl(curstab(), args[0]);
            if (!s)
                fatal(n->line, "Undeclared var %s", ctxstr(args[0]));

            if (s->decl.isgeneric)
                t = freshen(s->decl.type);
            else
                t = s->decl.type;
            settype(n, t);
            n->expr.did = s->decl.did;
            if (s->decl.isgeneric) {
                lappend(&specializationscope, &nspecializationscope, curstab());
                lappend(&specializations, &nspecializations, n);
                lappend(&genericdecls, &ngenericdecls, s);
            }
            break;
        case Ocons:
            uc = getucon(curstab(), args[0]);
            if (!uc)
                fatal(n->line, "No union constructor %s", namestr(n));
            if (!uc->etype && n->expr.nargs > 1)
                fatal(n->line, "nullary union constructor %s passed arg ", ctxstr(args[0]));
            else if (uc->etype && n->expr.nargs != 2)
                fatal(n->line, "union constructor %s needs arg ", ctxstr(args[0]));
            else
                unify(n, uc->etype, type(args[1]));
            settype(n, uc->utype);
            break;
        case Olit:      /* <lit>:@a::tyclass -> @a */
            switch (args[0]->lit.littype) {
                case Lfunc: infernode(args[0]->lit.fnval, NULL, NULL); break;
                case Larray: die("array types not implemented yet"); break;
                default: break;
            }
            settype(n, type(args[0]));
            break;
        case Olbl:      /* :lbl -> void* */
            settype(n, mktyptr(n->line, mkty(-1, Tyvoid)));
        case Obad: case Ocjmp:
        case Oload: case Ostor:
        case Oslbase: case Osllen:
        case Oblit: case Numops:
            die("Should not see %s in fe", opstr(exprop(n)));
            break;
    }
}

static void inferfunc(Node *n)
{
    size_t i;
    int sawret;

    sawret = 0;
    for (i = 0; i < n->func.nargs; i++)
        infernode(n->func.args[i], NULL, NULL);
    infernode(n->func.body, n->func.type->sub[0], &sawret);
    /* if there's no return stmt in the function, assume void ret */
    if (!sawret)
        unify(n, type(n)->sub[0], mkty(-1, Tyvoid));
}

static void inferdecl(Node *n)
{
    Type *t;

    t = tf(decltype(n));
    settype(n, t);
    if (n->decl.init) {
        inferexpr(n->decl.init, NULL, NULL);
        unify(n, type(n), type(n->decl.init));
    } else {
        if (n->decl.isconst && !n->decl.isextern)
            fatal(n->line, "non-extern \"%s\" has no initializer", ctxstr(n));
    }
}

static void inferstab(Stab *s)
{
    void **k;
    size_t n, i;
    Type *t;

    k = htkeys(s->ty, &n);
    for (i = 0; i < n; i++) {
        t = tf(gettype(s, k[i]));
        updatetype(s, k[i], t);
    }
    free(k);
}

static void tybind(Htab *bt, Type *t)
{
    size_t i;

    if (!t)
        return;
    if (t->type != Typaram)
        return;

    if (hthas(bt, t->pname))
        unify(NULL, htget(bt, t->pname), t);
    else if (isbound(t))
        return;

    htput(bt, t->pname, t);
    for (i = 0; i < t->nsub; i++)
        tybind(bt, t->sub[i]);
}

static void bind(Node *n)
{
    Htab *bt;

    if (!n->decl.isgeneric)
        return;
    if (!n->decl.init)
        fatal(n->line, "generic %s has no initializer", n->decl);

    bt = mkht(strhash, streq);
    lappend(&tybindings, &ntybindings, bt);

    tybind(bt, n->decl.type);
    tybind(bt, n->decl.init->expr.type);
}

static void unbind(Node *n)
{
    if (!n->decl.isgeneric)
        return;
    htfree(tybindings[ntybindings - 1]);
    lpop(&tybindings, &ntybindings);
}

static void infernode(Node *n, Type *ret, int *sawret)
{
    size_t i;
    Node *d;
    Node *s;

    if (!n)
        return;
    switch (n->type) {
        case Nfile:
            pushstab(n->file.globls);
            /* exports allow us to specify types later in the body, so we
             * need to patch the types in if they don't have a definition */
            inferstab(n->file.globls);
            inferstab(n->file.exports);
            for (i = 0; i < n->file.nstmts; i++) {
                d  = n->file.stmts[i];
                infernode(d, NULL, sawret);
                if (d->type == Ndecl)  {
                    s = getdcl(file->file.exports, d->decl.name);
                    if (s)
                        unify(d, type(d), s->decl.type);
                }
            }
            popstab();
            break;
        case Ndecl:
            bind(n);
            inferdecl(n);
            unbind(n);
            break;
        case Nblock:
            setsuper(n->block.scope, curstab());
            pushstab(n->block.scope);
            inferstab(n->block.scope);
            for (i = 0; i < n->block.nstmts; i++) {
                checkns(n->block.stmts[i], &n->block.stmts[i]);
                infernode(n->block.stmts[i], ret, sawret);
            }
            popstab();
            break;
        case Nifstmt:
            infernode(n->ifstmt.cond, NULL, sawret);
            infernode(n->ifstmt.iftrue, ret, sawret);
            infernode(n->ifstmt.iffalse, ret, sawret);
            constrain(n, type(n->ifstmt.cond), cstrtab[Tctest]);
            break;
        case Nloopstmt:
            infernode(n->loopstmt.init, ret, sawret);
            infernode(n->loopstmt.cond, NULL, sawret);
            infernode(n->loopstmt.step, ret, sawret);
            infernode(n->loopstmt.body, ret, sawret);
            constrain(n, type(n->loopstmt.cond), cstrtab[Tctest]);
            break;
        case Nexpr:
            inferexpr(n, ret, sawret);
            break;
        case Nfunc:
            setsuper(n->func.scope, curstab());
            if (ntybindings > 0)
                for (i = 0; i < n->func.nargs; i++)
                    tybind(tybindings[ntybindings - 1], n->func.args[i]->decl.type);
            pushstab(n->func.scope);
            inferstab(n->block.scope);
            inferfunc(n);
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

static void checkcast(Node *n)
{
}

/* returns the final type for t, after all unifications
 * and default constraint selections */
static Type *tyfix(Node *ctx, Type *t)
{
    static Type *tyint;
    size_t i;
    char buf[1024];

    if (!tyint)
        tyint = mkty(-1, Tyint);

    t = tf(t);
    if (t->type == Tyvar) {
        if (hascstr(t, cstrtab[Tcint]) || cstrcheck(t, tyint))
            return tyint;
    } else {
        if (t->type == Tyarray) {
            typesub(t->asize);
        } else if (t->type == Tystruct) {
            for (i = 0; i < t->nmemb; i++)
                typesub(t->sdecls[i]);
        } else if (t->type == Tyunion) {
            for (i = 0; i < t->nmemb; i++) {
                if (t->udecls[i]->etype)
                    t->udecls[i]->etype = tyfix(ctx, t->udecls[i]->etype);
            }
        }
        for (i = 0; i < t->nsub; i++)
            t->sub[i] = tyfix(ctx, t->sub[i]);
    }
    if (t->type == Tyvar) {
        fatal(t->line, "underconstrained type %s near %s", tyfmt(buf, 1024, t), ctxstr(ctx));
    }

    return t;
}

static void infercompn(Node *file)
{
    size_t i, j;
    Node *aggr;
    Node *memb;
    Node *n;
    Node **nl;
    Type *t;
    int found;

    for (i = 0; i < npostcheck; i++) {
        n = postcheck[i];
        if (exprop(n) != Omemb)
            continue;
        if (type(n)->type == Typtr)
            n = n->expr.args[0];
        aggr = postcheck[i]->expr.args[0];
        memb = postcheck[i]->expr.args[1];

        found = 0;
        t = tf(type(aggr));
        if (t->type == Tyslice || t->type == Tyarray) {
            if (!strcmp(namestr(memb), "len")) {
                constrain(n, type(n), cstrtab[Tcnum]);
                constrain(n, type(n), cstrtab[Tcint]);
                constrain(n, type(n), cstrtab[Tctest]);
                found = 1;
            }
        } else {
            t = tybase(t);
            if (t->type == Typtr)
                t = tf(t->sub[0]);
            nl = t->sdecls;
            for (j = 0; j < t->nmemb; j++) {
                if (!strcmp(namestr(memb), declname(nl[j]))) {
                    unify(n, type(n), decltype(nl[j]));
                    found = 1;
                    break;
                }
            }
        }
        if (!found)
            fatal(aggr->line, "Type %s has no member \"%s\" near %s",
                  tystr(type(aggr)), ctxstr(memb), ctxstr(aggr));
    }
}

static void stabsub(Stab *s)
{
    void **k;
    size_t n, i;
    Type *t;
    Node *d;

    k = htkeys(s->ty, &n);
    for (i = 0; i < n; i++) {
        t = tf(gettype(s, k[i]));
        updatetype(s, k[i], t);
    }
    free(k);

    k = htkeys(s->dcl, &n);
    for (i = 0; i < n; i++) {
        d = getdcl(s, k[i]);
        d->decl.type = tyfix(d->decl.name, d->decl.type);
    }
    free(k);
}

static void typesub(Node *n)
{
    size_t i;

    if (!n)
        return;
    switch (n->type) {
        case Nfile:
            pushstab(n->file.globls);
            stabsub(n->file.globls);
            stabsub(n->file.exports);
            for (i = 0; i < n->file.nstmts; i++)
                typesub(n->file.stmts[i]);
            popstab();
            break;
        case Ndecl:
            settype(n, tyfix(n, type(n)));
            if (n->decl.init)
                typesub(n->decl.init);
            break;
        case Nblock:
            pushstab(n->block.scope);
            for (i = 0; i < n->block.nstmts; i++)
                typesub(n->block.stmts[i]);
            popstab();
            break;
        case Nifstmt:
            typesub(n->ifstmt.cond);
            typesub(n->ifstmt.iftrue);
            typesub(n->ifstmt.iffalse);
            break;
        case Nloopstmt:
            typesub(n->loopstmt.cond);
            typesub(n->loopstmt.init);
            typesub(n->loopstmt.step);
            typesub(n->loopstmt.body);
            break;
        case Nexpr:
            settype(n, tyfix(n, type(n)));
            for (i = 0; i < n->expr.nargs; i++)
                typesub(n->expr.args[i]);
            break;
        case Nfunc:
            pushstab(n->func.scope);
            settype(n, tyfix(n, n->func.type));
            for (i = 0; i < n->func.nargs; i++)
                typesub(n->func.args[i]);
            typesub(n->func.body);
            popstab();
            break;
        case Nlit:
            settype(n, tyfix(n, type(n)));
            switch (n->lit.littype) {
                case Lfunc:     typesub(n->lit.fnval); break;
                case Larray:    typesub(n->lit.arrval); break;
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

static void mergeexports(Node *file)
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
                fatal(nl->line, "Exported type %s double-declared on line %d", namestr(nl), tg->line);
        } else {
            tg = gettype(globls, nl);
            if (tg) 
                updatetype(exports, nl, tf(tg));
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
            fatal(nl->line, "Export %s double-defined on line %d", ctxstr(nl), ng->line);
        if (!ng)
            putdcl(globls, nl);
        else
            unify(nl, type(ng), type(nl));
    }
    free(k);
    popstab();
}

static void specialize(Node *f)
{
    Node *d, *name;
    size_t i;

    for (i = 0; i < nspecializations; i++) {
        pushstab(specializationscope[i]);
        d = specializedcl(genericdecls[i], specializations[i]->expr.type, &name);
        specializations[i]->expr.args[0] = name;
        specializations[i]->expr.did = d->decl.did;
        popstab();
    }
}

void infer(Node *file)
{
    assert(file->type == Nfile);

    loaduses(file);
    mergeexports(file);
    infernode(file, NULL, NULL);
    infercompn(file);
    checkcast(file);
    typesub(file);
    specialize(file);
}
