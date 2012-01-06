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


/* find the most accurate type mapping */
static Type *tf(Type *t)
{
    assert(t != NULL);
    
    if (tytab[t->tid]) {
        printf ("%s => ", tystr(t));
        while (tytab[t->tid]) {
            t = tytab[t->tid];
            printf("%s => ", tystr(t));
        }
        printf("nil\n");
    }
    return t;
}

/* does b satisfy all the constraints of a? */
static int cstrcheck(Type *a, Type *b)
{
    Bitset *s;
    int n;

    /* a has no cstrs to satisfy */
    if (!a->cstrs)
        return 1;
    /* b satisfies no cstrs; only valid if a requires none */
    if (!b->cstrs)
        return bscount(a->cstrs) == 0;
    /* if b->cstrs \ a->cstrs == 0, then all of
     * a's constraints are satisfied. */
    s = dupbs(b->cstrs);
    bsdiff(s, a->cstrs);
    n = bscount(s);
    delbs(s);

    return n == 0;
}

static void loaduses(Node *n)
{
    int i;
    /* uses only allowed at top level. Do we want to keep it this way? */
    for (i = 0; i < n->file.nuses; i++)
        fprintf(stderr, "INTERNAL: implement use loading\n");
        /* readuse(n->file.uses[i], n->file.globls); */
}

/* a => b */
static void settype(Node *n, Type *t)
{
    switch (n->type) {
        case Nexpr:     n->expr.type = t;       break;
        case Ndecl:     n->decl.sym->type = t;  break;
        default:
            die("can't set type of %s", nodestr(n->type));
            break;
    }

}

static Op exprop(Node *e)
{
    assert(e->type == Nexpr);
    return e->expr.op;
}

static Type *littype(Node *n)
{
    switch (n->lit.littype) {
        case Lchr:      return mkty(n->line, Tychar);                           break;
        case Lbool:     return mkty(n->line, Tybool);                           break;
        case Lint:      return tylike(mktyvar(n->line), Tyint);                 break;
        case Lflt:      return tylike(mktyvar(n->line), Tyfloat32);             break;
        case Lstr:      return mktyslice(n->line, mkty(n->line, Tychar));       break;
        case Lfunc:     return NULL; break;
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
      default:
        t = NULL;
        die("untypeable %s", nodestr(n->type));
        break;
    };
    return tf(t);
}

static char *ctxstr(Node *n)
{
    return nodestr(n->type);
}

static void matchcstrs(Node *ctx, Type *a, Type *b)
{
    if (b->type == Tyvar) {
        if (!b->cstrs)
            b->cstrs = dupbs(a->cstrs);
        else
            bsunion(b->cstrs, a->cstrs);
    } else {
        if (!cstrcheck(b, a))
            fatal(ctx->line, "%s incompatible with %s near %s", tystr(a), tystr(b), ctxstr(ctx));
    }
}

static Type *unify(Node *ctx, Type *a, Type *b)
{
    Type *t;
    int i;

    /* a ==> b */
    a = tf(a);
    b = tf(b);
    if (b->type == Tyvar) {
        t = a;
        a = b;
        b = t;
    }

    matchcstrs(ctx, a, b);
    if (a->type != b->type && a->type != Tyvar)
        fatal(ctx->line, "%s incompatible with %s near %s", tystr(a), tystr(b), ctxstr(ctx));

    tytab[a->tid] = b;
    for (i = 0; i < b->nsub; i++) {
        /* types must have same arity */
        if (i >= a->nsub)
            fatal(ctx->line, "%s incompatible with %s near %s", tystr(a), tystr(b), ctxstr(ctx));

        /* FIXME: recurse properly.
        matchcstrs(ctx, a, b);
        unify(ctx, a->sub[i], b->sub[i]);
        */
    }
    return b;
}

static void unifycall(Node *n)
{
}

