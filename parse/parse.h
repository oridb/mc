#define Abiversion 21

typedef struct Srcloc Srcloc;
typedef struct Tysubst Tysubst;
typedef struct Traitspec Traitspec;

typedef struct Tok Tok;
typedef struct File File;
typedef struct Node Node;
typedef struct Ucon Ucon;
typedef struct Stab Stab;
typedef struct Tyenv Tyenv;

typedef struct Type Type;
typedef struct Trait Trait;

typedef enum {
	OTmisc,
	OTpre,
	OTpost,
	OTbin,
	OTzarg,
} Optype;

typedef enum {
#define O(op, pure, type, pretty) op,
#include "ops.def"
	Numops,
#undef O
} Op;

typedef enum {
#define N(nt) nt,
#include "nodes.def"
#undef N
} Ntype;

typedef enum {
#define L(lt) lt,
#include "lits.def"
#undef L
} Littype;

typedef enum {
#define Ty(t, n, stk) t,
#include "types.def"
#undef Ty
	Ntypes
} Ty;

typedef enum {
#define Tc(c, n) c,
#include "trait.def"
#undef Tc
	Ntraits
} Tc;

#define Zloc ((Srcloc){-1, 0})

struct Srcloc {
	int line;
	int file;
};

typedef enum {
	Visintern,
	Visexport,
	Vishidden,
	Visbuiltin,
} Vis;

struct Tok {
	int type;
	Srcloc loc;
	char *id;

	/* values parsed out */
	vlong intval;
	Ty inttype; /* for explicitly specified suffixes */
	double fltval;
	uint32_t chrval;
	Str strval;
};

struct Tysubst {
	Htab **subst;
	size_t nsubst;
};

typedef enum {
	Xcnt,
	Xbrk,
	Xret,
	Nexits
} Exit;

struct Stab {
	Stab *super;
	char *name;
	char isfunc;

	/* Contents of stab.
	 * types and values are in separate namespaces. */
	Htab *dcl;
	Htab *env;	/* the syms we close over, if we're a function */
	Htab *ty;	/* types */
	Htab *tr;	/* traits */
	Htab *uc;	/* union constructors */
	Htab *lbl;	/* labels */
	Htab *impl;	/* trait implementations: really a set of implemented traits. */

	/* See mi/flatten.c for the following. */
	Node **autotmp; /* temporaries for 'auto' expressions */
	size_t nautotmp;

	Node *exit[Nexits];
	size_t ndisposed[Nexits];
};

struct Tyenv {
	Tyenv *super;
	Htab *tab;
};

struct Traitspec {
	Node **trait;
	size_t ntrait;
	Type *param;
	Type *aux;
};

struct Type {
	Ty type;
	uint32_t tid;
	Srcloc loc;
	Vis vis;


	Traitspec **spec;
	size_t nspec;
	Type *seqaux;

	Type **gparam;		/* Tygeneric: type parameters that match the type args */
	size_t ngparam;		/* Tygeneric: count of type parameters */
	Type **arg;		/* Tyname: type arguments instantiated */
	size_t narg;		/* Tyname: count of type arguments */
	Type **inst;		/* Tyname: instances created */
	size_t ninst;		/* Tyname: count of instances created */

	Tyenv *env;		/* the environment for bound types, may be null */
	Type **sub;		/* sub-types; shared by all composite types */
	size_t nsub;		/* For compound types */
	size_t nmemb;		/* for aggregate types (struct, union) */
	Bitset *trneed;		/* traits needed by this Tyvar/Typaram */
	union {
		Node *name;	/* Tyname: unresolved name. Tyalias: alias name */
		Node *asize;	/* array size */
		char *pname;	/* Typaram: name of type parameter */
		Node **sdecls;	/* Tystruct: decls in struct */
		Ucon **udecls;	/* Tyunion: decls in union */
	};

	char hasparams;		/* cache for whether this type has params */
	char isenum;		/* Tyunion: see isenum(), it is lazily set there */
	char ishidden;		/* Tyname: whether this is hidden or not */
	char ispkglocal;	/* Tyname: whether this is package local or not */
	char isimport;		/* Tyname: whether tyis type was imported. */
	char isreflect;		/* Tyname: whether this type has reflection info */
	char isemitted;		/* Tyname: whether this type has been emitted */
	char resolved;		/* Have we resolved the subtypes? Prevents infinite recursion. */
	char fixed;		/* Have we fixed the subtypes? Prevents infinite recursion. */
	char tagged;		/* Have we tagged the type for export? */

};

