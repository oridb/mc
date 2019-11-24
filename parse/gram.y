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


#define LBLSTKSZ 64
static Node **lbls[LBLSTKSZ];
static size_t nlbls[LBLSTKSZ];
Stab *curscope;

/* the first time we see a label, we increment to 0 */
static int lbldepth = -1;

void yyerror(const char *s);
int yylex(void);

static Op binop(int);
static Node *mkpseudodecl(Srcloc, Type *);
static void installucons(Stab *, Type *);
static void setattrs(Node *, char **attrs, size_t);
static void setwith(Type *, Traitspec **, size_t);
static void mergeenv(Node *, Node *);
static void mangleautocall(Node *, char *);
static void addinit(Node *, Node *);
static void setuname(Type *);

%}

%token<tok> Tplus	/* + */
%token<tok> Tminus	/* - */
%token<tok> Tmul	/* * */
%token<tok> Tdiv	/* / */
%token<tok> Tinc	/* ++ */
%token<tok> Tdec	/* -- */
%token<tok> Tmod	/* % */
%token<tok> Tasn	/* = */
%token<tok> Taddeq	/* += */
%token<tok> Tsubeq	/* -= */
%token<tok> Tmuleq	/* *= */
%token<tok> Tdiveq	/* /= */
%token<tok> Tmodeq	/* %= */
%token<tok> Tboreq	/* |= */
%token<tok> Tbxoreq	/* ^= */
%token<tok> Tbandeq	/* &= */
%token<tok> Tbsleq	/* <<= */
%token<tok> Tbsreq	/* >>= */

%token<tok> Tbor	/* | */
%token<tok> Tbxor	/* ^ */
%token<tok> Tband	/* & */
%token<tok> Tbsl	/* << */
%token<tok> Tbsr	/* >> */
%token<tok> Tbnot	/* ~ */

%token<tok> Teq		/* == */
%token<tok> Tgt		/* > */
%token<tok> Tlt		/* < */
%token<tok> Tge		/* >= */
%token<tok> Tle		/* <= */
%token<tok> Tne		/* != */

%token<tok> Tlor	/* || */
%token<tok> Tland	/* && */
%token<tok> Tlnot	/* ! */

%token<tok> Tobrace	/* { */
%token<tok> Tcbrace	/* } */
%token<tok> Toparen	/* ( */
%token<tok> Tcparen	/* ) */
%token<tok> Tosqbrac	/* [ */
%token<tok> Tcsqbrac	/* ] */
%token<tok> Ttick	/* ` */
%token<tok> Tderef	/* # */
%token<tok> Tqmark	/* ? */

%token<tok> Ttype	/* type */
%token<tok> Tfor	/* for */
%token<tok> Twhile	/* while */
%token<tok> Tif		/* if */
%token<tok> Telse	/* else */
%token<tok> Telif	/* else */
%token<tok> Tmatch	/* match */
%token<tok> Tgoto	/* goto */
%token<tok> Tbreak	/* break */
%token<tok> Tcontinue	/* continue */
%token<tok> Tauto	/* auto */

%token<tok> Tintlit
%token<tok> Tstrlit
%token<tok> Tfloatlit
%token<tok> Tchrlit
%token<tok> Tboollit
%token<tok> Tvoidlit

%token<tok> Ttrait	/* trait */
%token<tok> Timpl	/* trait */
%token<tok> Tstruct	/* struct */
%token<tok> Tunion	/* union */
%token<tok> Ttyparam	/* @typename */

%token<tok> Tconst	/* const */
%token<tok> Tvar	/* var */
%token<tok> Tgeneric	/* generic */

%token<tok> Tgap	/* _ */
%token<tok> Tellipsis	/* ... */
%token<tok> Tendln	/* ; or \n */
%token<tok> Tendblk	/* ;; */
%token<tok> Tcolon	/* : */
%token<tok> Twith	/* :: */
%token<tok> Tdot	/* . */
%token<tok> Tcomma	/* , */
%token<tok> Tret	/* -> */
%token<tok> Tuse	/* use */
%token<tok> Tpkg	/* pkg */
%token<tok> Tattr	/* $attr */
%token<tok> Tsizeof	/* sizeof */

%token<tok> Tident

%start file

