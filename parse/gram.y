%{
#define YYERROR_VERBOSE
#define YYDEBUG 1

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
static void constrainwith(Type *t, char *str);

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
%token<tok> Thash    /* # */

%token<tok> Ttype    /* type */
%token<tok> Tfor     /* for */
%token<tok> Twhile   /* while */
%token<tok> Tif      /* if */
%token<tok> Telse    /* else */
%token<tok> Telif    /* else */
%token<tok> Tmatch   /* match */
%token<tok> Tdefault /* default */
%token<tok> Tgoto    /* goto */

%token<tok> Tintlit
%token<tok> Tstrlit
%token<tok> Tfloatlit
%token<tok> Tchrlit
%token<tok> Tboollit

%token<tok> Ttrait   /* trait */
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

%token<tok> Tellipsis/* ... */
%token<tok> Tendln   /* ; or \n */
%token<tok> Tendblk  /* ;; */
%token<tok> Tcolon   /* : */
%token<tok> Twith    /* :: */
%token<tok> Tdot     /* . */
%token<tok> Tcomma   /* , */
%token<tok> Tret     /* -> */
%token<tok> Tuse     /* use */
%token<tok> Tpkg     /* pkg */
%token<tok> Tsizeof  /* sizeof */

%token<tok> Tident
%token<tok> Teof

%start file

%type <ty> type structdef uniondef tupledef compoundtype functype funcsig
%type <ty> generictype
%type <tylist> tuptybody
%type <node> typaramlist

%type <tok> asnop cmpop addop mulop shiftop

%type <tydef> tydef

%type <node> exprln retexpr expr atomicexpr littok literal asnexpr lorexpr landexpr borexpr
%type <node> bandexpr cmpexpr unionexpr addexpr mulexpr shiftexpr prefixexpr postfixexpr
%type <node> funclit seqlit name block stmt label use
%type <node> decl declbody declcore structelt seqelt tuphead
%type <node> ifstmt forstmt whilestmt matchstmt elifs optexprln optexpr
%type <node> pat unionpat match
%type <node> castexpr
%type <ucon> unionelt
%type <node> body 

%type <nodelist> arglist argdefs params matches
%type <nodelist> structbody seqbody tupbody tuprest
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

file    : toplev
        | file Tendln toplev
        ;

toplev
        : decl
            {lappend(&file->file.stmts, &file->file.nstmts, $1);
             $1->decl.isglobl = 1;
             putdcl(file->file.globls, $1);}
        | use
            {lappend(&file->file.uses, &file->file.nuses, $1);}
        | package
        | tydef
            {puttype(file->file.globls, mkname($1.line, $1.name), $1.type);
             installucons(file->file.globls, $1.type);}
        | /* empty */
        ;

decl    : Tvar declbody
            {$$ = $2;}
        | Tconst declbody
            {$2->decl.isconst = 1;
             $$ = $2;}
        | Tgeneric declbody
            {$2->decl.isconst = 1;
             $2->decl.isgeneric = 1;
             $$ = $2;}
        | Textern Tvar declbody
            {$3->decl.isextern = 1;
             $$ = $3;}
        | Textern Tconst declbody
            {$3->decl.isconst = 1;
             $3->decl.isextern = 1;
             $$ = $3;}
        ;

use     : Tuse Tident
            {$$ = mkuse($1->line, $2->str, 0);}
        | Tuse Tstrlit
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
        | pkgbody Tendln pkgitem
        ;

pkgitem : decl
            {putdcl(file->file.exports, $1);
             if ($1->decl.init)
                 lappend(&file->file.stmts, &file->file.nstmts, $1);}
        | tydef {puttype(file->file.exports, mkname($1.line, $1.name), $1.type);}
        | visdef {die("Unimplemented visdef");}
        | /* empty */
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

tydef   : Ttype Tident Tasn type
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = mktyalias($2->line, mkname($2->line, $2->str), $4);}
        | Ttype Tident
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = NULL;}
        ;

