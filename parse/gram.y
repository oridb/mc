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
Op binop(int toktype);
Stab *curscope;
//int n = 0;
%}

%token<tok> TError
%token<tok> TPlus    /* + */
%token<tok> TMinus   /* - */
%token<tok> TStar    /* * */
%token<tok> TDiv     /* / */
%token<tok> TInc     /* ++ */
%token<tok> TDec     /* -- */
%token<tok> TMod     /* % */
%token<tok> TAsn     /* = */
%token<tok> TAddeq   /* += */
%token<tok> TSubeq   /* -= */
%token<tok> TMuleq   /* *= */
%token<tok> TDiveq   /* /= */
%token<tok> TModeq   /* %= */
%token<tok> TBoreq   /* |= */
%token<tok> TBxoreq  /* ^= */
%token<tok> TBandeq  /* &= */
%token<tok> TBsleq   /* <<= */
%token<tok> TBsreq   /* >>= */

%token<tok> TBor     /* | */
%token<tok> TBxor    /* ^ */
%token<tok> TBand    /* & */
%token<tok> TBsl     /* << */
%token<tok> TBsr     /* >> */
%token<tok> TBnot    /* ~ */

%token<tok> TEq      /* == */
%token<tok> TGt      /* > */
%token<tok> TLt      /* < */
%token<tok> TGe      /* >= */
%token<tok> TLe      /* <= */
%token<tok> TNe      /* != */

%token<tok> TLor     /* || */
%token<tok> TLand    /* && */
%token<tok> TLnot    /* ! */

%token<tok> TObrace  /* { */
%token<tok> TCbrace  /* } */
%token<tok> TOparen  /* ( */
%token<tok> TCparen  /* ) */
%token<tok> TOsqbrac /* [ */
%token<tok> TCsqbrac /* ] */
%token<tok> TAt      /* @ */

%token<tok> TType            /* type */
%token<tok> TFor             /* for */
%token<tok> TWhile           /* while */
%token<tok> TIf              /* if */
%token<tok> TElse            /* else */
%token<tok> TElif            /* else */
%token<tok> TMatch           /* match */
%token<tok> TDefault /* default */
%token<tok> TGoto            /* goto */

%token<tok> TIntlit
%token<tok> TStrlit
%token<tok> TFloatlit
%token<tok> TChrlit
%token<tok> TBoollit

%token<tok> TEnum    /* enum */
%token<tok> TStruct  /* struct */
%token<tok> TUnion   /* union */

%token<tok> TConst   /* const */
%token<tok> TVar             /* var */
%token<tok> TExtern  /* extern */

%token<tok> TExport  /* export */
%token<tok> TProtect /* protect */

%token<tok> TEllipsis        /* ... */
%token<tok> TEndln           /* ; or \n */
%token<tok> TEndblk  /* ;; */
%token<tok> TColon   /* : */
%token<tok> TDot             /* . */
%token<tok> TComma   /* , */
%token<tok> TRet             /* -> */
%token<tok> TUse             /* use */
%token<tok> TPkg             /* pkg */
%token<tok> TSizeof  /* sizeof */

%token<tok> TIdent
%token<tok> TEof

%start module

%type <ty> type structdef uniondef enumdef compoundtype functype funcsig

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
            {die("tydef unimplemented");}
        | TEndln
        ;

decl    : TVar declbody TEndln
            {$2->decl.flags = 0;
             $$ = $2;}
        | TConst declbody TEndln
            {$2->decl.sym->isconst = 1;
             $2->decl.flags = 0;
             $$ = $2;}
        | TExtern TVar declbody TEndln
            {$3->decl.flags = Dclextern;
             $$ = $3;}
        | TExtern TConst declbody TEndln
            {$3->decl.flags = Dclconst | Dclextern;
             $$ = $3;}
        ;

use     : TUse TIdent TEndln
            {$$ = mkuse($1->line, $2->str, 0);}
        | TUse TStrlit TEndln
            {$$ = mkuse($1->line, $2->str, 1);}
        ;

