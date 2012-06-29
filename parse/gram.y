%{
#define YYERROR_VERBOSE

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

#include "parse.h"

void yyerror(const char *s);
int yylex(void);
static Op binop(int toktype);
static Node *mkpseudodecl(Type *t);
static void installucons(Stab *st, Type *t);
Stab *curscope;

%}

%token<tok> Terror
%token<tok> Tplus    /* + */
%token<tok> Tminus   /* - */
%token<tok> Tstar    /* * */
%token<tok> Tdiv     /* / */
%token<tok> Tinc     /* ++ */
%token<tok> Tdec     /* -- */
%token<tok> Tmod     /* % */
%token<tok> Tasn     /* = */
%token<tok> Taddeq   /* += */
%token<tok> Tsubeq   /* -= */
%token<tok> Tmuleq   /* *= */
%token<tok> Tdiveq   /* /= */
%token<tok> Tmodeq   /* %= */
%token<tok> Tboreq   /* |= */
%token<tok> Tbxoreq  /* ^= */
%token<tok> Tbandeq  /* &= */
%token<tok> Tbsleq   /* <<= */
%token<tok> Tbsreq   /* >>= */

%token<tok> Tbor     /* | */
%token<tok> Tbxor    /* ^ */
%token<tok> Tband    /* & */
%token<tok> Tbsl     /* << */
%token<tok> Tbsr     /* >> */
%token<tok> Tbnot    /* ~ */

%token<tok> Teq      /* == */
%token<tok> Tgt      /* > */
%token<tok> Tlt      /* < */
%token<tok> Tge      /* >= */
%token<tok> Tle      /* <= */
%token<tok> Tne      /* != */

%token<tok> Tlor     /* || */
%token<tok> Tland    /* && */
%token<tok> Tlnot    /* ! */

%token<tok> Tobrace  /* { */
%token<tok> Tcbrace  /* } */
%token<tok> Toparen  /* ( */
%token<tok> Tcparen  /* ) */
%token<tok> Tosqbrac /* [ */
%token<tok> Tcsqbrac /* ] */
%token<tok> Tat      /* @ */
%token<tok> Ttick    /* ` */

%token<tok> Ttype            /* type */
%token<tok> Tfor             /* for */
%token<tok> Twhile           /* while */
%token<tok> Tif              /* if */
%token<tok> Telse            /* else */
%token<tok> Telif            /* else */
%token<tok> Tmatch           /* match */
%token<tok> Tdefault /* default */
%token<tok> Tgoto            /* goto */

%token<tok> Tintlit
%token<tok> Tstrlit
%token<tok> Tfloatlit
%token<tok> Tchrlit
%token<tok> Tboollit

%token<tok> Tstruct  /* struct */
%token<tok> Tunion   /* union */
%token<tok> Ttyparam /* @typename */

%token<tok> Tconst   /* const */
%token<tok> Tvar     /* var */
%token<tok> Tgeneric /* var */
%token<tok> Textern  /* extern */
%token<tok> Tcast    /* castto */

%token<tok> Texport  /* export */
%token<tok> Tprotect /* protect */

%token<tok> Tellipsis        /* ... */
%token<tok> Tendln           /* ; or \n */
%token<tok> Tendblk  /* ;; */
%token<tok> Tcolon   /* : */
%token<tok> Tcoloncolon   /* :: */
%token<tok> Tdot             /* . */
%token<tok> Tcomma   /* , */
%token<tok> Tret             /* -> */
%token<tok> Tuse             /* use */
%token<tok> Tpkg             /* pkg */
%token<tok> Tsizeof  /* sizeof */

%token<tok> Tident
%token<tok> Teof

%start module

%type <ty> type structdef uniondef compoundtype functype funcsig
%type <ty> generictype

%type <tok> asnop cmpop addop mulop shiftop

%type <tydef> tydef

%type <node> exprln retexpr expr atomicexpr literal asnexpr lorexpr landexpr borexpr
%type <node> bandexpr cmpexpr unioncons addexpr mulexpr shiftexpr prefixexpr postfixexpr
%type <node> funclit arraylit name block blockbody stmt label use
%type <node> decl declbody declcore structelt
%type <node> ifstmt forstmt whilestmt matchstmt elifs optexprln
%type <node> pat match
%type <node> castexpr
%type <ucon> unionelt