static void inferexpr(Node *n, Type *ret)
{
    Node **args;
    Sym *s;
    int nargs;
    Type *t;
    int i;

    assert(n->type == Nexpr);
    args = n->expr.args;
    nargs = n->expr.nargs;
    for (i = 0; i < nargs; i++)
        /* Nlit, Nvar, etc should not be inferred as exprs */
        if (args[i]->type == Nexpr)
            inferexpr(args[i], ret);
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
            die("inference of indexes not done yet");
            break;
        case Oslice:    /* @a[@b::tcint,@b::tcint] -> @a[,] */
            die("inference of slices not done yet");
            break;

        /* special cases */
        case Omemb:     /* @a.Ident -> @b, verify type(@a.Ident)==@b later */
            die("members not done yet");
            break;
        case Osize:     /* sizeof @a -> size */
            die("inference of sizes not done yet");
            break;
        case Ocall:     /* (@a, @b, @c, ... -> @r)(@a,@b,@c, ... -> @r) -> @r */
            unifycall(n);
            break;
        case Ocast:     /* cast(@a, @b) -> @b */
            die("casts not implemented");
            break;
        case Oret:      /* -> @a -> void */
            settype(n, mkty(-1, Tyvoid));
            break;
        case Ogoto:     /* goto void* -> void */
            settype(n, mkty(-1, Tyvoid));
            break;
        case Ovar:      /* a:@a -> @a */
            s = getdcl(curstab(), n);
            if (!s)
                fatal(n->line, "Undeclared var");
            else
                settype(n, s->type);
            break;
        case Olit:      /* <lit>:@a::tyclass -> @a */
            settype(n, type(args[0]));
            break;
        case Olbl:      /* :lbl -> void* */
            settype(n, mktyptr(n->line, mkty(-1, Tyvoid)));
        case Obad:      /* error! */
            break;
    }
}

static void inferfunc(Node *n)
{
}

static void inferdecl(Node *n)
{
    Type *t;

    t = decltype(n);
    settype(n, t);
    if (n->decl.init) {
        inferexpr(n->decl.init, NULL);
        unify(n, type(n), type(n->decl.init));
    }
}

static void infernode(Node *n)
{
    int i;

    switch (n->type) {
        case Nfile:
            pushstab(n->file.globls);
            for (i = 0; i < n->file.nstmts; i++)
                infernode(n->file.stmts[i]);
            popstab();
            break;
        case Ndecl:
            inferdecl(n);
            break;
        case Nblock:
            for (i = 0; i < n->block.nstmts; i++)
                infernode(n->block.stmts[i]);
            break;
        case Nifstmt:
            infernode(n->ifstmt.cond);
            infernode(n->ifstmt.iftrue);
            infernode(n->ifstmt.iffalse);
            break;
        case Nloopstmt:
            infernode(n->loopstmt.cond);
            infernode(n->loopstmt.init);
            infernode(n->loopstmt.step);
            infernode(n->loopstmt.body);
            break;
        case Nexpr:
            inferexpr(n, NULL);
        case Nfunc:
            inferfunc(n);
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

static void infercompn(Node *n)
{
}

static void checkcast(Node *n)
{
}

/* returns the final type for t, after all unifications
 * and default constraint selections */
static Type *tyfin(Type *t)
{
    t = tf(t);
    if (t->type == Tyvar) {
        if (hascstr(t, cstrtab[Tcint]) && cstrcheck(t, tytab[Tyint]))
            return mkty(-1, Tyint);
    }
    return t;
}

static void typesub(Node *n)
{
    int i;

    switch (n->type) {
        case Nfile:
            for (i = 0; i < n->file.nstmts; i++)
                typesub(n->file.stmts[i]);
            break;
        case Ndecl:
            settype(n, tyfin(type(n)));
            if (n->decl.init)
                typesub(n->decl.init);
            break;
        case Nblock:
            for (i = 0; i < n->block.nstmts; i++)
                typesub(n->block.stmts[i]);
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
            settype(n, tyfin(type(n)));
            for (i = 0; i < n->expr.nargs; i++)
                typesub(n->expr.args[i]);
            break;
        case Nfunc:
            die("don't do funcs yet...");
            typesub(n);
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

void infer(Node *file)
{
    assert(file->type == Nfile);

    loaduses(file);
    infernode(file);
    infercompn(file);
    checkcast(file);
    typesub(file);
}