type    : structdef
        | tupledef
        | uniondef
        | compoundtype
        | generictype
        | Tellipsis {$$ = mkty($1->line, Tyvalist);}
        ;

generictype
        : Ttyparam typaramlist 
            {$$ = mktyparam($1->line, $1->str);
            /* FIXME: this will only work for builtin cstrs */
             if ($2)
                constrainwith($$, $2->name.name);}
        ;

typaramlist
        : /* empty */ {$$ = NULL;}
        | Twith name {$$ = $2;}
        ;

compoundtype
        : functype   {$$ = $1;}
        | type Tosqbrac Tcolon Tcsqbrac {$$ = mktyslice($2->line, $1);}
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

tupledef: Tosqbrac tuptybody Tcsqbrac
            {$$ = mktytuple($1->line, $2.types, $2.ntypes);}
        ;

tuptybody
        : type
            {$$.types = NULL; $$.ntypes = 0;
             lappend(&$$.types, &$$.ntypes, $1);}
        | tuptybody Tcomma type
            {lappend(&$$.types, &$$.ntypes, $3);}
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
        : Ttick name type Tendln
            {$$ = mkucon($2->line, $2, NULL, $3);}
        | Ttick name Tendln
            {$$ = mkucon($2->line, $2, NULL, NULL);}
        | visdef Tendln
            {$$ = NULL;}
        | Tendln
            {$$ = NULL;}
        ;

retexpr : Tret expr
            {$$ = mkexpr($1->line, Oret, $2, NULL);}
        | expr
        | Tret
            {$$ = mkexpr($1->line, Oret, NULL);}
        ;

optexpr : expr {$$ = $1;}
        | /* empty */ {$$ = NULL;}
        ;

optexprln: exprln {$$ = $1;}
         | Tendln {$$ = NULL;}
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


cmpop   : Teq | Tgt | Tlt | Tge | Tle | Tne ;

castexpr: unionexpr Tcast Toparen type Tcparen
            {$$ = mkexpr($1->line, Ocast, $1, NULL);
             $$->expr.type = $4;}
        | unionexpr 
        ;

unionexpr
        : Ttick name borexpr
            {$$ = mkexpr($1->line, Ocons, $2, $3, NULL);}
        | Ttick name
            {$$ = mkexpr($1->line, Ocons, $2, NULL);}
        | borexpr
        ;


borexpr : borexpr Tbor bandexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | borexpr Tbxor bandexpr
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
        | postfixexpr Tosqbrac optexpr Tcolon optexpr Tcsqbrac
            {$$ = mksliceexpr($1->line, $1, $3, $5);}
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
        | Toparen tupbody Tcparen
            {$$ = mkexprl($1->line, Otup, $2.nl, $2.nn);}
        | Tsizeof Toparen type Tcparen
            {$$ = mkexpr($1->line, Osize, mkpseudodecl($3), NULL);}
        ;

tupbody : tuphead tuprest
            {$$ = $2;
             linsert(&$$.nl, &$$.nn, 0, $1);}
        ;

tuphead : expr Tcomma {$$ = $1;}
        ;

tuprest : /*empty */
            {$$.nl = NULL; $$.nn = 0;}
        | expr
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | tuprest Tcomma expr
            {lappend(&$$.nl, &$$.nn, $3);}
        ;

literal : funclit       {$$ = $1;}
        | seqlit        {$$ = $1;}
        | littok        {$$ = $1;}
        ;

littok  : Tstrlit       {$$ = mkstr($1->line, $1->str);}
        | Tintlit       {$$ = mkint($1->line, $1->intval);}
        | Tchrlit       {$$ = mkchar($1->line, *$1->str);} /* FIXME: expand escapes, unicode  */
        | Tfloatlit     {$$ = mkfloat($1->line, $1->fltval);}
        | Tboollit      {$$ = mkbool($1->line, !strcmp($1->str, "true"));}
        ;

funclit : Tobrace params Tendln body Tcbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, mktyvar($3->line), $4);}
        | Tobrace params Tret type Tendln body Tcbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, $4, $6);}
        ;

