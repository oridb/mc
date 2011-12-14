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

static void loaduses(Node *n)
{
    int i;
    /* uses only allowed at top level. Do we want to keep it this way? */
    for (i = 0; i < n->file.nuses; i++)
        readuse(n->file.uses[i], n->file.globls);
}

/* a => b */
static void settype(Node *n, Type *t)
{
}

static void inferdecl(Node *n)
{
    Type *t;

    t = decltype(n);
    settype(n, t);
}

static Op exprop(Node *e)
{
    assert(e->type == Nexpr);
    return e->expr.op;
}

static void inferexpr(Node *n)
{
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
        case Oaddr:     /* &@a -> @a* */
        case Oderef:    /* *@a* ->  @a */
        case Olor:      /* @a || @b -> bool */
        case Oland:     /* @a && @b -> bool */
        case Olnot:     /* !@a -> bool */
        case Oeq:       /* @a == @a -> bool */
        case One:       /* @a != @a -> bool */
        case Ogt:       /* @a > @a -> bool */
        case Oge:       /* @a >= @a -> bool */
        case Olt:       /* @a < @a -> bool */
        case Ole:       /* @a <= @b -> bool */
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
        case Oidx:      /* @a[@b::tcint] -> @a */
        case Oslice:    /* @a[@b::tcint,@b::tcint] -> @a[,] */
        case Omemb:     /* @a.Ident -> @b, verify type(@a.Ident)==@b later */
        case Osize:     /* sizeof @a -> size */
        case Ocall:     /* (@a, @b, @c, ... -> @r)(@a,@b,@c, ... -> @r) -> @r */
        case Ocast:     /* cast(@a, @b) -> @b */
        case Oret:      /* -> @a -> void */
        case Ogoto:     /* goto void* -> void */
        case Ovar:      /* a:@a -> @a */
        case Olit:      /* <lit>:@a::tyclass -> @a */
        case Olbl:      /* :lbl -> void* */
        case Obad:      /* error! */
            break;
    }
}

static void inferlit(Node *n)
{
}

static void inferfunc(Node *n)
{
}

static void loaduse(Node *n)
{
    fprintf(stderr, "INTERNAL: implement usefiles");
}

static void infernode(Node *n)
{
    int i;

    switch (n->type) {
        case Nfile:
            for (i = 0; i < n->file.nuses; i++)
                loaduse(n->file.uses[i]);
            for (i = 0; i < n->file.nstmts; i++)
                infernode(n->file.stmts[i]);
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
            inferexpr(n);
        case Nlit:
            inferlit(n);
        case Nfunc:
            inferfunc(n);
        case Nname:
        case Nuse:
            break;
    }
}

static void infercompn(Node *n)
{
}

static void checkcast(Node *n)
{
}

static void typesub(Node *n)
{
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
