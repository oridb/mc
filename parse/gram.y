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

Stab *curscope;

void yyerror(const char *s);
int yylex(void);

static Op binop(int toktype);
static Node *mkpseudodecl(Type *t);
static void installucons(Stab *st, Type *t);
static void addtrait(Type *t, char *str);

%}

%token<tok> Terror
%token<tok> Tplus    /* + */
%token<tok> Tminus   /* - */
%token<tok> Tmul     /* * */
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
%token<tok> Tderef   /* # */

%token<tok> Ttype    /* type */
%token<tok> Tfor     /* for */
%token<tok> Tin      /* in */
%token<tok> Twhile   /* while */
%token<tok> Tif      /* if */
%token<tok> Telse    /* else */
%token<tok> Telif    /* else */
%token<tok> Tmatch   /* match */
%token<tok> Tgoto    /* goto */
%token<tok> Tbreak   /* break */
%token<tok> Tcontinue   /* continue */

%token<tok> Tintlit
%token<tok> Tstrlit
%token<tok> Tfloatlit
%token<tok> Tchrlit
%token<tok> Tboollit

%token<tok> Ttrait   /* trait */
%token<tok> Timpl   /* trait */
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
%type <tylist> typelist typarams
%type <nodelist> typaramlist

%type <tok> asnop cmpop addop mulop shiftop optident

%type <tydef> tydef typeid
%type <trait> traitdef

%type <node> exprln retexpr goto continue break expr atomicexpr 
%type <node> littok literal asnexpr lorexpr landexpr borexpr
%type <node> bandexpr cmpexpr unionexpr addexpr mulexpr shiftexpr prefixexpr postfixexpr
%type <node> funclit seqlit tuplit name block stmt label use
%type <node> declbody declcore structent arrayelt structelt tuphead
%type <node> ifstmt forstmt whilestmt matchstmt elifs optexprln optexpr
%type <node> match
%type <node> castexpr
%type <ucon> unionelt
%type <node> blkbody
%type <node> implstmt

%type <nodelist> arglist argdefs params matches
%type <nodelist> structbody structelts arrayelts 
%type <nodelist> tupbody tuprest 
%type <nodelist> decl decllist
%type <nodelist> traitbody implbody

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
        Type **params;
        size_t nparams;
    } tydef;
    Trait *trait;
    Node *node;
    Tok  *tok;
    Type *ty;
    Ucon *ucon;
}

%%

file    : toplev
        | file Tendln toplev
        ;

toplev  : package
        | use {lappend(&file->file.uses, &file->file.nuses, $1);}
        | implstmt {
                lappend(&file->file.stmts, &file->file.nstmts, $1);
                putimpl(file->file.globls, $1);
            }
        | traitdef {
                size_t i;
                puttrait(file->file.globls, $1->name, $1);
                for (i = 0; i < $1->nfuncs; i++)
                    putdcl(file->file.exports, $1->funcs[i]);
            }
        | tydef {
                puttype(file->file.globls, mkname($1.line, $1.name), $1.type);
                installucons(file->file.globls, $1.type);
            }
        | decl {
                size_t i;
                for (i = 0; i < $1.nn; i++) {
                    lappend(&file->file.stmts, &file->file.nstmts, $1.nl[i]);
                    $1.nl[i]->decl.isglobl = 1;
                    putdcl(file->file.globls, $1.nl[i]);
                }
            }
        | /* empty */
        ;

decl    : Tvar decllist {$$ = $2;}
        | Tconst decllist {
                size_t i;
                for (i = 0; i < $2.nn; i++)
                    $2.nl[i]->decl.isconst = 1;
                $$ = $2;
            }
        | Tgeneric decllist {
                size_t i;
             for (i = 0; i < $2.nn; i++) {
                $2.nl[i]->decl.isconst = 1;
                $2.nl[i]->decl.isgeneric = 1;
             }
             $$ = $2;}
        | Textern Tvar decllist {
                size_t i;
                for (i = 0; i < $3.nn; i++)
                    $3.nl[i]->decl.isextern = 1;
                $$ = $3;
            }
        | Textern Tconst decllist {
                size_t i;
                for (i = 0; i < $3.nn; i++) {
                    $3.nl[i]->decl.isconst = 1;
                    $3.nl[i]->decl.isextern = 1;
                }
                $$ = $3;
            }
        ;

