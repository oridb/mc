#define Abiversion 11

typedef struct Srcloc Srcloc;
typedef struct Tysubst Tysubst;

typedef struct Tok Tok;
typedef struct Node Node;
typedef struct Ucon Ucon;
typedef struct Stab Stab;

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

typedef enum {
	Dclconst = 1 << 0,
	Dclextern = 1 << 1,
} Dclflags;


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

struct Stab {
	Stab *super;
	char *name;
	char isfunc;

	/* Contents of stab.
	 * types and values are in separate namespaces. */
	Htab *dcl;
	Htab *env;  /* the syms we close over, if we're a function */
	Htab *ty;   /* types */
	Htab *tr;   /* traits */
	Htab *uc;   /* union constructors */
	Htab *lbl;  /* labels */
	Htab *impl; /* trait implementations: really a set of implemented traits. */
};

struct Type {
	Ty type;
	int tid;
	Srcloc loc;
	Vis vis;

	Bitset *traits;	/* the type constraints matched on this type */
	Node **traitlist;  /* The names of the constraints on the type. Used to fill the bitset */
	size_t ntraitlist; /* The length of the constraint list above */

	Type **gparam;  /* Tygeneric: type parameters that match the type args */
	size_t ngparam; /* Tygeneric: count of type parameters */
	Type **arg;	 /* Tyname: type arguments instantiated */
	size_t narg;	/* Tyname: count of type arguments */
	Type **inst;	/* Tyname: instances created */
	size_t ninst;   /* Tyname: count of instances created */

	Type **sub;   /* sub-types; shared by all composite types */
	size_t nsub;  /* For compound types */
	size_t nmemb; /* for aggregate types (struct, union) */
	union {
		Node *name;	/* Tyname: unresolved name. Tyalias: alias name */
		Node *asize;   /* array size */
		char *pname;   /* Typaram: name of type parameter */
		Node **sdecls; /* Tystruct: decls in struct */
		Ucon **udecls; /* Tyunion: decls in union */
	};

	char hasparams;	 /* cache for whether this type has params */
	char issynth;	 /* Tyname: whether this is synthesized or not */
	char ishidden;   /* Tyname: whether this is hidden or not */
	char ispkglocal; /* Tyname: whether this is package local or not */
	char isimport;   /* Tyname: whether tyis type was imported. */
	char isreflect;  /* Tyname: whether this type has reflection info */
	char isemitted;  /* Tyname: whether this type has been emitted */
	char resolved; /* Have we resolved the subtypes? Prevents infinite recursion. */
	char fixed;	/* Have we fixed the subtypes? Prevents infinite recursion. */

};

struct Ucon {
	Srcloc loc;
	size_t id;   /* unique id */
	int synth;   /* is it generated? */
	Node *name;  /* ucon name */
	Type *utype; /* type of the union this is an element of */
	Type *etype; /* type for the element */
};

struct Trait {
	int uid; /* unique id */
	Srcloc loc;
	Vis vis;

	Node *name;  /* the name of the trait */
	Type *param; /* the type parameter */
	Type **aux;	/* auxiliary parameters */
	size_t naux;
	Node **memb; /* type must have these members */
	size_t nmemb;
	Node **funcs; /* and declare these funcs */
	size_t nfuncs;

	char isproto;  /* is it a prototype (for exporting purposes) */
	char ishidden; /* should user code be able to use this? */
	char isimport; /* have we defined it locally? */
};

struct Node {
	Srcloc loc;
	Ntype type;
	int nid;
	union {
		struct {
			size_t nfiles; /* file names for location mapping */
			char **files;
			Node **uses; /* use files that we loaded */
			size_t nuses;
			char **libdeps; /* library dependencies */
			size_t nlibdeps;
			char **extlibs; /* non-myrddin libraries */
			size_t nextlibs;
			Node **stmts; /* all top level statements */
			size_t nstmts;
			Node **init; /* a list of all __init__ function names of our deps. NB, this
					   is a Nname, not an Ndecl */
			size_t ninit;
			Node *localinit; /* and the local one, if any */
			Stab *globls;	/* global symtab */
			Stab *builtins;	/* global symtab */
			Htab *ns;	/* namespaces */
		} file;

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
			/*
			 If we have a link to a trait, we should only look it up
			 when specializing, but we should not create a new decl
			 node for it. That will be done when specializing the
			 impl.
			*/
			Trait *trait;
			Htab *impls;
			Node **gimpl; /* generic impls of this trait */
			size_t ngimpl;
			Node **gtype; /* generic impls of this trait */
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
			char isexportinit;
			char isinit;
		} decl;

		struct {
			Stab *scope;
			Type *type;
			size_t nargs;
			Node **args;
			Node *body;
		} func;

		struct {
			Node *name;
			size_t traitid;

