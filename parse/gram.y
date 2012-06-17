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
Stab *curscope;
//int n = 0;
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

%token<tok> Tenum    /* enum */
%token<tok> Tstruct  /* struct */
%token<tok> Tunion   /* union */
%token<tok> Ttyparam /* @typename */

%token<tok> Tconst   /* const */
%token<tok> Tvar     /* var */
%token<tok> Tgeneric /* var */
%token<tok> Textern  /* extern */

%token<tok> Texport  /* export */
%token<tok> Tprotect /* protect */


%token<tok> Tellipsis        /* ... */
%token<tok> Tendln           /* ; or \n */
%token<tok> Tendblk  /* ;; */
%token<tok> Tcolon   /* : */
%token<tok> Tdot             /* . */
%token<tok> Tcomma   /* , */
%token<tok> Tret             /* -> */
%token<tok> Tuse             /* use */
%token<tok> Tpkg             /* pkg */
%token<tok> Tsizeof  /* sizeof */

%token<tok> Tident
%token<tok> Teof

%start module

%type <ty> type structdef uniondef enumdef compoundtype functype funcsig
%type <ty> generictype

%type <tok> asnop cmpop addop mulop shiftop

%type <tydef> tydef

%type <node> exprln retexpr expr atomicexpr literal asnexpr lorexpr landexpr borexpr
%type <node> bandexpr cmpexpr addexpr mulexpr shiftexpr prefixexpr postfixexpr
%type <node> funclit arraylit name block blockbody stmt label use
%type <node> decl declbody declcore structelt enumelt unionelt
%type <node> ifstmt forstmt whilestmt elifs optexprln

%type <nodelist> arglist argdefs structbody enumbody unionbody params

%union {
    struct {
        int line;
        Node **nl;
        size_t nn;
    } nodelist;
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
             putdcl(file->file.globls, $1->decl.sym);}
        | use
            {lappend(&file->file.uses, &file->file.nuses, $1);}
        | package
        | tydef
            {puttype(file->file.globls, mkname($1.line, $1.name), $1.type);}
        | Tendln
        ;

decl    : Tvar declbody Tendln
            {$2->decl.flags = 0;
             $$ = $2;}
        | Tconst declbody Tendln
            {$2->decl.sym->isconst = 1;
             $2->decl.flags = 0;
             $$ = $2;}
        | Tgeneric declbody Tendln
            {$2->decl.sym->isconst = 1;
             $2->decl.flags = 0;
             $$ = $2;}
        | Textern Tvar declbody Tendln
            {$3->decl.flags = Dclextern;
             $$ = $3;}
        | Textern Tconst declbody Tendln
            {$3->decl.flags = Dclconst | Dclextern;
             $$ = $3;}
        ;

use     : Tuse Tident Tendln
            {$$ = mkuse($1->line, $2->str, 0);}
        | Tuse Tstrlit Tendln
            {$$ = mkuse($1->line, $2->str, 1);}
        ;

package : Tpkg Tident Tasn pkgbody Tendblk
            {file->file.exports->name = mkname($2->line, $1->str);}
        ;


pkgbody : pkgitem
        | pkgbody pkgitem
        ;

pkgitem : decl {putdcl(file->file.exports, $1->decl.sym);}
        | tydef {puttype(file->file.exports, 
                         mkname($1.line, $1.name),
                         $1.type);}
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
            {$$ = mkdecl($1->line, mksym($1->line, $1, mktyvar($1->line)));}
        | name Tcolon type
            {$$ = mkdecl($1->line, mksym($1->line, $1, $3));}
        ;

name    : Tident
            {$$ = mkname($1->line, $1->str);}
        | Tident Tdot name
            {$$ = $3; setns($3, $1->str);}
        ;

tydef   : Ttype Tident Tasn type Tendln
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = $4;}
        | Ttype Tident Tendln
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = NULL;}
        ;

type    : structdef
        | uniondef
        | enumdef
        | compoundtype
        | generictype
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
            {$$ = mktyunion($1->line, $2.nl, $2.nn);}
        ;