package : TPkg TIdent TAsn pkgbody TEndblk
        ;


pkgbody : pkgitem
        | pkgbody pkgitem
        ;

pkgitem : decl {putdcl(file->file.exports, $1->decl.sym);}
        | tydef {puttype(file->file.exports, 
                         mkname($1.line, $1.name),
                         $1.type);}
        | visdef {die("Unimplemented visdef");}
        | TEndln
        ;

visdef  : TExport TColon
        | TProtect TColon
        ;


declbody: declcore TAsn expr
            {$$ = $1; $1->decl.init = $3;}
        | declcore
        ;

declcore: name
            {$$ = mkdecl($1->line, mksym($1->line, $1, mktyvar($1->line)));}
        | name TColon type
            {$$ = mkdecl($1->line, mksym($1->line, $1, $3));}
        ;

name    : TIdent
            {$$ = mkname($1->line, $1->str);}
        | TIdent TDot name
            {$$ = $3; setns($3, $1->str);}
        ;

tydef   : TType TIdent TAsn type TEndln
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = $4;}
        | TType TIdent TEndln
            {$$.line = $1->line;
             $$.name = $2->str;
             $$.type = NULL;}
        ;

type    : structdef
        | uniondef
        | enumdef
        | compoundtype
        ;

compoundtype
        : functype   {$$ = $1;}
        | type TOsqbrac TComma TCsqbrac {$$ = mktyslice($2->line, $1);}
        | type TOsqbrac expr TCsqbrac {$$ = mktyarray($2->line, $1, $3);}
        | type TStar {$$ = mktyptr($2->line, $1);}
        | name       {$$ = mktynamed($1->line, $1);}
        | TAt TIdent {$$ = mktyparam($1->line, $2->str);}
        ;

functype: TOparen funcsig TCparen {$$ = $2;}
        ;

funcsig : argdefs
            {$$ = mktyfunc($1.line, $1.nl, $1.nn, mktyvar($1.line));}
        | argdefs TRet type
            {$$ = mktyfunc($1.line, $1.nl, $1.nn, $3);}
        ;