			Node **funcs;
			size_t nfuncs;
			Node **membs;
			size_t nmembs;
		} trait;

		struct {
			Node *traitname;
			Trait *trait;
			Type *type;
			Type **aux;
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
extern Srcloc curloc;
extern char *filename;
extern Tok *curtok;  /* the last token we tokenized */
extern Node *file;   /* the current file we're compiling */
extern Type **tytab; /* type -> type map used by inference. size maintained by type creation code */
extern Type **types;
extern size_t ntypes;
extern Trait **traittab; /* int -> trait map */
extern size_t ntraittab;
extern Node **impltab; /* int -> impl map */
extern size_t nimpltab;
extern Node **decls; /* decl id -> decl map */
extern size_t nnodes;
extern Node **nodes; /* node id -> node map */
extern size_t ndecls;
extern Node **exportimpls;
extern size_t nexportimpls;

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

void putns(Node *file, Stab *scope);
void puttype(Stab *st, Node *n, Type *ty);
void puttrait(Stab *st, Node *n, Trait *trait);
void putimpl(Stab *st, Node *impl);
void updatetype(Stab *st, Node *n, Type *t);
void putdcl(Stab *st, Node *dcl);
void forcedcl(Stab *st, Node *dcl);
void putucon(Stab *st, Ucon *uc);
void putlbl(Stab *st, char *name, Node *lbl);

Stab *getns(Node *file, char *n);
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
	Node **memb, size_t nmemb,
	Node **funcs, size_t nfuncs,
	int isproto);
Type *mktylike(Srcloc l, Ty ty); /* constrains tyvar t like it was builtin ty */
Ucon *finducon(Type *t, Node *name);
int isstacktype(Type *t);
int istysigned(Type *t);
int istyunsigned(Type *t);
int istyfloat(Type *t);
int istyprimitive(Type *t);
int hasparams(Type *t);

/* type manipulation */
Type *tybase(Type *t);
char *tyfmt(char *buf, size_t len, Type *t);
char *tystr(Type *t);
size_t tyidfmt(char *buf, size_t len, Type *t);

int hastrait(Type *t, Trait *c);
int settrait(Type *t, Trait *c);
int traiteq(Type *t, Trait **traits, size_t len);
int traitfmt(char *buf, size_t len, Type *t);
char *traitstr(Type *t);

/* node creation */
Node *mknode(Srcloc l, Ntype nt);
Node *mkfile(char *name);
Node *mkuse(Srcloc l, char *use, int islocal);
Node *mksliceexpr(Srcloc l, Node *sl, Node *base, Node *off);
Node *mkexprl(Srcloc l, Op op, Node **args, size_t nargs);
Node *mkexpr(Srcloc l, Op op, ...); /* NULL terminated */
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
Node *mkint(Srcloc l, uint64_t val);
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
uint64_t arraysz(Node *sz);
char *namestr(Node *name);
char *lblstr(Node *n);
char *declname(Node *n);
Type *decltype(Node * n);
Type *exprtype(Node *n);
Type *nodetype(Node *n);
void addstmt(Node *file, Node *stmt);
void setns(Node *n, char *ns);
void updatens(Stab *st, char *ns);
ulong varhash(void *dcl);
int vareq(void *a, void *b);
Op exprop(Node *n);

/* specialize generics */
Tysubst *mksubst();
void substfree();
void substput(Tysubst *subst, Type *from, Type *to);
Type *substget(Tysubst *subst, Type *from);
void substpush(Tysubst *subst);
void substpop(Tysubst *subst);
Node *specializedcl(Node *n, Type *to, Node **name);
Type *tyspecialize(Type *t, Tysubst *tymap, Htab *delayed, Htab *tybase);
Node *genericname(Node *n, Type *t);
void geninit(Node *file);

/* usefiles */
int loaduse(char *path, FILE *f, Stab *into, Vis vis);
void readuse(Node *use, Stab *into, Vis vis);
void writeuse(FILE *fd, Node *file);
void tagexports(Node *file, int hidelocal);
void tagreflect(Type *t);
void addextlibs(Node *file, char **libs, size_t nlibs);

/* expression folding */
Node *fold(Node *n, int foldvar);

/* typechecking/inference */
void infer(Node *file);
Type *tysearch(Type *t);

/* debug */
void dump(Node *t, FILE *fd);
void dumpsym(Node *s, FILE *fd);
void dumpstab(Stab *st, FILE *fd);

/* option parsing */
void optinit(Optctx *ctx, char *optstr, char **optargs, size_t noptargs);
int optnext(Optctx *c);
int optdone(Optctx *c);

/* Options to control the compilation */
extern char debugopt[128];
extern int asmonly;
extern char *outfile;
extern char **incpaths;
extern size_t nincpaths;

void yyerror(const char *s);