struct Ucon {
	Srcloc loc;
	size_t id;	/* tag id */
	int synth;	/* is it generated? */
	Node *name;	/* ucon name */
	Type *utype;	/* type of the union this is an element of */
	Type *etype;	/* type for the element */
};

struct Trait {
	int uid; /* unique id */
	Srcloc loc;
	Vis vis;

	Node *name;	/* the name of the trait */
	Type *param;	/* the type parameter */
	Type **aux;	/* auxiliary parameters */
	size_t naux;
	Node **proto;	/* type must implement these prototypes */
	size_t nproto;
	Tyenv *env;

	char isproto;	/* is it a prototype (for exporting purposes) */
	char ishidden;	/* should user code be able to use this? */
	char isimport;	/* have we defined it locally? */
};

struct File {
	size_t nfiles;	/* file names for location mapping */
	char **files;
	Node **uses;	/* use files that we loaded */
	size_t nuses;
	char **libdeps;	/* library dependencies */
	size_t nlibdeps;
	char **extlibs;	/* non-myrddin libraries */
	size_t nextlibs;
	Node **stmts;	/* all top level statements */
	size_t nstmts;
	Node **init;	/* all __init__ function names of our deps. NB, this
			   is a Nname, not an Ndecl */
	size_t ninit;
	Node **fini;	/* all __fini__ function names of our deps. NB, this
			   is a Nname, not an Ndecl */
	size_t nfini;
	Node **impl;	/* impls defined in this file, across all namespaces */
	size_t nimpl;
	Node *localinit;/* and the local one, if any */
	Node *localfini;/* and the local one, if any */
	Stab *globls;	/* global symtab */
	Stab *builtins;	/* global symtab */
	Htab *ns;	/* namespaces */
};

struct Node {
	Srcloc loc;
	Ntype type;
	int nid;
	char inferred;
	union {
		struct {
			Op op;
			Type *type;
			Type *param;	/* for specialized traits, the primary param */
			int isconst;
			size_t did;	/* for Ovar, we want a mapping to the decl id */
			size_t nargs;
			Node *idx;	/* used when this is in an indexed initializer */
			Node **args;
		} expr;

		struct {
			char *ns;
			char *name;
		} name;

		struct {
			int islocal;
			char *name;
		} use;

		struct {
			Littype littype;
			Type *type;
			size_t nelt;
			union {
				uvlong intval;
				double fltval;
				uint32_t chrval;
				Str strval;
				struct {
					char *lblval;
					char *lblname;
				};
				int boolval;
				Node *fnval;
			};
		} lit;

		struct {
			Node *init;
			Node *cond;
			Node *step;
			Node *body;
			Stab *scope;
		} loopstmt;

		struct {
			Node *elt;
			Node *seq;
			Node *body;
		} iterstmt;

		struct {
			Node *cond;
			Node *iftrue;
			Node *iffalse;
		} ifstmt;

		struct {
			Node *val;
			size_t nmatches;
			Node **matches;
		} matchstmt;

		struct {
			Node *pat;
			Node *block;
		} match;

		struct {
			Stab *scope;
			size_t nstmts;
			Node **stmts;
		} block;

		struct {
			size_t did;
			Node *name;
			Type *type;
			Node *init;
			Tyenv *env;	/* bound types */

			/*
			 If we have a link to a trait, we should only look it up
			 when specializing, but we should not create a new decl
			 node for it. That will be done when specializing the
			 impl.
			*/
			Trait *trait;
			Htab *impls;
			Node **gimpl;	/* generic impls of this trait */
			size_t ngimpl;
			Node **gtype;	/* generic impls of this trait */
			size_t ngtype;

			char vis;
			/* flags */
			char isglobl;
			char isconst;
			char isgeneric;
			char isextern;
			char ispkglocal;
			char ishidden;
			char isimport;
			char isnoret;
			char isexportval;
			char isinit;
			char isfini;
		} decl;

		struct {
			Tyenv *env;
			Stab *scope;
			Type *type;
			size_t nargs;
			Node **args;
			Node *body;
		} func;

		struct {
			Node *traitname;
			Trait *trait;
			Type *type;
			Type **aux;
			Tyenv *env;
			size_t naux;
			Node **decls;
			size_t ndecls;
			Vis vis;
			char isproto;
			char isextern;
		} impl;
	};
};