decllist: declbody {
                $$.nl = NULL; $$.nn = 0;
                lappend(&$$.nl, &$$.nn, $1);
            }
        | declbody Tcomma decllist {
                linsert(&$3.nl, &$3.nn, 0, $1);
                $$=$3;
            }
        ;

use     : Tuse Tident {$$ = mkuse($1->line, $2->str, 0);}
        | Tuse Tstrlit {$$ = mkuse($1->line, $2->str, 1);}
        ;

optident: Tident      {$$ = $1;}
        | /* empty */ {$$ = NULL;}
        ;

package : Tpkg optident Tasn pkgbody Tendblk {
                if (file->file.exports->name)
                    fatal($1->line, "Package already declared\n");
                if ($2) {
                    updatens(file->file.exports, $2->str);
                    updatens(file->file.globls, $2->str);
                }
            }
        ;

pkgbody : pkgitem
        | pkgbody Tendln pkgitem
        ;

pkgitem : decl {
                size_t i;
                for (i = 0; i < $1.nn; i++) {
                    putdcl(file->file.exports, $1.nl[i]);
                    if ($1.nl[i]->decl.init)
                        lappend(&file->file.stmts, &file->file.nstmts, $1.nl[i]);
                }
            }
        | tydef {
                puttype(file->file.exports, mkname($1.line, $1.name), $1.type);
                installucons(file->file.exports, $1.type);
            }
        | traitdef {
                size_t i;
                $1->vis = Visexport;
                puttrait(file->file.exports, $1->name, $1);
                for (i = 0; i < $1->nfuncs; i++)
                    putdcl(file->file.exports, $1->funcs[i]);
            }
        | implstmt {
                $1->impl.vis = Visexport;
                putimpl(file->file.exports, $1);
            }
        | visdef {die("Unimplemented visdef");}
        | /* empty */
        ;

visdef  : Texport Tcolon
        | Tprotect Tcolon
        ;

declbody: declcore Tasn expr {$$ = $1; $1->decl.init = $3;}
        | declcore
        ;

declcore: name {$$ = mkdecl($1->line, $1, mktyvar($1->line));}
        | name Tcolon type {$$ = mkdecl($1->line, $1, $3);}
        ;

name    : Tident {$$ = mkname($1->line, $1->str);}
        | Tident Tdot name {$$ = $3; setns($3, $1->str);}
        ;

implstmt: Timpl name type {
                $$ = mkimplstmt($1->line, $2, $3, NULL, 0);
                $$->impl.isproto = 1;
            }
        | Timpl name type Tasn Tendln implbody Tendblk {
                $$ = mkimplstmt($1->line, $2, $3, $6.nl, $6.nn);
            }
        ;

implbody
        : optendlns {$$.nl = NULL; $$.nn = 0;}
        | implbody Tident Tasn exprln optendlns {
                Node *d;
                $$ = $1;
                d = mkdecl($2->line, mkname($2->line, $2->str), mktyvar($2->line));
                d->decl.init = $4;
                d->decl.isconst = 1;
                lappend(&$$.nl, &$$.nn, d);
            }
        ;

traitdef: Ttrait Tident generictype /* trait prototype */ {
                $$ = mktrait($1->line, mkname($2->line, $2->str), $3, NULL, 0, NULL, 0, 1);
            }
        | Ttrait Tident generictype Tasn traitbody Tendblk /* trait definition */ {
                size_t i;
                $$ = mktrait($1->line, mkname($2->line, $2->str), $3, NULL, 0, $5.nl, $5.nn, 0);
                for (i = 0; i < $5.nn; i++) {
                    $5.nl[i]->decl.trait = $$;
                    $5.nl[i]->decl.isgeneric = 1;
                }
            }
        ;

traitbody
        : optendlns {$$.nl = NULL; $$.nn = 0;}
        | traitbody Tident Tcolon type optendlns {
                Node *d;
                $$ = $1;
                d = mkdecl($2->line, mkname($2->line, $2->str), $4);
                d->decl.isgeneric = 1;
                lappend(&$$.nl, &$$.nn, d);
            }
        ;