%type <nodelist> arglist argdefs structbody params matches
%type <uconlist> unionbody

%union {
    struct {
        int line;
        Node **nl;
        size_t nn;
    } nodelist;
    struct {
        int line;
        Ucon **ucl;
        size_t nucl;
    } uconlist;
    struct {
        int line;
        Type **types;
        size_t ntypes;
    } tylist;
    struct { /* FIXME: unused */
        int line;
        char *name;
        Type *type;
    } tydef;
    Node *node;
    Tok  *tok;
    Type *ty;
    Ucon *ucon;
}

%%

module  : file
        | /* empty */
        ;

file    : toplev
        | file toplev
        ;

toplev
        : decl
            {lappend(&file->file.stmts, &file->file.nstmts, $1);
             putdcl(file->file.globls, $1);}
        | use
            {lappend(&file->file.uses, &file->file.nuses, $1);}
        | package
        | tydef
            {puttype(file->file.globls, mkname($1.line, $1.name), $1.type);
             installucons(file->file.globls, $1.type);}
        | Tendln
        ;

decl    : Tvar declbody Tendln
            {$$ = $2;}
        | Tconst declbody Tendln
            {$2->decl.isconst = 1;
             $$ = $2;}
        | Tgeneric declbody Tendln
            {$2->decl.isconst = 1;
             $2->decl.isgeneric = 1;
             $$ = $2;}
        | Textern Tvar declbody Tendln
            {$3->decl.isextern = 1;
             $$ = $3;}
        | Textern Tconst declbody Tendln
            {$3->decl.isconst = 1;
             $3->decl.isextern = 1;
             $$ = $3;}
        ;

use     : Tuse Tident Tendln
            {$$ = mkuse($1->line, $2->str, 0);}
        | Tuse Tstrlit Tendln
            {$$ = mkuse($1->line, $2->str, 1);}
        ;

package : Tpkg Tident Tasn pkgbody Tendblk
            {if (file->file.exports->name)
                 fatal($1->line, "Package already declared\n");
             updatens(file->file.exports, $2->str);
             updatens(file->file.globls, $2->str);
            }
        ;

pkgbody : pkgitem
        | pkgbody pkgitem
        ;

pkgitem : decl 
            {putdcl(file->file.exports, $1);
            if ($1->decl.init)
                 lappend(&file->file.stmts, &file->file.nstmts, $1);}
        | tydef {puttype(file->file.exports, mkname($1.line, $1.name), $1.type);}
        | visdef {die("Unimplemented visdef");}
        | Tendln
        ;

visdef  : Texport Tcolon
        | Tprotect Tcolon
        ;

declbody: declcore Tasn expr
            {$$ = $1; $1->decl.init = $3;}
        | declcore
        ;

declcore: name
            {$$ = mkdecl($1->line, $1, mktyvar($1->line));}
        | name Tcolon type
            {$$ = mkdecl($1->line, $1, $3);}
        ;

name    : Tident
            {$$ = mkname($1->line, $1->str);}
        | Tident Tdot name
            {$$ = $3; setns($3, $1->str);}
        ;

tydef   : Ttype Tident Tasn type Tendln
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = mktyalias($2->line, mkname($2->line, $2->str), $4);}
        | Ttype Tident Tendln
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = NULL;}
        ;

type    : structdef
        | uniondef
        | compoundtype
        | generictype
        | Tellipsis {$$ = mkty($1->line, Tyvalist);}
        ;

generictype
        : Ttyparam {$$ = mktyparam($1->line, $1->str);}
        /* FIXME: allow constrained typarmas */
        ;

compoundtype
        : functype   {$$ = $1;}
        | type Tosqbrac Tcomma Tcsqbrac {$$ = mktyslice($2->line, $1);}
        | type Tosqbrac expr Tcsqbrac {$$ = mktyarray($2->line, $1, $3);}
        | type Tstar {$$ = mktyptr($2->line, $1);}
        | name       {$$ = mktynamed($1->line, $1);}
        | Tat Tident {$$ = mktyparam($1->line, $2->str);}
        ;