/* globals */
extern File file;	/* the current file we're compiling */
extern Srcloc curloc;
extern char *filename;
extern Tok *curtok;	/* the last token we tokenized */
extern Type **tytab;	/* type -> type map used by inference. size maintained by type creation code */
extern Type **types;
extern size_t ntypes;
extern Trait **traittab;/* int -> trait map */
extern size_t ntraittab;
extern Node **impltab;	/* int -> impl map */
extern size_t nimpltab;
extern Node **decls;	/* decl id -> decl map */
extern size_t nnodes;
extern Node **nodes;	/* node id -> node map */
extern size_t ndecls;
extern Node **exportimpls;
extern size_t nexportimpls;
extern int allowhidden;

/* property tables */
extern int opispure[];
extern char *opstr[];
extern char *oppretty[];
extern int opclass[];
extern char *nodestr[];
extern char *litstr[];
extern char *tidstr[];

/* useful key types */
int liteq(Node *a, Node *b);
int litvaleq(Node *a, Node *b);
ulong tyhash(void *t);
int tyeq(void *a, void *b);
int tystricteq(void *a, void *b);
int tymatchrank(Type *pat, Type *to);
ulong namehash(void *t);
int nameeq(void *a, void *b);
ulong nsnamehash(void *t);
int nsnameeq(void *a, void *b);

/* parsing etc */
void tokinit(char *file);
int yylex(void);
int yyparse(void);

/* locations */
char *fname(Srcloc l);
int lnum(Srcloc l);
void fatal(Node *n, char *fmt, ...) FATAL;
void lfatal(Srcloc l, char *fmt, ...) FATAL;
void lfatalv(Srcloc l, char *fmt, va_list ap) FATAL;

/* stab creation */
Stab *mkstab(int isfunc);

void putns(Stab *scope);
void puttype(Stab *st, Node *n, Type *ty);
void puttrait(Stab *st, Node *n, Trait *trait);
void putimpl(Stab *st, Node *impl);
void updatetype(Stab *st, Node *n, Type *t);
void putdcl(Stab *st, Node *dcl);
void forcedcl(Stab *st, Node *dcl);
void putucon(Stab *st, Ucon *uc);
void putlbl(Stab *st, char *name, Node *lbl);

Stab *getns(char *n);
Node *getdcl(Stab *st, Node *n);
Node *getclosed(Stab *st, Node *n);
Node **getclosure(Stab *st, size_t *n);
Type *gettype_l(Stab *st, Node *n);
Type *gettype(Stab *st, Node *n);
Node *getimpl(Stab *st, Node *impl);
Trait *gettrait(Stab *st, Node *n);
Ucon *getucon(Stab *st, Node *n);
Node *getlbl(Stab *st, Srcloc loc, char *name);

Stab *curstab(void);
void pushstab(Stab *st);
void popstab(void);

void bindtype(Tyenv *env, Type *t);
Type *boundtype(Type *t);
Tyenv *mkenv(void);
Tyenv *curenv(void);
void pushenv(Tyenv *e);
void popenv(Tyenv *e);

/* type creation */
void tyinit(Stab *st); /* sets up built in types */

Type *mktype(Srcloc l, Ty ty);
Type *tydup(Type *t); /* shallow duplicate; all subtypes/members/... kept */
Type *tydedup(Type *t);
Type *mktyvar(Srcloc l);
Type *mktyparam(Srcloc l, char *name);
Type *mktygeneric(Srcloc l, Node *name, Type **params, size_t nparams, Type *base);
Type *mktyname(Srcloc l, Node *name, Type *base);
Type *mktyunres(Srcloc l, Node *name, Type **params, size_t nparams);
Type *mktyarray(Srcloc l, Type *base, Node *sz);
Type *mktyslice(Srcloc l, Type *base);
Type *mktyptr(Srcloc l, Type *base);
Type *mktytuple(Srcloc l, Type **sub, size_t nsub);
Type *mktyfunc(Srcloc l, Node **args, size_t nargs, Type *ret);
Type *mktystruct(Srcloc l, Node **decls, size_t ndecls);
Type *mktyunion(Srcloc l, Ucon **decls, size_t ndecls);
Trait *mktrait(Srcloc l, Node *name,
	Type *param,
	Type **aux, size_t naux,
	Node **proto, size_t nproto,
	int isproto);
Ucon *finducon(Type *t, Node *name);
int isstacktype(Type *t);
int isenum(Type *t);
int istysigned(Type *t);
int istyint(Type *t);
int istyunsigned(Type *t);
int istyfloat(Type *t);
int istyprimitive(Type *t);
int hasparams(Type *t);

/* type manipulation */
Type *tybase(Type *t);
char *tyfmt(char *buf, size_t len, Type *t);
char *tystr(Type *t);
size_t tyidfmt(char *buf, size_t len, Type *t);