params  : declcore
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | params Tcomma declcore
            {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */
            {$$.nl = NULL; $$.nn = 0;}
        ;

seqlit  : Tosqbrac seqbody Tcsqbrac
            {$$ = mkseq($1->line, $2.nl, $2.nn);}
        ;

seqbody : /* empty */ {$$.nl = NULL; $$.nn = 0;}
        | seqelt
            {$$.nl = NULL; $$.nn = 0;
             lappend(&$$.nl, &$$.nn, $1);}
        | seqbody Tcomma seqelt
             {lappend(&$$.nl, &$$.nn, $3);}
        ;

seqelt  : Tdot Tident Tasn expr
            {die("Unimplemented struct member init");}
        | Thash atomicexpr Tasn expr
            {die("Unimplmented array member init");}
        | endlns expr endlns{$$ = $2;}
        ;

endlns  : /* none */
        | endlns Tendln
        ;

stmt    : decl
        | retexpr
        | label
        | ifstmt
        | forstmt
        | whilestmt
        | matchstmt
        | /* empty */ {$$ = NULL;}
        ;

forstmt : Tfor optexprln optexprln optexprln block
            {$$ = mkloopstmt($1->line, $2, $3, $4, $5);}
        | Tfor decl Tendln optexprln optexprln block
            {$$ = mkloopstmt($1->line, $2, $4, $5, $6);}
        ;

whilestmt
        : Twhile exprln block
            {$$ = mkloopstmt($1->line, NULL, $2, NULL, $3);}
        ;

ifstmt  : Tif exprln body elifs
            {$$ = mkifstmt($1->line, $2, $3, $4);}
        ;

elifs   : Telif exprln body elifs
            {$$ = mkifstmt($1->line, $2, $3, $4);}
        | Telse block
            {$$ = $2;}
        | Tendblk
            {$$ = NULL;}
        ;

matchstmt: Tmatch exprln matches Tendblk
            {$$ = mkmatchstmt($1->line, $2, $3.nl, $3.nn);}
         ;

matches : match
            {$$.nl = NULL; $$.nn = 0;
             if ($1)
                 lappend(&$$.nl, &$$.nn, $1);}
        | matches match
            {if ($2)
                 lappend(&$$.nl, &$$.nn, $2);}
        ;

match   : pat Tcolon block {$$ = mkmatch($1->line, $1, $3);}
        | Tendln {$$ = NULL;}
        ;

pat     : unionpat {$$ = $1;}
        | littok {$$ = mkexpr($1->line, Olit, $1, NULL);}
        | Tident {$$ = mkexpr($1->line, Ovar, mkname($1->line, $1->str), NULL);}
        | Toparen pat Tcparen {$$ = $2;}
        ;

unionpat: Ttick Tident pat
            {$$ = mkexpr($1->line, Ocons, mkname($2->line, $2->str), $3, NULL);}
        | Ttick Tident
            {$$ = mkexpr($1->line, Ocons, mkname($2->line, $2->str), NULL);}
        ;

block   : body Tendblk
        ;

body    : stmt
            {$$ = mkblock(line, mkstab());
             if ($1)
                lappend(&$$->block.stmts, &$$->block.nstmts, $1);
             if ($1 && $1->type == Ndecl)
                putdcl($$->block.scope, $1);}
        | body Tendln stmt
            {if ($3)
                lappend(&$1->block.stmts, &$1->block.nstmts, $3);
             if ($3 && $3->type == Ndecl)
                putdcl($1->block.scope, $3);
             $$ = $1;}
        ;

label   : Tcolon Tident
            {$$ = mklbl($1->line, $1->str);}
        ;

%%

static void constrainwith(Type *t, char *str)
{
    size_t i;

    for (i = 0; i < ncstrs; i++) {
        if (!strcmp(cstrtab[i]->name, str)) {
            setcstr(t, cstrtab[i]);
            return;
        }
    }
    fatal(t->line, "Constraint %s does not exist", str);
}

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