tydef   : Ttype typeid {$$ = $2;}
        | Ttype typeid Tasn type {
                $$ = $2;
                $$.type = mktyname($2.line, mkname($2.line, $2.name), $2.params, $2.nparams, $4);
            }
        ;

typeid  : Tident {
                $$.line = $1->line;
                $$.name = $1->str;
                $$.params = NULL;
                $$.type = NULL;
            }
        | Tident Toparen typarams Tcparen {
                $$.line = $1->line;
                $$.name = $1->str;
                $$.params = $3.types;
                $$.nparams = $3.ntypes;
                $$.type = NULL;
            }
        ;

typarams: generictype {
                $$.types = NULL; $$.ntypes = 0;
                lappend(&$$.types, &$$.ntypes, $1);
            }
        | typarams Tcomma generictype {lappend(&$$.types, &$$.ntypes, $3);}
        ;

type    : structdef
        | tupledef
        | uniondef
        | compoundtype
        | generictype
        | Tellipsis {$$ = mktype($1->line, Tyvalist);}
        ;

generictype
        : Ttyparam {$$ = mktyparam($1->line, $1->str);}
        | Ttyparam Twith name {
                $$ = mktyparam($1->line, $1->str);
                addtrait($$, $3->name.name);
            }
        | Ttyparam Twith Toparen typaramlist Tcparen {
                size_t i;
                $$ = mktyparam($1->line, $1->str);
                for (i = 0; i < $4.nn; i++)
                    addtrait($$, $4.nl[i]->name.name);
            }
        ;

typaramlist
        : name {
                $$.nl = NULL; $$.nn = 0;
                lappend(&$$.nl, &$$.nn, $1);
            }
        | typaramlist Tcomma name {lappend(&$$.nl, &$$.nn, $3);}
        ;

compoundtype
        : functype   {$$ = $1;}
        | type Tosqbrac Tcolon Tcsqbrac {$$ = mktyslice($2->line, $1);}
        | type Tosqbrac expr Tcsqbrac {$$ = mktyarray($2->line, $1, $3);}
        | type Tderef {$$ = mktyptr($2->line, $1);}
        | Tat Tident {$$ = mktyparam($1->line, $2->str);}
        | name       {$$ = mktyunres($1->line, $1, NULL, 0);}
        | name Toparen typelist Tcparen {$$ = mktyunres($1->line, $1, $3.types, $3.ntypes);}
        ;

functype: Toparen funcsig Tcparen {$$ = $2;}
        ;

funcsig : argdefs
            {$$ = mktyfunc($1.line, $1.nl, $1.nn, mktyvar($1.line));}
        | argdefs Tret type
            {$$ = mktyfunc($1.line, $1.nl, $1.nn, $3);}
        ;

