typedef uint32_t        unichar;
typedef long long       vlong;
typedef unsigned long long uvlong;

typedef struct Tok Tok;
typedef struct Node Node;
typedef struct Stab Stab;
typedef struct Sym Sym;

typedef struct Type Type;
typedef struct Cstr Cstr;


typedef enum {
#define O(op) op,
#include "ops.def"
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
#define Ty(t) t,
#include "types.def"
#undef Ty
} Ty;

typedef enum {
    Dclconst = 1 << 0,
    Dclextern = 1 << 1,
} Dclflags;


struct Tok {
    int type;
    int line;
    char *str;
};

struct Stab {
    Stab *super;
    char *name;

    /* Contents of stab.
     * Types and values are in separate namespaces. */
    size_t ntypes;
    Sym **types;
    size_t nsyms;
    Sym **syms;
};

/* reused for both decls and types */
struct Sym {
    int   line;
    Node *name;
    Type *type;
};

struct Type {
    Ty type;
    int tid;
    size_t nsub;      /* For fnsub, tusub, sdecls, udecls, edecls. */
    union {
        Node *name;   /* Tyname: unresolved name */
        Type **fnsub; /* Tyfunc: return, args */
        Type **tusub; /* Tytuple: element types */
        Type *pbase;  /* Typtr: pointer target */
        Type *sbase;  /* Tyslice: slice target */
        struct {      /* Tyarray: array target and size */
            Type *abase;
            Node *asize;
        };
        char *pname;   /* Typaram: name of type parameter */
        Node **sdecls; /* Tystruct: decls in struct */
        Node **udecls; /* Tyunion: decls in union */
        Node **edecls; /* Tyenum: decls in enum */
    };
};

struct Cstr {
    int cid;    /* unique id */
    /* required members */
    size_t nreqmemb;
    Node **reqmemb;
    /* required functions */
    size_t nreqfn;
    Node **reqfn;
};

struct Node {
    int line;
    int type;
    union {
        struct {
            char  *name;
            size_t nuses;
            Node **uses;
            size_t nstmts;
            Node **stmts;
            Stab  *globls;
            Stab  *exports;
        } file;

        struct {
            Op op;
            int isconst;
            size_t nargs;
            Node **args;
        } expr;

        struct {
            size_t nparts;
            char **parts;
        } name;

        struct {
            int islocal;
            char *name;
        } use;

        struct {
            Littype littype;
            Type    *type;
            union {
                uvlong   intval;
                double   fltval;
                unichar  chrval;
                char    *strval;
                int      boolval;
                Node    *fnval;
                Node    *arrval;
            };
        } lit;

        struct {
            Node *init;
            Node *cond;
            Node *step;
            Node *body;
        } loopstmt;

        struct {
            Node *cond;
            Node *iftrue;
            Node *iffalse;
        } ifstmt;

        struct {
            Stab *scope;
            size_t nstmts;
            Node **stmts;
        } block;

        struct {
            char *name;
        } lbl;

        struct {
            Sym *sym;
            Node *init;
            int flags;
        } decl;

        struct {
            Stab *scope;
            Node **args;
            size_t nargs;
            Node *body;
        } func;
    };
};

/* globals */
extern int debug;
extern char *filename;
extern int ignorenl;
extern int line;
extern Tok *curtok;
extern Node *file;
extern Stab *scope;

/* util functions */
void *zalloc(size_t size);
void *xalloc(size_t size);
void *xrealloc(void *p, size_t size);
void  die(char *msg, ...);
void  fatal(int line, char *fmt, ...);
char *strdupn(char *s, size_t len);
void *memdup(void *mem, size_t len);

/* parsing etc */
void tokinit(char *file);
int yylex(void);
int yyparse(void);

/* stab creation */
Stab *mkstab(Stab *super);
Stab *stput(Sym *decl);
Sym  *stget(char *name);
Sym *mksym(int line, Node *name, Type *ty);

/* type creation */
void tyinit(); /* sets up built in types */

Type *mktyvar(int line);
Type *mktyparam(int line, char *name);
Type *mktynamed(int line, Node *name);
Type *mktyarray(int line, Type *base, Node *sz);
Type *mktyslice(int line, Type *base);
Type *mktyptr(int line, Type *base);
Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret);
Type *mktystruct(int line, Node **decls, size_t ndecls);
Type *mktyunion(int line, Node **decls, size_t ndecls);
Type *mktyenum(int line, Node **decls, size_t ndecls);
Cstr *mkcstr(int line, char *name, Node **reqmemb, size_t nreqmemb, Node **reqdecl, size_t nreqdecl); 

/* type manipulation */
int hascstr(Type *t, Cstr *c);
char *tyfmt(char *buf, size_t len, Type *t);
char *tystr(Type *t);

void tlappend(Type ***tl, int *len, Type *t);

/* node creation */
Node *mkfile(char *name);
Node *mkuse(int line, char *use, int islocal);
Node *mkexpr(int line, Op op, ...); /* NULL terminated */
Node *mkcall(int line, Node *fn, Node **args, size_t nargs);
Node *mklit(int line, Littype lt, void *val);
Node *mkif(int line, Node *cond, Node *iftrue, Node *iffalse);
Node *mkloop(int line, Node *init, Node *cond, Node *incr, Node *body);
Node *mkblock(int line, Stab *scope);

Node *mkbool(int line, int val);
Node *mkint(int line, uint64_t val);
Node *mkchar(int line, uint32_t val);
Node *mkstr(int line, char *s);
Node *mkfloat(int line, double flt);
Node *mkfunc(int line, Node **args, size_t nargs, Node *body);
Node *mkarray(int line, Node **vals);
Node *mkname(int line, char *name);
Node *mkdecl(int line, Sym *sym);
Node *mklabel(int line, char *lbl);

/* node util functions */
Type *decltype(Node *n);
void addstmt(Node *file, Node *stmt);
void setns(Node *n, char *name);
void def(Stab *s, Node *n);
void deftype(int line, Stab *s, char *name, Type *t);

/* usefiles */
void readuse(Node *use, Stab *into);

/* typechecking/inference */
void infer(Node *file);

/* debug */
void dump(Node *t, FILE *fd);
void dumpsym(Sym *s, FILE *fd);
char *opstr(Op o);
char *nodestr(Ntype nt);
char *litstr(Littype lt);
char *tidstr(Ty tid);

/* convenience func */
void nlappend(Node ***nl, size_t *len, Node *n);
void slappend(Sym ***sl, size_t *len, Sym *s);

/* backend functions */
void gen();