functype: Toparen funcsig Tcparen {$$ = $2;}
        ;

funcsig : argdefs
            {$$ = mktyfunc($1.line, $1.nl, $1.nn, mktyvar($1.line));}
        | argdefs Tret type
            {$$ = mktyfunc($1.line, $1.nl, $1.nn, $3);}
        ;

argdefs : declcore
            {$$.line = $1->line;
             $$.nl = NULL;
             $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | argdefs Tcomma declcore
            {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */
            {$$.line = line;
             $$.nl = NULL;
             $$.nn = 0;}
        ;

structdef
        : Tstruct structbody Tendblk
            {$$ = mktystruct($1->line, $2.nl, $2.nn);}
        ;

structbody
        : structelt
            {if ($1) {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}}
        | structbody structelt
            {if ($2) {lappend(&$$.nl, &$$.nn, $2);}}
        ;

structelt
        : declcore Tendln
            {$$ = $1;}
        | visdef Tendln
            {$$ = NULL;}
        | Tendln
            {$$ = NULL;}
        ;

uniondef
        : Tunion unionbody Tendblk
            {$$ = mktyunion($1->line, $2.ucl, $2.nucl);}
        ;

unionbody
        : unionelt
            {$$.ucl = NULL; $$.nucl = 0;
             if ($1) {lappend(&$$.ucl, &$$.nucl, $1);}}
        | unionbody unionelt
            {if ($2) {lappend(&$$.ucl, &$$.nucl, $2);}}
        ;

unionelt /* nb: the ucon union type gets filled in when we have context */
        : Ttick Tident type Tendln
            {$$ = mkucon($2->line, mkname($2->line, $2->str), NULL, $3);}
        | Ttick Tident Tendln
            {$$ = mkucon($2->line, mkname($2->line, $2->str), NULL, NULL);}
        | visdef Tendln
            {$$ = NULL;}
        | Tendln
            {$$ = NULL;}
        ;

retexpr : Tret exprln
            {$$ = mkexpr($1->line, Oret, $2, NULL);}
        | exprln
        | Tret Tendln
            {$$ = mkexpr($1->line, Oret, NULL);}
        ;

exprln  : expr Tendln
        ;

expr    : asnexpr
        ;

asnexpr : lorexpr asnop asnexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | lorexpr
        ;

asnop   : Tasn
        | Taddeq        /* += */
        | Tsubeq        /* -= */
        | Tmuleq        /* *= */
        | Tdiveq        /* /= */
        | Tmodeq        /* %= */
        | Tboreq        /* |= */
        | Tbxoreq       /* ^= */
        | Tbandeq       /* &= */
        | Tbsleq        /* <<= */
        | Tbsreq        /* >>= */
        ;

lorexpr : lorexpr Tlor landexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | landexpr
        ;

landexpr: landexpr Tland cmpexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | cmpexpr
        ;

cmpexpr : cmpexpr cmpop castexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | castexpr
        ;

castexpr: unioncons Tcast Toparen type Tcparen
            {$$ = mkexpr($1->line, Ocast, $1, NULL);
             $$->expr.type = $4;}
        | unioncons 
        ;


cmpop   : Teq | Tgt | Tlt | Tge | Tle | Tne ;

unioncons
        : Ttick Tident borexpr
            {$$ = mkexpr($1->line, Ocons, mkname($2->line, $2->str), $3, NULL);}
        | Ttick Tident
            {$$ = mkexpr($1->line, Ocons, mkname($2->line, $2->str), NULL);}
        | borexpr
        ;


borexpr : borexpr Tbor bandexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | bandexpr
        ;

bandexpr: bandexpr Tband addexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | addexpr
        ;

addexpr : addexpr addop mulexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | mulexpr
        ;

addop   : Tplus | Tminus ;

mulexpr : mulexpr mulop shiftexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | shiftexpr
        ;

mulop   : Tstar | Tdiv | Tmod
        ;

shiftexpr
        : shiftexpr shiftop prefixexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | prefixexpr
        ;

shiftop : Tbsl | Tbsr;

prefixexpr
        : Tinc prefixexpr      {$$ = mkexpr($1->line, Opreinc, $2, NULL);}
        | Tdec prefixexpr      {$$ = mkexpr($1->line, Opredec, $2, NULL);}
        | Tstar prefixexpr     {$$ = mkexpr($1->line, Oderef, $2, NULL);}
        | Tband prefixexpr     {$$ = mkexpr($1->line, Oaddr, $2, NULL);}
        | Tlnot prefixexpr     {$$ = mkexpr($1->line, Olnot, $2, NULL);}
        | Tbnot prefixexpr     {$$ = mkexpr($1->line, Obnot, $2, NULL);}
        | Tminus prefixexpr    {$$ = mkexpr($1->line, Oneg, $2, NULL);}
        | Tplus prefixexpr     {$$ = $2;} /* positive is a nop */
        | postfixexpr
        ;

postfixexpr
        : postfixexpr Tdot Tident
            {$$ = mkexpr($1->line, Omemb, $1, mkname($3->line, $3->str), NULL);}
        | postfixexpr Tinc
            {$$ = mkexpr($1->line, Opostinc, $1, NULL);}
        | postfixexpr Tdec
            {$$ = mkexpr($1->line, Opostdec, $1, NULL);}
        | postfixexpr Tosqbrac expr Tcsqbrac
            {$$ = mkexpr($1->line, Oidx, $1, $3, NULL);}
        | postfixexpr Tosqbrac expr Tcomma expr Tcsqbrac
            {$$ = mkexpr($1->line, Oslice, $1, $3, $5, NULL);}
        | postfixexpr Toparen arglist Tcparen
            {$$ = mkcall($1->line, $1, $3.nl, $3.nn);}
        | atomicexpr
        ;

arglist : asnexpr
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | arglist Tcomma asnexpr
            {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */
            {$$.nl = NULL; $$.nn = 0;}
        ;

atomicexpr
        : Tident
            {$$ = mkexpr($1->line, Ovar, mkname($1->line, $1->str), NULL);}
        | literal {$$ = mkexpr($1->line, Olit, $1, NULL);}
        | Toparen expr Tcparen
            {$$ = $2;}
        | Tsizeof Toparen type Tcparen
            {$$ = mkexpr($1->line, Osize, mkpseudodecl($3), NULL);}
        ;

literal : funclit       {$$ = $1;}
        | arraylit      {$$ = $1;}
        | Tstrlit       {$$ = mkstr($1->line, $1->str);}
        | Tintlit       {$$ = mkint($1->line, strtol($1->str, NULL, 0));}
        | Tchrlit       {$$ = mkchar($1->line, *$1->str);} /* FIXME: expand escapes, unicode  */
        | Tfloatlit     {$$ = mkfloat($1->line, strtod($1->str, NULL));}
        | Tboollit      {$$ = mkbool($1->line, !strcmp($1->str, "true"));}
        ;

funclit : Tobrace params Tendln blockbody Tcbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, mktyvar($3->line), $4);}
        | Tobrace params Tret type Tendln blockbody Tcbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, $4, $6);}
        ;