%type <ty> type structdef uniondef tupledef compoundtype functype funcsig
%type <ty> generictype
%type <tylist> typelist typarams optauxtypes
%type <traitspecs> traitspec traits
%type <traitspec> traitvar
%type <nodelist> traitlist

%type <tok> asnop cmpop addop mulop shiftop optident obrace

%type <tydef> tydef pkgtydef typeid
%type <trait> traitdef

%type<node> exprln retexpr goto continue break expr atomicexpr
%type<node> littok literal lorexpr landexpr borexpr strlit bandexpr
%type<node> cmpexpr addexpr mulexpr shiftexpr prefixexpr ternexpr
%type<node> postfixexpr funclit seqlit tuplit name block stmt label
%type<node> use fnparam declbody declcore typedeclcore structent
%type<node> arrayelt structelt tuphead ifstmt forstmt whilestmt
%type<node> matchstmt elifs optexprln loopcond optexpr match

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

/*
We want to bind more tightly to `Foo unaryexpr
than to binary operators around it. Eg:

	`Foo +bar

should be interpreted as

	(`Foo (+bar))

and not

	(`Foo) + (bar)

The default action of shifting does the right thing,
but warnings suck.
*/
%left Ttick
%left Tplus Tminus Tband

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
	struct {
		Traitspec **spec;
		size_t nspec;
	} traitspecs;
	Traitspec *traitspec;
	Trait *trait;
	Node *node;
	Tok  *tok;
	Type *ty;
	Ucon *ucon;
}

%%

file	: toplev
	| file Tendln toplev
	;

toplev	: package
	| use {lappend(&file.uses, &file.nuses, $1);}
	| implstmt {
		lappend(&file.stmts, &file.nstmts, $1);
	}
	| traitdef {
		size_t i;
		puttrait(file.globls, $1->name, $1);
		for (i = 0; i < $1->nproto; i++)
			putdcl(file.globls, $1->proto[i]);
	}
	| tydef {
		puttype(file.globls, mkname($1.loc, $1.name), $1.type);
		installucons(file.globls, $1.type);
	}
	| decl {
		size_t i;
		Node *n, *d;

		for (i = 0; i < $1.nn; i++) {
			d = $1.nl[i];
			/* putdcl can merge, so we need to getdcl after */
			mangleautocall(d, declname(d));
			putdcl(file.globls, d);
			n = getdcl(file.globls, d->decl.name);
			lappend(&file.stmts, &file.nstmts, n);
			d->decl.isglobl = 1;
			if (d->decl.isinit)
				file.localinit = d;
			if (d->decl.isfini)
				file.localfini = d;
		}
	}
	| /* empty */
	;

decl	: attrs Tvar decllist traitspec {
		size_t i;

		for (i = 0; i < $3.nn; i++)
			setattrs($3.nl[i], $1.str, $1.nstr);
		$$ = $3;
	}
	| attrs Tconst decllist traitspec {
		size_t i;
		for (i = 0; i < $3.nn; i++) {
			setattrs($3.nl[i], $1.str, $1.nstr);
			$3.nl[i]->decl.isconst = 1;
		}
		$$ = $3;
	}
	| attrs Tgeneric decllist traitspec {
		size_t i;

		for (i = 0; i < $3.nn; i++) {
			setattrs($3.nl[i], $1.str, $1.nstr);
			setwith($3.nl[i]->decl.type, $4.spec, $4.nspec);
			$3.nl[i]->decl.isconst = 1;
			$3.nl[i]->decl.isgeneric = 1;
		}
		$$ = $3;
	}
        ;

attrs	: /* empty */ {$$.nstr = 0; $$.str = NULL;}
	| Tattr attrs {
		$$ = $2;
		lappend(&$$.str, &$$.nstr, strdup($1->id));
	}
	;

traitspec
	: Twith traits	{$$ = $2;}
	| /* nothing */ {$$.nspec = 0;}
	;

traits	: traitvar {
       		$$.spec = NULL;
       		$$.nspec = 0;
		lappend(&$$.spec, &$$.nspec, $1);
	}
	| traits listsep traitvar {
		$$ = $1;
		lappend(&$$.spec, &$$.nspec, $3);
	}
	;