unionbody
        : unionelt
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | unionbody unionelt
            {if ($2) {lappend(&$$.nl, &$$.nn, $2);}}
        ;

unionelt
        : Tident type Tendln
            {$$ = NULL; die("unionelt impl");}
        | visdef Tendln
            {$$ = NULL;}
        | Tendln
            {$$ = NULL;}
        ;

enumdef : Tenum enumbody Tendblk
            {$$ = mktyenum($1->line, $2.nl, $2.nn);}
        ;

enumbody: enumelt
            {$$.nl = NULL; $$.nn = 0; if ($1) lappend(&$$.nl, &$$.nn, $1);}
        | enumbody enumelt
            {if ($2) {lappend(&$$.nl, &$$.nn, $2);}}
        ;

enumelt : Tident Tendln
            {$$ = NULL; die("enumelt impl");}
        | Tident Tasn exprln
            {$$ = NULL; die("enumelt impl");}
        | Tendln
            {$$ = NULL;}
        ;

retexpr : Tret exprln
            {$$ = mkexpr($1->line, Oret, $2, NULL);}
        | exprln
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

landexpr: landexpr Tland borexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | borexpr
        ;

borexpr : borexpr Tbor bandexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | bandexpr
        ;

bandexpr: bandexpr Tband cmpexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | cmpexpr
        ;

cmpexpr : cmpexpr cmpop addexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | addexpr
        ;

cmpop   : Teq | Tgt | Tlt | Tge | Tle | Tne ;

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
        : Tinc postfixexpr      {$$ = mkexpr($1->line, Opreinc, $2, NULL);}
        | Tdec postfixexpr      {$$ = mkexpr($1->line, Opredec, $2, NULL);}
        | Tstar postfixexpr     {$$ = mkexpr($1->line, Oderef, $2, NULL);}
        | Tband postfixexpr     {$$ = mkexpr($1->line, Oaddr, $2, NULL);}
        | Tlnot postfixexpr     {$$ = mkexpr($1->line, Olnot, $2, NULL);}
        | Tbnot postfixexpr     {$$ = mkexpr($1->line, Obnot, $2, NULL);}
        | Tminus postfixexpr    {$$ = mkexpr($1->line, Oneg, $2, NULL);}
        | Tplus postfixexpr     {$$ = $2;} /* positive is a nop */
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
        | Tsizeof atomicexpr
            {$$ = mkexpr($1->line, Osize, $2, NULL);}
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
        | Tendln {$$ = NULL;}
        ;

forstmt : Tfor optexprln optexprln optexprln block
            {$$ = mkloop($1->line, $2, $3, $4, $5);}
        | Tfor decl optexprln optexprln block
            {$$ = mkloop($1->line, $2, $3, $4, $5);}
        ;

optexprln: exprln {$$ = $1;}
         | Tendln {$$ = NULL;}
         ;

whilestmt
        : Twhile exprln block
            {$$ = mkloop($1->line, NULL, $2, NULL, $3);}
        ;

ifstmt  : Tif exprln blockbody elifs
            {$$ = mkif($1->line, $2, $3, $4);}
        ;

elifs   : Telif exprln blockbody elifs
            {$$ = mkif($1->line, $2, $3, $4);}
        | Telse blockbody Tendblk
            {$$ = $2;}
        | Tendblk
            {$$ = NULL;}
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
                putdcl($$->block.scope, $1->decl.sym);}
        | blockbody stmt
            {if ($2)
                lappend(&$1->block.stmts, &$1->block.nstmts, $2);
             if ($2 && $2->type == Ndecl)
                putdcl($1->block.scope, $2->decl.sym);
             $$ = $1;}
        ;

label   : Tcolon Tident
            {$$ = mklbl($1->line, $1->str);}
        ;

%%

void yyerror(const char *s)
{
    fprintf(stderr, "%d: %s", line, s);
    if (curtok->str)
        fprintf(stderr, " near %s", curtok->str);
    fprintf(stderr, "\n");
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