params  : declcore
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | params Tcomma declcore
            {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */
            {$$.nl = NULL; $$.nn = 0;}
        ;

arraylit : Tosqbrac arraybody Tcsqbrac
            {$$ = NULL; die("Unimpl arraylit");}
         ;

arraybody
        : expr
        | arraybody Tcomma expr
        ;

stmt    : decl
        | retexpr
        | label
        | ifstmt
        | forstmt
        | whilestmt
        | matchstmt
        | Tendln {$$ = NULL;}
        ;

forstmt : Tfor optexprln optexprln optexprln block
            {$$ = mkloopstmt($1->line, $2, $3, $4, $5);}
        | Tfor decl optexprln optexprln block
            {$$ = mkloopstmt($1->line, $2, $3, $4, $5);}
        ;

optexprln: exprln {$$ = $1;}
         | Tendln {$$ = NULL;}
         ;

whilestmt
        : Twhile exprln block
            {$$ = mkloopstmt($1->line, NULL, $2, NULL, $3);}
        ;

ifstmt  : Tif exprln blockbody elifs
            {$$ = mkifstmt($1->line, $2, $3, $4);}
        ;

elifs   : Telif exprln blockbody elifs
            {$$ = mkifstmt($1->line, $2, $3, $4);}
        | Telse blockbody Tendblk
            {$$ = $2;}
        | Tendblk
            {$$ = NULL;}
        ;

matchstmt: Tmatch exprln matches Tendblk
            {$$ = NULL;}
         ;

matches : match
            {$$.nl = NULL; $$.nn = 0;
             if ($1)
                 lappend(&$$.nl, &$$.nn, $1);}
        | matches match
            {if ($2)
                 lappend(&$$.nl, &$$.nn, $2);}
        ;

match   : pat Tcolon block {$$ = NULL;}
        | Tendln {$$ = NULL;}
        ;

pat     : literal {$$ = NULL;}
        ;

block   : blockbody Tendblk
        | Tendblk {$$ = NULL;}
        ;

blockbody
        : stmt
            {$$ = mkblock(line, mkstab());
             if ($1)
                lappend(&$$->block.stmts, &$$->block.nstmts, $1);
             if ($1 && $1->type == Ndecl)
                putdcl($$->block.scope, $1);}
        | blockbody stmt
            {if ($2)
                lappend(&$1->block.stmts, &$1->block.nstmts, $2);
             if ($2 && $2->type == Ndecl)
                putdcl($1->block.scope, $2);
             $$ = $1;}
        ;

label   : Tcolon Tident
            {$$ = mklbl($1->line, $1->str);}
        ;

%%

static Node *mkpseudodecl(Type *t)
{
    static int nextpseudoid;
    char buf[128];

    snprintf(buf, 128, ".pdecl%d", nextpseudoid++);
    return mkdecl(-1, mkname(-1, buf), t);
}

static void installucons(Stab *st, Type *t)
{
    Type *b;
    size_t i;

    b = tybase(t);
    if (b->type != Tyunion)
        return;
    for (i = 0; i < b->nmemb; i++) {
        b->udecls[i]->utype = t;
        b->udecls[i]->id = i;
        putucon(st, b->udecls[i]);
    }
}

void yyerror(const char *s)
{
    fprintf(stderr, "%d: %s", line, s);
    if (curtok->str)
        fprintf(stderr, " near \"%s\"", curtok->str);
    fprintf(stderr, "\n");
    exit(1);
}

static Op binop(int tt)
{
    Op o;

    o = Obad;
    switch (tt) {
        case Tplus:     o = Oadd;       break;
        case Tminus:    o = Osub;       break;
        case Tstar:     o = Omul;       break;
        case Tdiv:      o = Odiv;       break;
        case Tmod:      o = Omod;       break;
        case Tasn:      o = Oasn;       break;
        case Taddeq:    o = Oaddeq;     break;
        case Tsubeq:    o = Osubeq;     break;
        case Tmuleq:    o = Omuleq;     break;
        case Tdiveq:    o = Odiveq;     break;
        case Tmodeq:    o = Omodeq;     break;
        case Tboreq:    o = Oboreq;     break;
        case Tbxoreq:   o = Obxoreq;    break;
        case Tbandeq:   o = Obandeq;    break;
        case Tbsleq:    o = Obsleq;     break;
        case Tbsreq:    o = Obsreq;     break;
        case Tbor:      o = Obor;       break;
        case Tbxor:     o = Obxor;      break;
        case Tband:     o = Oband;      break;
        case Tbsl:      o = Obsl;       break;
        case Tbsr:      o = Obsr;       break;
        case Teq:       o = Oeq;        break;
        case Tgt:       o = Ogt;        break;
        case Tlt:       o = Olt;        break;
        case Tge:       o = Oge;        break;
        case Tle:       o = Ole;        break;
        case Tne:       o = One;        break;
        case Tlor:      o = Olor;       break;
        case Tland:     o = Oland;      break;
        default:
            die("Unimplemented binop\n");
            break;
    }
    return o;
}