traitvar
	: traitlist generictype {
		$$ = calloc(sizeof(Traitspec), 1);
		$$->trait = $1.nl;
		$$->ntrait = $1.nn;
		$$->param = $2;
		$$->aux = NULL;
	}
	| traitlist generictype Tret type {
		$$ = calloc(sizeof(Traitspec), 1);
		$$->trait = $1.nl;
		$$->ntrait = $1.nn;
		$$->param = $2;
		$$->aux = $4;
	}
	;

traitlist
	: name {
		$$.nl = 0;
		$$.nn = 0;
		lappend(&$$.nl, &$$.nn, $1);
	}
	| traitlist listsep name {
		lappend(&$$.nl, &$$.nn, $3);
	}
	;

decllist: declbody {
		$$.loc = $1->loc; $$.nl = NULL; $$.nn = 0;
		lappend(&$$.nl, &$$.nn, $1);
	}
	| declbody listsep decllist {
		linsert(&$3.nl, &$3.nn, 0, $1);
		$$=$3;
	}
	;

use	: Tuse Tident {$$ = mkuse($1->loc, $2->id, 0);}
	| Tuse Tstrlit {$$ = mkuse($1->loc, $2->strval.buf, 1);}
	;

optident: Tident      {$$ = $1;}
	| /* empty */ {$$ = NULL;}
	;