argdefs : declcore {
                $$.line = $1->line;
                $$.nl = NULL;
                $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);
            }
        | argdefs Tcomma declcore {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */ {
                $$.line = line;
                $$.nl = NULL;
                $$.nn = 0;
            }
        ;

tupledef: Tosqbrac typelist Tcsqbrac
            {$$ = mktytuple($1->line, $2.types, $2.ntypes);}
        ;

typelist: type {
                $$.types = NULL; $$.ntypes = 0;
                lappend(&$$.types, &$$.ntypes, $1);
            }
        | typelist Tcomma type
            {lappend(&$$.types, &$$.ntypes, $3);}
        ;

structdef
        : Tstruct structbody Tendblk
            {$$ = mktystruct($1->line, $2.nl, $2.nn);}
        ;

structbody
        : structent {
                if ($1) {
                    $$.nl = NULL;
                    $$.nn = 0;
                    lappend(&$$.nl, &$$.nn, $1);
                }
            }
        | structbody structent {
                if ($2)
                    lappend(&$$.nl, &$$.nn, $2);
            }
        ;

structent
        : declcore Tendln {$$ = $1;}
        | visdef Tendln {$$ = NULL;}
        | Tendln {$$ = NULL;}
        ;

uniondef
        : Tunion unionbody Tendblk
            {$$ = mktyunion($1->line, $2.ucl, $2.nucl);}
        ;

unionbody
        : unionelt {
                $$.ucl = NULL;
                $$.nucl = 0;
                if ($1)
                    lappend(&$$.ucl, &$$.nucl, $1);
             }
        | unionbody unionelt {
                if ($2)
                    lappend(&$$.ucl, &$$.nucl, $2);
            }
        ;

unionelt /* nb: the ucon union type gets filled in when we have context */
        : Ttick name type Tendln {$$ = mkucon($2->line, $2, NULL, $3);}
        | Ttick name Tendln {$$ = mkucon($2->line, $2, NULL, NULL);}
        | visdef Tendln {$$ = NULL;}
        | Tendln {$$ = NULL;}
        ;

goto	: Tgoto Tident {$$ = mkexpr($1->line, Ojmp, mklbl($2->line, $2->str), NULL);}
     	;

retexpr : Tret expr {$$ = mkexpr($1->line, Oret, $2, NULL);}
        | Tret {$$ = mkexpr($1->line, Oret, NULL);}
        | expr
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

castexpr: castexpr Tcast Toparen type Tcparen {
                $$ = mkexpr($1->line, Ocast, $1, NULL);
                $$->expr.type = $4;
            }
        | unionexpr 
        ;

unionexpr
        : Ttick name unionexpr {$$ = mkexpr($1->line, Oucon, $2, $3, NULL);}
        | Ttick name {$$ = mkexpr($1->line, Oucon, $2, NULL);}
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

mulop   : Tmul | Tdiv | Tmod
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
        | postfixexpr Tderef
            {$$ = mkexpr($1->line, Oderef, $1, NULL);}
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
        | literal
        | Toparen expr Tcparen
            {$$ = $2;}
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
        | expr {
                $$.nl = NULL; $$.nn = 0;
                lappend(&$$.nl, &$$.nn, $1);
            }
        | tuprest Tcomma expr {lappend(&$$.nl, &$$.nn, $3);}
        ;

literal : funclit       {$$ = mkexpr($1->line, Olit, $1, NULL);}
        | littok        {$$ = mkexpr($1->line, Olit, $1, NULL);}
        | seqlit        {$$ = $1;}
        | tuplit        {$$ = $1;}
        ;

tuplit  : Toparen tupbody Tcparen
            {$$ = mkexprl($1->line, Otup, $2.nl, $2.nn);}

littok  : Tstrlit       {$$ = mkstr($1->line, $1->str);}
        | Tchrlit       {$$ = mkchar($1->line, $1->chrval);}
        | Tfloatlit     {$$ = mkfloat($1->line, $1->fltval);}
        | Tboollit      {$$ = mkbool($1->line, !strcmp($1->str, "true"));}
        | Tintlit {
                $$ = mkint($1->line, $1->intval);
		if ($1->inttype)
                    $$->lit.type = mktype($1->line, $1->inttype);
            }
        ;

funclit : Tobrace params Tendln blkbody Tcbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, mktyvar($3->line), $4);}
        | Tobrace params Tret type Tendln blkbody Tcbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, $4, $6);}
        ;