int traiteq(Type *t, Trait **traits, size_t len);
int traitfmt(char *buf, size_t len, Type *t);
char *traitstr(Type *t);

/* node creation */
void initfile(File *f, char *path);
Node *mknode(Srcloc l, Ntype nt);
Node *mkuse(Srcloc l, char *use, int islocal);
Node *mksliceexpr(Srcloc l, Node *sl, Node *base, Node *off);
Node *mkexprl(Srcloc l, Op op, Node **args, size_t nargs);
Node *mkexpr(Srcloc l, int op, ...); /* NULL terminated */
Node *mkcall(Srcloc l, Node *fn, Node **args, size_t nargs);
Node *mkifstmt(Srcloc l, Node *cond, Node *iftrue, Node *iffalse);
Node *mkloopstmt(Srcloc l, Node *init, Node *cond, Node *incr, Node *body);
Node *mkiterstmt(Srcloc l, Node *elt, Node *seq, Node *body);
Node *mkmatchstmt(Srcloc l, Node *val, Node **matches, size_t nmatches);
Node *mkmatch(Srcloc l, Node *pat, Node *body);
Node *mkblock(Srcloc l, Stab *scope);
Node *mkimplstmt(Srcloc loc, Node *name, Type *t, Type **aux, size_t naux, Node **decls, size_t ndecls);
Node *mkintlit(Srcloc l, uvlong val);
Node *mkboollit(Srcloc l, int val);
Node *mkidxinit(Srcloc l, Node *idx, Node *init);

Node *mkvoid(Srcloc loc);
Node *mkbool(Srcloc l, int val);
Node *mkint(Srcloc l, uvlong val);
Node *mkchar(Srcloc l, uint32_t val);
Node *mkstr(Srcloc l, Str str);
Node *mkfloat(Srcloc l, double flt);
Node *mkfunc(Srcloc l, Node **args, size_t nargs, Type *ret, Node *body);
Node *mkname(Srcloc l, char *name);
Node *mknsname(Srcloc l, char *ns, char *name);
Node *mkdecl(Srcloc l, Node *name, Type *ty);
Node *gentemp(Srcloc loc, Type *ty, Node **dcl);
Node *mklbl(Srcloc l, char *lbl);
Node *genlbl(Srcloc loc);
char *genlblstr(char *buf, size_t sz, char *suffix);
Node *mkslice(Srcloc l, Node *base, Node *off);
Ucon *mkucon(Srcloc l, Node *name, Type *ut, Type *uet);

/* node util functions */
uvlong arraysz(Node *sz);
char *namestr(Node *name);
char *lblstr(Node *n);
char *declname(Node *n);
Type *decltype(Node * n);
Type *exprtype(Node *n);
Type *nodetype(Node *n);
void addstmt(Node *stmt);
void setns(Node *n, char *ns);
void updatens(Stab *st, char *ns);
ulong varhash(void *dcl);
int vareq(void *a, void *b);
Op exprop(Node *n);

/* specialize generics */
Tysubst *mksubst(void);
void substfree(Tysubst *subst);
void substput(Tysubst *subst, Type *from, Type *to);
Type *substget(Tysubst *subst, Type *from);
Node *specializedcl(Node *n, Type *param, Type *to, Node **name);
Type *tyspecialize(Type *t, Tysubst *tymap, Htab *delayed);
Node *genericname(Node *n, Type *param, Type *t);
void genautocall(Node **, size_t, Node *, char *);

/* usefiles */
void loaduses(void);
int loaduse(char *path, FILE *f, Stab *into, Vis vis);
void readuse(Node *use, Stab *into, Vis vis);
void writeuse(FILE *fd);
void tagexports(int hidelocal);
void tagreflect(Type *t);
void addextlibs(char **libs, size_t nlibs);

/* expression folding */
Node *fold(Node *n, int foldvar);
int getintlit(Node *lit, vlong *val);

/* typechecking/inference */
void infer(void);
Type *tysearch(Type *t);

/* debug */
void dump(FILE *);
void dumpn(Node *, FILE *);
void dumpsym(Node *, FILE *);
void dumpstab(Stab *, FILE *);

/* option parsing */
void optinit(Optctx *ctx, char *optstr, char **optargs, size_t noptargs);
int optnext(Optctx *c);
int optdone(Optctx *c);

/* Options to control the compilation */
extern char debugopt[128];
extern int asmonly;
extern char *outfile;
extern char *objdir;
extern char **incpaths;
extern size_t nincpaths;
extern char *localincpath;
extern size_t (*sizefn)(Node *n);

void yyerror(const char *s);