argdefs : declcore
            {$$.line = $1->line;
             $$.nl = NULL;
             $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | argdefs TComma declcore
            {lappend(&$$.nl, &$$.nn, $3);}
        ;

structdef
        : TStruct structbody TEndblk
            {$$ = mktystruct($1->line, $2.nl, $2.nn);}
        ;

structbody
        : structelt
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | structbody structelt
            {if ($2) {lappend(&$$.nl, &$$.nn, $2);}}
        ;

structelt
        : declcore TEndln
            {$$ = $1;}
        | visdef TEndln
            {$$ = NULL;}
        | TEndln
            {$$ = NULL;}
        ;

uniondef
        : TUnion unionbody TEndblk
            {$$ = mktyunion($1->line, $2.nl, $2.nn);}
        ;

unionbody
        : unionelt
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | unionbody unionelt
            {if ($2) {lappend(&$$.nl, &$$.nn, $2);}}
        ;

unionelt
        : TIdent type TEndln
            {$$ = NULL; die("unionelt impl");}
        | visdef TEndln
            {$$ = NULL;}
        | TEndln
            {$$ = NULL;}
        ;

enumdef : TEnum enumbody TEndblk
            {$$ = mktyenum($1->line, $2.nl, $2.nn);}
        ;

enumbody: enumelt
            {$$.nl = NULL; $$.nn = 0; if ($1) lappend(&$$.nl, &$$.nn, $1);}
        | enumbody enumelt
            {if ($2) {lappend(&$$.nl, &$$.nn, $2);}}
        ;

enumelt : TIdent TEndln
            {$$ = NULL; die("enumelt impl");}
        | TIdent TAsn exprln
            {$$ = NULL; die("enumelt impl");}
        | TEndln
            {$$ = NULL;}
        ;

retexpr : TRet exprln
            {$$ = mkexpr($1->line, Oret, $2, NULL);}
        | exprln
        ;

exprln  : expr TEndln
        ;

expr    : asnexpr
        ;

asnexpr : lorexpr asnop asnexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | lorexpr
        ;

asnop   : TAsn
        | TAddeq        /* += */
        | TSubeq        /* -= */
        | TMuleq        /* *= */
        | TDiveq        /* /= */
        | TModeq        /* %= */
        | TBoreq        /* |= */
        | TBxoreq       /* ^= */
        | TBandeq       /* &= */
        | TBsleq        /* <<= */
        | TBsreq        /* >>= */
        ;

lorexpr : lorexpr TLor landexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | landexpr
        ;

landexpr: landexpr TLand borexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | borexpr
        ;

borexpr : borexpr TBor bandexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | bandexpr
        ;

bandexpr: bandexpr TBand cmpexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | cmpexpr
        ;

cmpexpr : cmpexpr cmpop addexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | addexpr
        ;

cmpop   : TEq | TGt | TLt | TGe | TLe | TNe ;

addexpr : addexpr addop mulexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | mulexpr
        ;

addop   : TPlus | TMinus ;

mulexpr : mulexpr mulop shiftexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | shiftexpr
        ;

mulop   : TStar | TDiv | TMod
        ;

shiftexpr
        : shiftexpr shiftop prefixexpr
            {$$ = mkexpr($1->line, binop($2->type), $1, $3, NULL);}
        | prefixexpr
        ;

shiftop : TBsl | TBsr;

prefixexpr
        : TInc postfixexpr      {$$ = mkexpr($1->line, Opreinc, $2, NULL);}
        | TDec postfixexpr      {$$ = mkexpr($1->line, Opredec, $2, NULL);}
        | TStar postfixexpr     {$$ = mkexpr($1->line, Oderef, $2, NULL);}
        | TBand postfixexpr     {$$ = mkexpr($1->line, Oaddr, $2, NULL);}
        | TLnot postfixexpr     {$$ = mkexpr($1->line, Olnot, $2, NULL);}
        | TBnot postfixexpr     {$$ = mkexpr($1->line, Obnot, $2, NULL);}
        | TMinus postfixexpr    {$$ = mkexpr($1->line, Oneg, $2, NULL);}
        | TPlus postfixexpr     {$$ = $2;} /* positive is a nop */
        | postfixexpr
        ;

postfixexpr
        : postfixexpr TDot TIdent
            {$$ = mkexpr($1->line, Omemb, $1, mkname($3->line, $3->str), NULL);}
        | postfixexpr TInc
            {$$ = mkexpr($1->line, Opostinc, $1, NULL);}
        | postfixexpr TDec
            {$$ = mkexpr($1->line, Opostdec, $1, NULL);}
        | postfixexpr TOsqbrac expr TCsqbrac
            {$$ = mkexpr($1->line, Oidx, $1, $3);}
        | postfixexpr TOsqbrac expr TComma expr TCsqbrac
            {$$ = mkexpr($1->line, Oslice, $1, $3, $5, NULL);}
        | postfixexpr TOparen arglist TCparen
            {$$ = mkcall($1->line, $1, $3.nl, $3.nn);}
        | atomicexpr
        ;

arglist : asnexpr
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | arglist TComma asnexpr
            {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */
            {$$.nl = NULL; $$.nn = 0;}
        ;

atomicexpr
        : TIdent
            {$$ = mkexpr($1->line, Ovar, mkname($1->line, $1->str), NULL);}
        | literal {$$ = mkexpr($1->line, Olit, $1, NULL);}
        | TOparen expr TCparen
            {$$ = $2;}
        | TSizeof atomicexpr
            {$$ = mkexpr($1->line, Osize, $2, NULL);}
        ;

literal : funclit       {$$ = $1;}
        | arraylit      {$$ = $1;}
        | TStrlit       {$$ = mkstr($1->line, $1->str);}
        | TIntlit       {$$ = mkint($1->line, strtol($1->str, NULL, 0));}
        | TChrlit       {$$ = mkchar($1->line, *$1->str);} /* FIXME: expand escapes, unicode  */
        | TFloatlit     {$$ = mkfloat($1->line, strtod($1->str, NULL));}
        | TBoollit      {$$ = mkbool($1->line, !strcmp($1->str, "true"));}
        ;

funclit : TObrace params TEndln blockbody TCbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, mktyvar($3->line), $4);}
        | TObrace params TRet type TEndln blockbody TCbrace
            {$$ = mkfunc($1->line, $2.nl, $2.nn, $4, $6);}
        ;