params  : declcore {
                $$.nl = NULL;
                $$.nn = 0;
                lappend(&$$.nl, &$$.nn, $1);
            }
        | params Tcomma declcore {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */ {$$.nl = NULL; $$.nn = 0;}
        ;

seqlit  : Tosqbrac arrayelts Tcsqbrac
            {$$ = mkexprl($1->line, Oarr, $2.nl, $2.nn);}
        | Tosqbrac structelts Tcsqbrac
            {$$ = mkexprl($1->line, Ostruct, $2.nl, $2.nn);}
        | Tosqbrac Tcsqbrac /* [] is the empty array. */
            {$$ = mkexprl($1->line, Oarr, NULL, 0);}
        ;

arrayelts
        : optendlns arrayelt {
                $$.nl = NULL;
                $$.nn = 0;
                lappend(&$$.nl, &$$.nn, mkidxinit($2->line, mkint($2->line, 0), $2));
            }
        | arrayelts Tcomma optendlns arrayelt
             {lappend(&$$.nl, &$$.nn, mkidxinit($4->line, mkint($4->line, $$.nn), $4));}
        | arrayelts Tcomma optendlns
        ;

arrayelt: expr optendlns {$$ = $1;}
        ;

structelts
        : structelt {
                $$.nl = NULL;
                $$.nn = 0;
                lappend(&$$.nl, &$$.nn, $1);
            }
        | structelts Tcomma structelt
             {lappend(&$$.nl, &$$.nn, $3);}
        ;

structelt: optendlns Tdot Tident Tasn expr optendlns 
            {$$ = mkidxinit($2->line, mkname($3->line, $3->str), $5);}
         ;

optendlns  : /* none */
        | optendlns Tendln
        ;

stmt    : goto
        | break
        | continue
        | retexpr
        | label
        | ifstmt
        | forstmt
        | whilestmt
        | matchstmt
        | /* empty */ {$$ = NULL;}
        ;

break   : Tbreak
     	    {$$ = mkexpr($1->line, Obreak, NULL);}
        ;

continue   : Tcontinue
     	    {$$ = mkexpr($1->line, Ocontinue, NULL);}
        ;

forstmt : Tfor optexprln optexprln optexprln block
            {$$ = mkloopstmt($1->line, $2, $3, $4, $5);}
        | Tfor expr Tin exprln block 
            {$$ = mkiterstmt($1->line, $2, $4, $5);}
        /* FIXME: allow decls in for loops
        | Tfor decl Tendln optexprln optexprln block
            {$$ = mkloopstmt($1->line, $2, $4, $5, $6);}
        */
        ;

whilestmt
        : Twhile exprln block
            {$$ = mkloopstmt($1->line, NULL, $2, NULL, $3);}
        ;

ifstmt  : Tif exprln blkbody elifs
            {$$ = mkifstmt($1->line, $2, $3, $4);}
        ;

elifs   : Telif exprln blkbody elifs
            {$$ = mkifstmt($1->line, $2, $3, $4);}
        | Telse block
            {$$ = $2;}
        | Tendblk
            {$$ = NULL;}
        ;

matchstmt: Tmatch exprln optendlns Tbor matches Tendblk
            {$$ = mkmatchstmt($1->line, $2, $5.nl, $5.nn);}
         ;

matches : match {
                $$.nl = NULL;
                $$.nn = 0;
                if ($1)
                    lappend(&$$.nl, &$$.nn, $1);
            }
        | matches Tbor match {
                if ($2)
                    lappend(&$$.nl, &$$.nn, $3);
            }
        ;

match   : expr Tcolon blkbody Tendln {$$ = mkmatch($1->line, $1, $3);}
        ;

block   : blkbody Tendblk
        ;

blkbody : decl {
                size_t i;
                $$ = mkblock(line, mkstab());
                for (i = 0; i < $1.nn; i++) {
                    putdcl($$->block.scope, $1.nl[i]);
                    lappend(&$$->block.stmts, &$$->block.nstmts, $1.nl[i]);
                }
            }
        | stmt {
                $$ = mkblock(line, mkstab());
                if ($1)
                    lappend(&$$->block.stmts, &$$->block.nstmts, $1);
            }
        | blkbody Tendln stmt {
                if ($3)
                    lappend(&$1->block.stmts, &$1->block.nstmts, $3);
                $$ = $1;
            }
        | blkbody Tendln decl {
                size_t i;
                for (i = 0; i < $3.nn; i++){
                    putdcl($$->block.scope, $3.nl[i]);
                    lappend(&$1->block.stmts, &$1->block.nstmts, $3.nl[i]);
                }
            }
        ;

label   : Tcolon Tident
            {$$ = mklbl($2->line, $2->str);}
        ;

%%

static void addtrait(Type *t, char *str)
{
    size_t i;

    for (i = 0; i < ntraittab; i++) {
        if (!strcmp(namestr(traittab[i]->name), str)) {
            settrait(t, traittab[i]);
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

    if (!t)
        return;
    b = tybase(t);
    switch (b->type) {
        case Tystruct:
            for (i = 0; i < b->nmemb; i++)
                installucons(st, b->sdecls[i]->decl.type);
            break;
        case Tyunion:
            for (i = 0; i < b->nmemb; i++) {
                b->udecls[i]->utype = t;
                b->udecls[i]->id = i;
                putucon(st, b->udecls[i]);
            }
            break;
        default:
            break;
    }
}

void yyerror(const char *s)
{
    fprintf(stderr, "%s:%d: %s", filename, line, s);
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
        case Tmul:      o = Omul;       break;
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

