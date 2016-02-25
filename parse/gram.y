%{
#define YYERROR_VERBOSE
#define YYDEBUG 1

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "parse.h"


Stab *curscope;
static Node **lbls;
static size_t nlbls;

void yyerror(const char *s);
int yylex(void);

static Op binop(int toktype);
static Node *mkpseudodecl(Srcloc l, Type *t);
static void installucons(Stab *st, Type *t);
static void addtrait(Type *t, char *str);
static void setattrs(Node *dcl, char **attrs, size_t nattrs);
static void setupinit(Node *n);

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
%token<tok> Tvoidlit

%token<tok> Ttrait   /* trait */
%token<tok> Timpl   /* trait */
%token<tok> Tstruct  /* struct */
%token<tok> Tunion   /* union */
%token<tok> Ttyparam /* @typename */

%token<tok> Tconst   /* const */
%token<tok> Tvar     /* var */
%token<tok> Tgeneric /* var */
%token<tok> Tcast    /* castto */

%token<tok> Tgap     /* _ */
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
%token<tok> Tattr    /* $attr */
%token<tok> Tsizeof  /* sizeof */

%token<tok> Tident

%start file

%type <ty> type structdef uniondef tupledef compoundtype functype funcsig
%type <ty> generictype
%type <tylist> typelist typarams optauxtypes
%type <nodelist> typaramlist

%type <tok> asnop cmpop addop mulop shiftop optident

%type <tydef> tydef pkgtydef typeid
%type <trait> traitdef

%type <node> exprln retexpr goto continue break expr atomicexpr
%type <node> littok literal asnexpr lorexpr landexpr borexpr
%type <node> bandexpr cmpexpr unionexpr addexpr mulexpr shiftexpr prefixexpr postfixexpr
%type <node> funclit seqlit tuplit name block stmt label use
%type <node> fnparam declbody declcore typedeclcore structent arrayelt structelt tuphead
%type <node> ifstmt forstmt whilestmt matchstmt elifs optexprln loopcond optexpr
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
%type <strlist> attrs

%type <uconlist> unionbody

%union {
	struct {
		Srcloc loc;
		Node **nl;
		size_t nn;
	} nodelist;
	struct {
		char **str;
		size_t nstr;
	} strlist;
	struct {
		Srcloc loc;
		Ucon **ucl;
		size_t nucl;
	} uconlist;
	struct {
		Srcloc loc;
		Type **types;
		size_t ntypes;
	} tylist;
	struct { /* FIXME: unused */
		Srcloc loc;
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

toplev	: package
	| use {lappend(&file->file.uses, &file->file.nuses, $1);}
	| implstmt {
		lappend(&file->file.stmts, &file->file.nstmts, $1);
	}
	| traitdef {
		size_t i;
		puttrait(file->file.globls, $1->name, $1);
		for (i = 0; i < $1->nfuncs; i++)
			putdcl(file->file.globls, $1->funcs[i]);
	}
	| tydef {
		puttype(file->file.globls, mkname($1.loc, $1.name), $1.type);
		installucons(file->file.globls, $1.type);
	}
	| decl {
		size_t i;
		Node *n;

		for (i = 0; i < $1.nn; i++) {
			if (!strcmp(declname($1.nl[i]), "__init__"))
				setupinit($1.nl[i]);
			/* putdcl can merge, so we need to getdcl after */
			putdcl(file->file.globls, $1.nl[i]);
			n = getdcl(file->file.globls, $1.nl[i]->decl.name);
			lappend(&file->file.stmts, &file->file.nstmts, n);
			$1.nl[i]->decl.isglobl = 1;
			if ($1.nl[i]->decl.isinit)
				file->file.localinit = $1.nl[i];
		}
	}
	| /* empty */
	;

decl    : attrs Tvar decllist {
		size_t i;

		for (i = 0; i < $3.nn; i++)
			setattrs($3.nl[i], $1.str, $1.nstr);
		$$ = $3;
	}
	| attrs Tconst decllist {
		size_t i;
		for (i = 0; i < $3.nn; i++) {
			setattrs($3.nl[i], $1.str, $1.nstr);
			$3.nl[i]->decl.isconst = 1;
		}
		$$ = $3;
	}
	| attrs Tgeneric decllist {
		size_t i;

		for (i = 0; i < $3.nn; i++) {
			setattrs($3.nl[i], $1.str, $1.nstr);
			$3.nl[i]->decl.isconst = 1;
			$3.nl[i]->decl.isgeneric = 1;
		}
		$$ = $3;
	}
        ;

attrs   : /* empty */ {$$.nstr = 0; $$.str = NULL;}
	| Tattr attrs {
		$$ = $2;
		lappend(&$$.str, &$$.nstr, strdup($1->id));
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

use     : Tuse Tident {$$ = mkuse($1->loc, $2->id, 0);}
	| Tuse Tstrlit {$$ = mkuse($1->loc, $2->strval.buf, 1);}
	;

optident: Tident      {$$ = $1;}
	| /* empty */ {$$ = NULL;}
	;

package : Tpkg optident Tasn pkgbody Tendblk {
		if (file->file.globls->name)
			lfatal($1->loc, "Package already declared\n");
		if ($2) {
			updatens(file->file.globls, $2->id);
		}
	}
	;

pkgbody : pkgitem
	| pkgbody Tendln pkgitem
	;

pkgitem : decl {
		size_t i;
		for (i = 0; i < $1.nn; i++) {
			$1.nl[i]->decl.vis = Visexport;
			$1.nl[i]->decl.isglobl = 1;
			putdcl(file->file.globls, $1.nl[i]);
			if ($1.nl[i]->decl.init)
				lappend(&file->file.stmts, &file->file.nstmts, $1.nl[i]);
		}
	}
	| pkgtydef {
		/* the type may only be null in a package context, so we
		can set the type when merging in this case.
		
		FIXME: clean up the fucking special cases. */
		if ($1.type)
			$1.type->vis = Visexport;
		puttype(file->file.globls, mkname($1.loc, $1.name), $1.type);
		installucons(file->file.globls, $1.type);
	}
	| traitdef {
		size_t i;
		$1->vis = Visexport;
		puttrait(file->file.globls, $1->name, $1);
		for (i = 0; i < $1->nfuncs; i++)
			putdcl(file->file.globls, $1->funcs[i]);
	}
	| implstmt {
		$1->impl.vis = Visexport;
		lappend(&file->file.stmts, &file->file.nstmts, $1);
	}
	| /* empty */
	;

pkgtydef: attrs tydef {
		size_t i;
		$$ = $2;
		for (i = 0; i < $1.nstr; i++) {
			if (!strcmp($1.str[i], "pkglocal"))
				$$.type->ispkglocal = 1;
			else
				lfatal($$.loc, "invalid type attribute '%s'", $1.str[i]);
		}
	}
	;

declbody: declcore Tasn expr {$$ = $1; $1->decl.init = $3;}
	| declcore
	;

declcore: name {$$ = mkdecl($1->loc, $1, mktyvar($1->loc));}
	| typedeclcore {$$ = $1;}
	;

typedeclcore
	: name Tcolon type {$$ = mkdecl($1->loc, $1, $3);}
	;

name    : Tident {$$ = mkname($1->loc, $1->id);}
	| Tident Tdot name {$$ = $3; setns($3, $1->id);}
	;

implstmt: Timpl name type optauxtypes {
		$$ = mkimplstmt($1->loc, $2, $3, $4.types, $4.ntypes, NULL, 0);
		$$->impl.isproto = 1;
	}
	| Timpl name type optauxtypes Tasn Tendln implbody Tendblk {
		$$ = mkimplstmt($1->loc, $2, $3, $4.types, $4.ntypes, $7.nl, $7.nn);
	}
	;

implbody
	: optendlns {$$.nl = NULL; $$.nn = 0;}
	| implbody Tident Tasn exprln optendlns {
		Node *d;
		$$ = $1;
		d = mkdecl($2->loc, mkname($2->loc, $2->id), mktyvar($2->loc));
		d->decl.init = $4;
		d->decl.isconst = 1;
		d->decl.isglobl = 1;
		lappend(&$$.nl, &$$.nn, d);
	}
	;

traitdef: Ttrait Tident generictype optauxtypes { /* trait prototype */
		$$ = mktrait($1->loc,
			mkname($2->loc, $2->id), $3,
			$4.types, $4.ntypes,
			NULL, 0,
			NULL, 0,
			1);
	}
	| Ttrait Tident generictype optauxtypes Tasn traitbody Tendblk /* trait definition */ {
		size_t i;
		$$ = mktrait($1->loc,
			mkname($2->loc, $2->id), $3,
			$4.types, $4.ntypes,
			NULL, 0,
			$6.nl, $6.nn,
			0);
		for (i = 0; i < $6.nn; i++) {
			$6.nl[i]->decl.trait = $$;
			$6.nl[i]->decl.impls = mkht(tyhash, tyeq); 
			$6.nl[i]->decl.isgeneric = 1;
		}
	}
	;

optauxtypes
	: Tret typelist {$$ = $2;}
	| /* empty */ { $$.types = NULL; $$.ntypes = 0; }
	;

traitbody
	: optendlns {$$.nl = NULL; $$.nn = 0;}
	| traitbody Tident Tcolon type optendlns {
		Node *d;
		$$ = $1;
		d = mkdecl($2->loc, mkname($2->loc, $2->id), $4);
		d->decl.isgeneric = 1;
		d->decl.isconst = 1;
		lappend(&$$.nl, &$$.nn, d);
	}
	;


tydef   : Ttype typeid {$$ = $2;}
	| Ttype typeid Tasn type {
		$$ = $2;
		if ($$.nparams == 0) {
			$$.type = mktyname($2.loc, mkname($2.loc, $2.name), $4);
		} else {
			$$.type = mktygeneric($2.loc, mkname($2.loc, $2.name), $2.params, $2.nparams, $4);
		}
	}
	;

typeid  : Tident {
		$$.loc = $1->loc;
		$$.name = $1->id;
		$$.params = NULL;
		$$.type = NULL;
	}
	| Tident Toparen typarams Tcparen {
		$$.loc = $1->loc;
		$$.name = $1->id;
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
	| Tellipsis {$$ = mktype($1->loc, Tyvalist);}
	;

generictype
	: Ttyparam {$$ = mktyparam($1->loc, $1->id);}
	| Ttyparam Twith name {
		$$ = mktyparam($1->loc, $1->id);
		addtrait($$, $3->name.name);
	}
	| Ttyparam Twith Toparen typaramlist Tcparen {
		size_t i;
		$$ = mktyparam($1->loc, $1->id);
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
	| type Tosqbrac Tcolon Tcsqbrac {$$ = mktyslice($2->loc, $1);}
	| type Tosqbrac expr Tcsqbrac {$$ = mktyarray($2->loc, $1, $3);}
	| type Tosqbrac Tellipsis Tcsqbrac {$$ = mktyarray($2->loc, $1, NULL);}
	| name Toparen typelist Tcparen {$$ = mktyunres($1->loc, $1, $3.types, $3.ntypes);}
	| type Tderef	{$$ = mktyptr($2->loc, $1);}
	| Tat Tident	{$$ = mktyparam($1->loc, $2->id);}
	| Tvoidlit	{$$ = mktyunres($1->loc, mkname($1->loc, $1->id), NULL, 0);}
	| name	{$$ = mktyunres($1->loc, $1, NULL, 0);}
	;

functype: Toparen funcsig Tcparen {$$ = $2;}
	;

funcsig : argdefs Tret type
	{$$ = mktyfunc($2->loc, $1.nl, $1.nn, $3);}
	;

argdefs : typedeclcore {
		$$.loc = $1->loc;
		$$.nl = NULL;
		$$.nn = 0; lappend(&$$.nl, &$$.nn, $1);
	}
	| argdefs Tcomma declcore {lappend(&$$.nl, &$$.nn, $3);}
	| /* empty */ {
		$$.loc.line = 0;
		$$.loc.file = 0;
		$$.nl = NULL;
		$$.nn = 0;
	}
	;

tupledef: Toparen typelist Tcparen
	{$$ = mktytuple($1->loc, $2.types, $2.ntypes);}
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
	{$$ = mktystruct($1->loc, $2.nl, $2.nn);}
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
	| Tendln {$$ = NULL;}
	;

uniondef
	: Tunion unionbody Tendblk
	{$$ = mktyunion($1->loc, $2.ucl, $2.nucl);}
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
	: Ttick name type Tendln {$$ = mkucon($2->loc, $2, NULL, $3);}
	| Ttick name Tendln {$$ = mkucon($2->loc, $2, NULL, NULL);}
	| Tendln {$$ = NULL;}
	;

goto    : Tgoto Tident {
		Node *lbl;

		lbl = mklbl($2->loc, "");
		lbl->expr.args[0]->lit.lblname = strdup($2->id);
		$$ = mkexpr($1->loc, Ojmp, lbl, NULL);
	}
	;

retexpr : Tret expr {$$ = mkexpr($1->loc, Oret, $2, NULL);}
	| expr
	;

optexpr : expr {$$ = $1;}
	| /* empty */ {$$ = NULL;}
	;

loopcond: exprln {$$ = $1;}
	| Tendln {$$ = mkboollit($1->loc, 1);}
	;

optexprln
	: exprln {$$ = $1;}
	| Tendln {$$ = NULL;}
	;

exprln  : expr Tendln
	;

expr    : asnexpr
	;

asnexpr : lorexpr asnop asnexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
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
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| landexpr
	;

landexpr: landexpr Tland cmpexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| cmpexpr
	;

cmpexpr : cmpexpr cmpop castexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| unionexpr
	;

cmpop   : Teq | Tgt | Tlt | Tge | Tle | Tne ;

unionexpr
	: Ttick name unionexpr {$$ = mkexpr($1->loc, Oucon, $2, $3, NULL);}
	| Ttick name {$$ = mkexpr($1->loc, Oucon, $2, NULL);}
	| castexpr
	;

castexpr: castexpr Tcast Toparen type Tcparen {
		$$ = mkexpr($1->loc, Ocast, $1, NULL);
		$$->expr.type = $4;
	}
	| borexpr
	;


borexpr : borexpr Tbor bandexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| borexpr Tbxor bandexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| bandexpr
	;

bandexpr: bandexpr Tband addexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| addexpr
	;

addexpr : addexpr addop mulexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| mulexpr
	;

addop   : Tplus | Tminus ;

mulexpr : mulexpr mulop shiftexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| shiftexpr
	;

mulop   : Tmul | Tdiv | Tmod
	;

shiftexpr
	: shiftexpr shiftop prefixexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| prefixexpr
	;

shiftop : Tbsl | Tbsr;

prefixexpr
	: Tinc prefixexpr      {$$ = mkexpr($1->loc, Opreinc, $2, NULL);}
	| Tdec prefixexpr      {$$ = mkexpr($1->loc, Opredec, $2, NULL);}
	| Tband prefixexpr     {$$ = mkexpr($1->loc, Oaddr, $2, NULL);}
	| Tlnot prefixexpr     {$$ = mkexpr($1->loc, Olnot, $2, NULL);}
	| Tbnot prefixexpr     {$$ = mkexpr($1->loc, Obnot, $2, NULL);}
	| Tminus prefixexpr    {$$ = mkexpr($1->loc, Oneg, $2, NULL);}
	| Tplus prefixexpr     {$$ = $2;} /* positive is a nop */
	| postfixexpr
	;

postfixexpr
	: postfixexpr Tdot Tident
	{$$ = mkexpr($1->loc, Omemb, $1, mkname($3->loc, $3->id), NULL);}
	| postfixexpr Tinc
	{$$ = mkexpr($1->loc, Opostinc, $1, NULL);}
	| postfixexpr Tdec
	{$$ = mkexpr($1->loc, Opostdec, $1, NULL);}
	| postfixexpr Tosqbrac expr Tcsqbrac
	{$$ = mkexpr($1->loc, Oidx, $1, $3, NULL);}
	| postfixexpr Tosqbrac optexpr Tcolon optexpr Tcsqbrac
	{$$ = mksliceexpr($1->loc, $1, $3, $5);}
	| postfixexpr Tderef
	{$$ = mkexpr($1->loc, Oderef, $1, NULL);}
	| postfixexpr Toparen arglist Tcparen
	{$$ = mkcall($1->loc, $1, $3.nl, $3.nn);}
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
	{$$ = mkexpr($1->loc, Ovar, mkname($1->loc, $1->id), NULL);}
	| Tgap
	{$$ = mkexpr($1->loc, Ogap, NULL);}
	| literal
	| Toparen expr Tcparen
	{$$ = $2;}
	| Tsizeof Toparen type Tcparen
	{$$ = mkexpr($1->loc, Osize, mkpseudodecl($1->loc, $3), NULL);}
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

literal : funclit       {$$ = mkexpr($1->loc, Olit, $1, NULL);}
	| littok        {$$ = mkexpr($1->loc, Olit, $1, NULL);}
	| seqlit        {$$ = $1;}
	| tuplit        {$$ = $1;}
	;

tuplit  : Toparen tupbody Tcparen
	{$$ = mkexprl($1->loc, Otup, $2.nl, $2.nn);}

littok  : Tstrlit       {$$ = mkstr($1->loc, $1->strval);}
	| Tchrlit       {$$ = mkchar($1->loc, $1->chrval);}
	| Tfloatlit     {$$ = mkfloat($1->loc, $1->fltval);}
	| Tboollit      {$$ = mkbool($1->loc, !strcmp($1->id, "true"));}
	| Tvoidlit      {$$ = mkvoid($1->loc);}
	| Tintlit {
		$$ = mkint($1->loc, $1->intval);
		if ($1->inttype)
			$$->lit.type = mktype($1->loc, $1->inttype);
	}
	;

funclit : Tobrace params Tendln blkbody Tcbrace {
		size_t i;
		Node *fn, *lit;

		$$ = mkfunc($1->loc, $2.nl, $2.nn, mktyvar($3->loc), $4);
		fn = $$->lit.fnval;
		for (i = 0; i < nlbls; i++) {
			lit = lbls[i]->expr.args[0];
			putlbl(fn->func.scope, lit->lit.lblname, lbls[i]);
		}
		lfree(&lbls, &nlbls);
	}
	| Tobrace params Tret type Tendln blkbody Tcbrace {
		size_t i;
		Node *fn, *lit;

		$$ = mkfunc($1->loc, $2.nl, $2.nn, $4, $6);
		fn = $$->lit.fnval;
		for (i = 0; i < nlbls; i++) {
			lit = lbls[i]->expr.args[0];
			putlbl(fn->func.scope, lbls[i]->lit.lblname, lbls[i]);
		}
		lfree(&lbls, &nlbls);
	}
	;

params  : fnparam {
		$$.nl = NULL;
		$$.nn = 0;
		lappend(&$$.nl, &$$.nn, $1);
	}
	| params Tcomma fnparam {lappend(&$$.nl, &$$.nn, $3);}
	| /* empty */ {$$.nl = NULL; $$.nn = 0;}
	;

fnparam : declcore {$$ = $1;}
	| Tgap { $$ = mkpseudodecl($1->loc, mktyvar($1->loc)); }
	| Tgap Tcolon type { $$ = mkpseudodecl($1->loc, $3); }
	;

seqlit  : Tosqbrac arrayelts optcomma Tcsqbrac
	{$$ = mkexprl($1->loc, Oarr, $2.nl, $2.nn);}
	| Tosqbrac structelts optcomma Tcsqbrac
	{$$ = mkexprl($1->loc, Ostruct, $2.nl, $2.nn);}
	| Tosqbrac optendlns optcomma Tcsqbrac /* [] is the empty array. */
	{$$ = mkexprl($1->loc, Oarr, NULL, 0);}
	;

optcomma: Tcomma optendlns
	| /* empty */
	;

arrayelts
	: optendlns arrayelt {
		$$.nl = NULL;
		$$.nn = 0;
		if ($2->expr.idx)
			lappend(&$$.nl, &$$.nn, $2);
		else
			lappend(&$$.nl, &$$.nn, mkidxinit($2->loc, mkintlit($2->loc, 0), $2));
	}
	| arrayelts Tcomma optendlns arrayelt {
		if ($4->expr.idx)
			lappend(&$$.nl, &$$.nn, $4);
		else
			lappend(&$$.nl, &$$.nn, mkidxinit($4->loc, mkintlit($4->loc, $$.nn), $4));
	}
	;

arrayelt: expr optendlns {$$ = $1;}
	| expr Tcolon expr optendlns {
		$$ = mkidxinit($2->loc, $1, $3);
	}
	;

structelts
	: optendlns structelt {
		$$.nl = NULL;
		$$.nn = 0;
		lappend(&$$.nl, &$$.nn, $2);
	}
	| structelts Tcomma optendlns structelt {
		lappend(&$$.nl, &$$.nn, $4);
	}
	;

structelt: Tdot Tident Tasn expr optendlns {
		$$ = $4;
		mkidxinit($2->loc, mkname($2->loc, $2->id), $4);
	}
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
	{$$ = mkexpr($1->loc, Obreak, NULL);}
	;

continue   : Tcontinue
	{$$ = mkexpr($1->loc, Ocontinue, NULL);}
	;

forstmt : Tfor optexprln loopcond optexprln block
	{$$ = mkloopstmt($1->loc, $2, $3, $4, $5);}
	| Tfor expr Tin exprln block
	{$$ = mkiterstmt($1->loc, $2, $4, $5);}
	| Tfor decl Tendln loopcond optexprln block {
		//Node *init;
		if ($2.nn != 1)
			lfatal($1->loc, "only one declaration is allowed in for loop");
		$$ = mkloopstmt($1->loc, $2.nl[0], $4, $5, $6);
		putdcl($$->loopstmt.scope, $2.nl[0]);
	}
	;

whilestmt
	: Twhile exprln block
	{$$ = mkloopstmt($1->loc, NULL, $2, NULL, $3);}
	;

ifstmt  : Tif exprln blkbody elifs
	{$$ = mkifstmt($1->loc, $2, $3, $4);}
	;

elifs   : Telif exprln blkbody elifs
	{$$ = mkifstmt($1->loc, $2, $3, $4);}
	| Telse block
	{$$ = $2;}
	| Tendblk
	{$$ = NULL;}
	;

matchstmt: Tmatch exprln optendlns Tbor matches Tendblk
	{$$ = mkmatchstmt($1->loc, $2, $5.nl, $5.nn);}
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

match   : expr Tcolon blkbody Tendln {$$ = mkmatch($1->loc, $1, $3);}
	;

block   : blkbody Tendblk
	;

blkbody : decl {
		size_t i;
		Node *n, *d, *u;

		$$ = mkblock($1.loc, mkstab(0));
		for (i = 0; i < $1.nn; i++) {
			d = $1.nl[i];
			putdcl($$->block.scope, d);
			if (!d->decl.init) {
				n = mkexpr(d->loc, Ovar, d->decl.name, NULL);
				u = mkexpr(n->loc, Oundef, n, NULL);
				n->expr.did = d->decl.did;
				lappend(&$$->block.stmts, &$$->block.nstmts, u);
			}
			lappend(&$$->block.stmts, &$$->block.nstmts, d);
		}
	}
	| stmt {
		$$ = mkblock(curloc, mkstab(0));
		if ($1)
			lappend(&$$->block.stmts, &$$->block.nstmts, $1);
	}
	| tydef {
		$$ = mkblock(curloc, mkstab(0));
		puttype($$->block.scope, mkname($1.loc, $1.name), $1.type);
		installucons($$->block.scope, $1.type);
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
	| blkbody Tendln tydef {
		puttype($$->block.scope, mkname($3.loc, $3.name), $3.type);
		installucons($$->block.scope, $3.type);
	}
	;

label   : Tcolon Tident {
		char buf[512];
		genlblstr(buf, sizeof buf, $2->id);
		$$ = mklbl($2->loc, buf);
		$$->expr.args[0]->lit.lblname = strdup($2->id);
		lappend(&lbls, &nlbls, $$);
	}
	;

%%

static void setupinit(Node *n)
{
	char name[1024];
	char *p;

	bprintf(name, sizeof name, "%s$__init__", file->file.files[0]);
	p = name;
	while (*p) {
		if (!isalnum(*p) && *p != '_')
			*p = '$';
		p++;
	}
	n->decl.isinit = 1;
	n->decl.vis = Vishidden;
	n->decl.name->name.name = strdup(name);
}

static void addtrait(Type *t, char *str)
{
	size_t i;

	for (i = 0; i < ntraittab; i++) {
		if (!strcmp(namestr(traittab[i]->name), str)) {
			settrait(t, traittab[i]);
			return;
		}
	}
	lfatal(t->loc, "Constraint %s does not exist", str);
}

static Node *mkpseudodecl(Srcloc l, Type *t)
{
	static int nextpseudoid;
	char buf[128];

	bprintf(buf, 128, ".pdecl%d", nextpseudoid++);
	return mkdecl(l, mkname(l, buf), t);
}

static void setattrs(Node *dcl, char **attrs, size_t nattrs)
{
	size_t i;

	for (i = 0; i < nattrs; i++) {
		if (!strcmp(attrs[i], "extern"))
			dcl->decl.isextern = 1;
		else if (!strcmp(attrs[i], "$noret"))
			dcl->decl.isnoret = 1;
		else if (!strcmp(attrs[i], "pkglocal"))
			dcl->decl.ispkglocal = 1;
	}
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
			b->udecls[i]->utype = b;
			b->udecls[i]->id = i;
			putucon(st, b->udecls[i]);
		}
		break;
	default:
		break;
	}
}


static Op binop(int tt)
{
	Op o;

	o = Obad;
	switch (tt) {
	case Tplus:	o = Oadd;	break;
	case Tminus:	o = Osub;	break;
	case Tmul:	o = Omul;	break;
	case Tdiv:	o = Odiv;	break;
	case Tmod:	o = Omod;	break;
	case Tasn:	o = Oasn;	break;
	case Taddeq:	o = Oaddeq;	break;
	case Tsubeq:	o = Osubeq;	break;
	case Tmuleq:	o = Omuleq;	break;
	case Tdiveq:	o = Odiveq;	break;
	case Tmodeq:	o = Omodeq;	break;
	case Tboreq:	o = Oboreq;	break;
	case Tbxoreq:	o = Obxoreq;	break;
	case Tbandeq:	o = Obandeq;	break;
	case Tbsleq:	o = Obsleq;	break;
	case Tbsreq:	o = Obsreq;	break;
	case Tbor:	o = Obor;	break;
	case Tbxor:	o = Obxor;	break;
	case Tband:	o = Oband;	break;
	case Tbsl:	o = Obsl;	break;
	case Tbsr:	o = Obsr;	break;
	case Teq:	o = Oeq;	break;
	case Tgt:	o = Ogt;	break;
	case Tlt:	o = Olt;	break;
	case Tge:	o = Oge;	break;
	case Tle:	o = Ole;	break;
	case Tne:	o = One;	break;
	case Tlor:	o = Olor;	break;
	case Tland:	o = Oland;	break;
	default:
		die("Unimplemented binop\n");
		break;
	}
	return o;
}