params  : declcore
            {$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
        | params TComma declcore
            {lappend(&$$.nl, &$$.nn, $3);}
        | /* empty */
            {$$.nl = NULL; $$.nn = 0;}
        ;

arraylit : TOsqbrac arraybody TCsqbrac
            {$$ = NULL; die("Unimpl arraylit");}
         ;

arraybody
        : expr
        | arraybody TComma expr
        ;

stmt    : decl
        | retexpr
        | label
        | ifstmt
        | forstmt
        | whilestmt
        | TEndln {$$ = NULL;}
        ;

forstmt : TFor optexprln optexprln optexprln block
            {$$ = mkloop($1->line, $2, $3, $4, $5);}
        | TFor decl optexprln optexprln block
            {$$ = mkloop($1->line, $2, $3, $4, $5);}
        ;

optexprln: exprln {$$ = $1;}
         | TEndln {$$ = NULL;}
         ;

whilestmt
        : TWhile exprln block
            {$$ = mkloop($1->line, NULL, $2, NULL, $3);}
        ;

ifstmt  : TIf exprln blockbody elifs
            {$$ = mkif($1->line, $2, $3, $4);}
        ;

elifs   : TElif exprln blockbody elifs
            {$$ = mkif($1->line, $2, $3, $4);}
        | TElse blockbody TEndblk
            {$$ = $2;}
        | TEndblk
            {$$ = NULL;}
        ;

block   : blockbody TEndblk
        ;

blockbody
        : stmt
            {$$ = mkblock(line, mkstab());
             if ($1)
                lappend(&$$->block.stmts, &$$->block.nstmts, $1);}
        | blockbody stmt
            {if ($2)
                lappend(&$$->block.stmts, &$$->block.nstmts, $2);}
        ;

label   : TColon TIdent
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

Op binop(int tt)
{
    Op o;

    o = Obad;
    switch (tt) {
        case TPlus:     o = Oadd;       break;
        case TMinus:    o = Osub;       break;
        case TStar:     o = Omul;       break;
        case TDiv:      o = Odiv;       break;
        case TMod:      o = Omod;       break;
        case TAsn:      o = Oasn;       break;
        case TAddeq:    o = Oaddeq;     break;
        case TSubeq:    o = Osubeq;     break;
        case TMuleq:    o = Omuleq;     break;
        case TDiveq:    o = Odiveq;     break;
        case TModeq:    o = Omodeq;     break;
        case TBoreq:    o = Oboreq;     break;
        case TBxoreq:   o = Obxoreq;    break;
        case TBandeq:   o = Obandeq;    break;
        case TBsleq:    o = Obsleq;     break;
        case TBsreq:    o = Obsreq;     break;
        case TBor:      o = Obor;       break;
        case TBxor:     o = Obxor;      break;
        case TBand:     o = Oband;      break;
        case TBsl:      o = Obsl;       break;
        case TBsr:      o = Obsr;       break;
        case TEq:       o = Oeq;        break;
        case TGt:       o = Ogt;        break;
        case TLt:       o = Olt;        break;
        case TGe:       o = Oge;        break;
        case TLe:       o = Ole;        break;
        case TNe:       o = One;        break;
        case TLor:      o = Olor;       break;
        case TLand:     o = Oland;      break;
        default:
            die("Unimplemented binop\n");
            break;
    }
    return o;
}