package : Tpkg optident Tasn pkgbody Tendblk {
		if (file.globls->name)
			lfatal($1->loc, "Package already declared\n");
		if ($2) {
			updatens(file.globls, $2->id);
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
			putdcl(file.globls, $1.nl[i]);
			if ($1.nl[i]->decl.init)
				lappend(&file.stmts, &file.nstmts, $1.nl[i]);
		}
	}
	| pkgtydef {
		/* the type may only be null in a package context, so we
		can set the type when merging in this case.

		FIXME: clean up the fucking special cases. */
		if ($1.type)
			$1.type->vis = Visexport;
		puttype(file.globls, mkname($1.loc, $1.name), $1.type);
		installucons(file.globls, $1.type);
	}
	| traitdef {
		size_t i;
		$1->vis = Visexport;
		puttrait(file.globls, $1->name, $1);
		for (i = 0; i < $1->nproto; i++)
			putdcl(file.globls, $1->proto[i]);
	}
	| implstmt {
		$1->impl.vis = Visexport;
		lappend(&file.stmts, &file.nstmts, $1);
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

declbody: declcore
	| declcore Tasn expr {
		$$ = $1;
		$1->decl.init = $3;
		mergeenv($1, $3);
	}
	;

declcore: name {$$ = mkdecl($1->loc, $1, mktyvar($1->loc));}
	| typedeclcore {$$ = $1;}
	;

typedeclcore
	: name Tcolon type {$$ = mkdecl($1->loc, $1, $3);}
	;

name	: Tident {$$ = mkname($1->loc, $1->id);}
	| Tident Tdot Tident {
		$$ = mknsname($3->loc, $1->id, $3->id);
	}
	;

implstmt
	: Timpl name type optauxtypes traitspec {
		size_t i;

		$$ = mkimplstmt($1->loc, $2, $3, $4.types, $4.ntypes, NULL, 0);
		$$->impl.isproto = 1;
		setwith($3, $5.spec, $5.nspec);
		for (i = 0; i < $4.ntypes; i++)
			setwith($4.types[i], $5.spec, $5.nspec);
	}
	| Timpl name type optauxtypes traitspec Tasn Tendln implbody Tendblk {
		size_t i;

		$$ = mkimplstmt($1->loc, $2, $3, $4.types, $4.ntypes, $8.nl, $8.nn);
		setwith($3, $5.spec, $5.nspec);
		for (i = 0; i < $4.ntypes; i++)
			setwith($4.types[i], $5.spec, $5.nspec);
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
		mergeenv(d, $4);
		lappend(&$$.nl, &$$.nn, d);
	}
	;

traitdef
	: Ttrait Tident generictype optauxtypes traitspec { /* trait prototype */
		size_t i;
		$$ = mktrait($1->loc,
			mkname($2->loc, $2->id), $3,
			$4.types, $4.ntypes,
			NULL, 0,
			1);
		setwith($3, $5.spec, $5.nspec);
		for (i = 0; i < $4.ntypes; i++)
			setwith($4.types[i], $5.spec, $5.nspec);
	}
	| Ttrait Tident generictype optauxtypes traitspec Tasn traitbody Tendblk /* trait definition */ {
		size_t i;
		$$ = mktrait($1->loc,
			mkname($2->loc, $2->id), $3,
			$4.types, $4.ntypes,
			$7.nl, $7.nn,
			0);
		for (i = 0; i < $7.nn; i++) {
			$7.nl[i]->decl.trait = $$;
			$7.nl[i]->decl.impls = mkht(tyhash, tyeq);
			$7.nl[i]->decl.isgeneric = 1;
		}
		setwith($3, $5.spec, $5.nspec);
		for (i = 0; i < $4.ntypes; i++)
			setwith($4.types[i], $5.spec, $5.nspec);
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
		d->decl.isglobl = 1;
		d->decl.isgeneric = 1;
		d->decl.isconst = 1;
		lappend(&$$.nl, &$$.nn, d);
	}
	;


tydef	: Ttype typeid traitspec {$$ = $2;}
	| Ttype typeid traitspec Tasn type {
		$$ = $2;
		if ($$.nparams == 0)
			$$.type = mktyname($2.loc, mkname($2.loc, $2.name), $5);
		else
			$$.type = mktygeneric($2.loc, mkname($2.loc, $2.name), $2.params, $2.nparams, $5);
		setuname($$.type);
		setwith($$.type, $3.spec, $3.nspec);
	}
	;

typeid	: Tident {
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

typarams
	: generictype {
		$$.types = NULL; $$.ntypes = 0;
		lappend(&$$.types, &$$.ntypes, $1);
	}
	| typarams listsep generictype {
		lappend(&$$.types, &$$.ntypes, $3);
	}
	;

type	: structdef
	| tupledef
	| uniondef
	| compoundtype
	| generictype
	| Tellipsis {$$ = mktype($1->loc, Tyvalist);}
	;

generictype
	: Ttyparam {$$ = mktyparam($1->loc, $1->id);}
	;

compoundtype
	: functype   				{$$ = $1;}
	| type Tosqbrac Tcolon Tcsqbrac		{$$ = mktyslice($2->loc, $1);}
	| type Tosqbrac expr Tcsqbrac		{$$ = mktyarray($2->loc, $1, $3);}
	| type Tosqbrac Tellipsis Tcsqbrac 	{$$ = mktyarray($2->loc, $1, NULL);}
	| name Toparen typelist Tcparen 	{$$ = mktyunres($1->loc, $1, $3.types, $3.ntypes);}
	| type Tderef				{$$ = mktyptr($2->loc, $1);}
	| Tvoidlit				{$$ = mktyunres($1->loc, mkname($1->loc, $1->id), NULL, 0);}
	| name					{$$ = mktyunres($1->loc, $1, NULL, 0);}
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
	| argdefs listsep typedeclcore {lappend(&$$.nl, &$$.nn, $3);}
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
	| typelist listsep type
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

goto	: Tgoto Tident {
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

exprln	: expr Tendln
	;

expr	: ternexpr asnop expr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| ternexpr
	;

asnop	: Tasn
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

ternexpr
	: lorexpr
	| lorexpr Tqmark lorexpr Tcolon lorexpr
	{$$ = mkexpr($1->loc, Otern, $1, $3, $5, NULL);}
	;

lorexpr : lorexpr Tlor landexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| landexpr
	;

landexpr: landexpr Tland cmpexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| cmpexpr
	;

cmpexpr : cmpexpr cmpop borexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| borexpr
	;

cmpop	: Teq | Tgt | Tlt | Tge | Tle | Tne ;


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

addop	: Tplus | Tminus ;

mulexpr : mulexpr mulop shiftexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| shiftexpr
	;

mulop	: Tmul | Tdiv | Tmod
	;

shiftexpr
	: shiftexpr shiftop prefixexpr
	{$$ = mkexpr($1->loc, binop($2->type), $1, $3, NULL);}
	| prefixexpr
	;

shiftop : Tbsl | Tbsr;

prefixexpr
	: Tauto prefixexpr	{$$ = mkexpr($1->loc, Oauto, $2, NULL);}
	| Tinc prefixexpr	{$$ = mkexpr($1->loc, Opreinc, $2, NULL);}
	| Tdec prefixexpr	{$$ = mkexpr($1->loc, Opredec, $2, NULL);}
	| Tband prefixexpr	{$$ = mkexpr($1->loc, Oaddr, $2, NULL);}
	| Tlnot prefixexpr	{$$ = mkexpr($1->loc, Olnot, $2, NULL);}
	| Tbnot prefixexpr	{$$ = mkexpr($1->loc, Obnot, $2, NULL);}
	| Tminus prefixexpr	{$$ = mkexpr($1->loc, Oneg, $2, NULL);}
	| Tplus prefixexpr	{$$ = $2;} /* positive is a nop */
	| Ttick name prefixexpr	{$$ = mkexpr($1->loc, Oucon, $2, $3, NULL);}
	| Ttick name 		{$$ = mkexpr($1->loc, Oucon, $2, NULL);}
	| postfixexpr
	;

postfixexpr
	: postfixexpr Tdot Tident
	{$$ = mkexpr($1->loc, Omemb, $1, mkname($3->loc, $3->id), NULL);}
	| postfixexpr Tdot Tintlit
	{$$ = mkexpr($1->loc, Otupmemb, $1, mkint($3->loc, $3->intval), NULL);}
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

arglist : expr
	{$$.nl = NULL; $$.nn = 0; lappend(&$$.nl, &$$.nn, $1);}
	| arglist listsep expr
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
	| Toparen expr Tcolon type Tcparen {
		$$ = mkexpr($1->loc, Ocast, $2, NULL);
		$$->expr.type = $4;
	}
	| Tsizeof Toparen type Tcparen {
		$$ = mkexpr($1->loc, Osize, mkpseudodecl($1->loc, $3), NULL);
	}
	| Timpl Toparen name listsep type Tcparen {
		$$ = mkexpr($1->loc, Ovar, $3, NULL);
		$$->expr.param = $5;
	}
	;

tupbody : tuphead tuprest
	{$$ = $2;
	 linsert(&$$.nl, &$$.nn, 0, $1);}
	;

tuphead : expr listsep {$$ = $1;}
	;

tuprest : /*empty */
	{$$.nl = NULL; $$.nn = 0;}
	| expr {
		$$.nl = NULL; $$.nn = 0;
		lappend(&$$.nl, &$$.nn, $1);
	}
	| tuprest listsep expr {lappend(&$$.nl, &$$.nn, $3);}
	;

literal : funclit       {$$ = mkexpr($1->loc, Olit, $1, NULL);}
	| littok        {$$ = mkexpr($1->loc, Olit, $1, NULL);}
	| seqlit        {$$ = $1;}
	| tuplit        {$$ = $1;}
	;

tuplit	: Toparen tupbody Tcparen
	{$$ = mkexprl($1->loc, Otup, $2.nl, $2.nn);}

littok	: strlit	{$$ = $1;}
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

strlit : Tstrlit { $$ = mkstr($1->loc, $1->strval); }
	| strlit Tstrlit {
		Str merged;

		merged.len = $1->lit.strval.len + $2->strval.len;
		merged.buf = malloc(merged.len);
		memcpy(merged.buf, $1->lit.strval.buf, $1->lit.strval.len);
		memcpy(merged.buf + $1->lit.strval.len, $2->strval.buf, $2->strval.len);
		$$ = mkstr($1->loc, merged);
	}
	;

obrace	: Tobrace {
		assert(lbldepth < LBLSTKSZ);
		lbldepth++;
	}
	;

funclit : obrace params traitspec Tendln blkbody Tcbrace {
		size_t i;
		Node *fn, *lit;

		for (i = 0; i < $2.nn; i++)
			setwith($2.nl[i]->decl.type, $3.spec, $3.nspec);
		$$ = mkfunc($1->loc, $2.nl, $2.nn, mktyvar($4->loc), $5);
		fn = $$->lit.fnval;
		for (i = 0; i < nlbls[lbldepth]; i++) {
			lit = lbls[lbldepth][i]->expr.args[0];
			putlbl(fn->func.scope, lit->lit.lblname, lbls[lbldepth][i]);
		}
		lfree(&lbls[lbldepth], &nlbls[lbldepth]);
		assert(lbldepth >= 0);
		lbldepth--;
	}
	| obrace params Tret type traitspec Tendln blkbody Tcbrace {
		size_t i;
		Node *fn, *lit;

		setwith($4, $5.spec, $5.nspec);
		for (i = 0; i < $2.nn; i++)
			setwith($2.nl[i]->decl.type, $5.spec, $5.nspec);
		$$ = mkfunc($1->loc, $2.nl, $2.nn, $4, $7);
		fn = $$->lit.fnval;
		for (i = 0; i < nlbls[lbldepth]; i++) {
			lit = lbls[lbldepth][i]->expr.args[0];
			putlbl(fn->func.scope, lit->lit.lblname, lbls[lbldepth][i]);
		}
		lfree(&lbls[lbldepth], &nlbls[lbldepth]);
		assert(lbldepth >= 0);
		lbldepth--;
	}
	;

params	: fnparam {
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

seqlit	: Tosqbrac optendlns arrayelts optcomma Tcsqbrac
	{$$ = mkexprl($1->loc, Oarr, $3.nl, $3.nn);}
	| Tosqbrac optendlns structelts optcomma Tcsqbrac
	{$$ = mkexprl($1->loc, Ostruct, $3.nl, $3.nn);}
	| Tosqbrac optendlns optcomma Tcsqbrac /* [] is the empty array. */
	{$$ = mkexprl($1->loc, Oarr, NULL, 0);}
	;

arrayelts
	: arrayelt {
		$$.nl = NULL;
		$$.nn = 0;
		if ($1->expr.idx)
			lappend(&$$.nl, &$$.nn, $1);
		else
			lappend(&$$.nl, &$$.nn, mkidxinit($1->loc, mkintlit($1->loc, 0), $1));
	}
	| arrayelts listsep arrayelt {
		if ($3->expr.idx)
			lappend(&$$.nl, &$$.nn, $3);
		else
			lappend(&$$.nl, &$$.nn, mkidxinit($3->loc, mkintlit($3->loc, $$.nn), $3));
	}
	;

arrayelt: expr optendlns {$$ = $1;}
	| Tdot Tosqbrac expr Tcsqbrac Tasn expr optendlns {
		$$ = mkidxinit($1->loc, $3, $6);
	}
	;

structelts
	: structelt {
		$$.nl = NULL;
		$$.nn = 0;
		lappend(&$$.nl, &$$.nn, $1);
	}
	| structelts listsep structelt {
		lappend(&$$.nl, &$$.nn, $3);
	}
	;

structelt: Tdot Tident Tasn expr optendlns {
		$$ = $4;
		mkidxinit($2->loc, mkname($2->loc, $2->id), $4);
	}
	;

listsep	: Tcomma optendlns
	;

optcomma: Tcomma optendlns
	| /* empty */
	;

optendlns 
	: /* empty */
	| optendlns Tendln
	;

stmt	: goto
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

break	: Tbreak
	{$$ = mkexpr($1->loc, Obreak, NULL);}
	;

continue	: Tcontinue
	{$$ = mkexpr($1->loc, Ocontinue, NULL);}
	;

forstmt : Tfor optexprln loopcond optexprln block
	{$$ = mkloopstmt($1->loc, $2, $3, $4, $5);}
	| Tfor expr Tcolon exprln block
	{$$ = mkiterstmt($1->loc, $2, $4, $5);}
	| Tfor decl Tendln loopcond optexprln block {
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

ifstmt	: Tif exprln blkbody elifs
	{$$ = mkifstmt($1->loc, $2, $3, $4);}
	;

elifs	: Telif exprln blkbody elifs
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
		lappend(&$$.nl, &$$.nn, $1);
	}
	| matches Tbor match {
		lappend(&$$.nl, &$$.nn, $3);
	}
	;

match	: expr Tcolon blkbody Tendln {$$ = mkmatch($1->loc, $1, $3);}
	;

block	: blkbody Tendblk
	;

blkbody : decl {
		size_t i;

		$$ = mkblock($1.loc, mkstab(0));
		for (i = 0; i < $1.nn; i++) {
			putdcl($$->block.scope, $1.nl[i]);
			addinit($$, $1.nl[i]);
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
			addinit($$, $3.nl[i]);
		}
	}
	| blkbody Tendln tydef {
		puttype($$->block.scope, mkname($3.loc, $3.name), $3.type);
		installucons($$->block.scope, $3.type);
	}
	;

label	: Tcolon Tident {
		char buf[512];
		genlblstr(buf, sizeof buf, $2->id);
		$$ = mklbl($2->loc, buf);
		$$->expr.args[0]->lit.lblname = strdup($2->id);
		lappend(&lbls[lbldepth], &nlbls[lbldepth], $$);
	}
	;

%%

static void
addinit(Node *blk, Node *dcl)
{
	Node *n, *u;
	if (!dcl->decl.init) {
		n = mkexpr(dcl->loc, Ovar, dcl->decl.name, NULL);
		u = mkexpr(n->loc, Oundef, n, NULL);
		n->expr.did = dcl->decl.did;
		lappend(&blk->block.stmts, &blk->block.nstmts, u);
	}
	lappend(&blk->block.stmts, &blk->block.nstmts, dcl);
}

static void
mangleautocall(Node *n, char *fn)
{
	char name[1024];
	char *p, *s;

	if (strcmp(fn, "__init__") == 0)
		n->decl.isinit = 1;
	else if (strcmp(fn, "__fini__") == 0)
		n->decl.isfini = 1;
	else
		return;

	bprintf(name, sizeof name, "%s$%s", file.files[0], fn);
	if ((s = strrchr(name, '/')) != NULL)
		s++;
	else
		s = name;
	for(p = s; *p; p++)
		if (!isalnum(*p) && *p != '_')
			*p = '$';
	n->decl.vis = Vishidden;
	n->decl.name->name.name = strdup(s);
}

static void
setuname(Type *ty)
{
	size_t i;

	if (ty->sub[0]->type != Tyunion)
		return;
	for (i = 0; i < ty->sub[0]->nmemb; i++)
		ty->sub[0]->udecls[i]->utype = ty;
}

static Node *
mkpseudodecl(Srcloc l, Type *t)
{
	static int nextpseudoid;
	char buf[128];
	Node *d;

	bprintf(buf, 128, ".pdecl%d", nextpseudoid++);
	d = mkdecl(l, mkname(l, buf), t);
	d->decl.env = NULL;
	return d;
}

static void
setattrs(Node *dcl, char **attrs, size_t nattrs)
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

static void
setwith(Type *ty, Traitspec **ts, size_t nts)
{
	size_t i, j;

	if (!ty)
		return;
	for (i = 0; i < nts; i++) {
		switch (ty->type) {
		case Typaram:
			assert(ts[i]->param->type == Typaram);
			if (streq(ty->pname, ts[i]->param->pname))
				lappend(&ty->spec, &ty->nspec, ts[i]);

			break;
		case Tyname:
		case Tyunres:
			for (j = 0; j < ty->ngparam; j++)
				setwith(ty->gparam[j], ts, nts);
			for (j = 0; j < ty->narg; j++)
				setwith(ty->arg[j], ts, nts);
			break;
		case Tystruct:
			for (j = 0; j < ty->nmemb; j++)
				setwith(ty->sdecls[j]->decl.type, ts, nts);
			break;
		case Tyunion:
			for (j = 0; j < ty->nmemb; j++)
				setwith(ty->udecls[j]->etype, ts, nts);
			break;
		case Typtr:
		case Tyarray:
		case Tyslice:
		case Tyfunc:
		case Tytuple:
			for (j = 0; j < ty->nsub; j++)
				setwith(ty->sub[j], ts, nts);
			break;
		default:
			break;
		}
	}
}

static void
mergeenv(Node *dcl, Node *init)
{
	Node *f;

	if (init->type != Nexpr || exprop(init) != Olit)
		return;
	if (init->expr.args[0]->lit.littype != Lfunc)
		return;
	f = init->expr.args[0]->lit.fnval;
	if (!dcl->decl.env)
		dcl->decl.env = mkenv();
	bindtype(dcl->decl.env, f->func.type);
}


static void
installucons(Stab *st, Type *t)
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
			if (!b->udecls[i]->utype)
				b->udecls[i]->utype = b;
			b->udecls[i]->id = i;
			putucon(st, b->udecls[i]);
		}
		break;
	default:
		break;
	}
}


static Op
binop(int tt)
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
