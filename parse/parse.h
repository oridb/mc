typedef uint32_t        unichar;
typedef unsigned int    uint;
typedef unsigned long   ulong;
typedef long long       vlong;
typedef unsigned long long uvlong;

typedef struct Bitset Bitset;
typedef struct Htab Htab;

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
#define Tc(c, n) c,
#include "cstr.def"
#undef Tc
} Tc;

typedef enum {
    Dclconst = 1 << 0,
    Dclextern = 1 << 1,
} Dclflags;

struct Bitset {
    int nchunks;
    uint *chunks;
};

struct Htab {
    size_t nelt;
    size_t sz;
    ulong (*hash)(void *k);
    int (*cmp)(void *a, void *b);
    void **keys;
    void **vals;
    ulong *hashes;
};

struct Tok {
    int type;
    int line;
    char *str;
};

struct Stab {
    Stab *super;
    Node *name;

    /* Contents of stab.
     * types and values are in separate namespaces. */
    Htab *ns;
    Htab *dcl;
    Htab *ty;
};

struct Sym {
    int   line;
    Node *name;
    Type *type;
};

struct Type {
    Ty type;
    int tid;
    Bitset *cstrs;    /* the type constraints matched on this type */
    size_t nsub;      /* For fnsub, tusub, sdecls, udecls, edecls. */
    Type **sub;       /* sub-types; shared by all composite types */
    union {
        Node *name;    /* Tyname: unresolved name */
        Node *asize;   /* array size */
        char *pname;   /* Typaram: name of type parameter */
        Node **sdecls; /* Tystruct: decls in struct */
        Node **udecls; /* Tyunion: decls in union */
        Node **edecls; /* Tyenum: decls in enum */
    };
};

struct Cstr {
    int cid;    /* unique id */
    char *name;
    Node **memb;        /* type must have these members */
    size_t nmemb;
    Node **funcs;       /* and declare these funcs */
    size_t nfuncs;
};

struct Node {
    int line;
    Ntype type;
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
            Type *type;
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
            int flags;
            Node *init;
        } decl;

        struct {
            Stab *scope;
            size_t nargs;
            Node **args;
            Node *body;
        } func;
    };
};

/* globals */
extern int debug;
extern char *filename;
extern int ignorenl;    /* are '\n' chars currently stmt separators? */
extern Tok *curtok;     /* the last token we tokenized */
extern int line;        /* the last line number we tokenized */
extern Node *file;      /* the current file we're compiling */
extern Stab *curscope;  /* the current scope to insert symbols into */
extern Type **tytab;    /* type -> type map used by inference. size maintained by type creation code */
extern int ntypes;
extern Cstr **cstrtab;  /* int -> cstr map */
extern int ncstrs;

extern Type *littypes[]; /* literal type -> type map */

/* data structures */
Bitset *mkbs();
Bitset *dupbs(Bitset *bs);
void delbs(Bitset *bs);
void bsput(Bitset *bs, uint elt);
void bsdel(Bitset *bs, uint elt);
int  bshas(Bitset *bs, uint elt);
void bsunion(Bitset *a, Bitset *b);
void bsintersect(Bitset *a, Bitset *b);
void bsdiff(Bitset *a, Bitset *b);
int  bscount(Bitset *bs);

Htab *mkht(ulong (*hash)(void *key), int (*cmp)(void *k1, void *k2));
int htput(Htab *ht, void *k, void *v);
void *htget(Htab *ht, void *k);
int hthas(Htab *ht, void *k);
void **htkeys(Htab *ht, int *nkeys);
/* useful key types */
ulong strhash(void *str);
int streq(void *s1, void *s2);

/* util functions */
void *zalloc(size_t size);
void *xalloc(size_t size);
void *zrealloc(void *p, size_t oldsz, size_t size);
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

void putns(Stab *st, Stab *scope);
void puttype(Stab *st, Node *n, Type *ty);
void putdcl(Stab *st, Sym *s);

Stab *getns(Stab *st, Node *n);
Sym *getdcl(Stab *st, Node *n);
Type *gettype(Stab *st, Node *n);

Sym *mksym(int line, Node *name, Type *ty);

/* type creation */
void tyinit(); /* sets up built in types */

Type *mkty(int line, Ty ty);
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
Cstr *mkcstr(int line, char *name, Node **memb, size_t nmemb, Node **funcs, size_t nfuncs);
Type *tylike(Type *t, Ty ty); /* constrains tyvar t like it was builtin ty */

/* type manipulation */
int hascstr(Type *t, Cstr *c);
int cstreq(Type *t, Cstr **cstrs, size_t len);
int constrain(Type *t, Cstr *c);
char *tyfmt(char *buf, size_t len, Type *t);
char *tystr(Type *t);

void tlappend(Type ***tl, int *len, Type *t);

/* node creation */
Node *mknode(int line, Ntype nt);
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

/* usefiles */
void readuse(Node *use, Stab *into);

/* typechecking/inference */
void infer(Node *file);

/* debug */
void dump(Node *t, FILE *fd);
void dumpsym(Sym *s, FILE *fd);
void dumpstab(Stab *st, FILE *fd);
char *opstr(Op o);
char *nodestr(Ntype nt);
char *litstr(Littype lt);
char *tidstr(Ty tid);

/* serialization/usefiles */
void pickle(Node *n, FILE *fd);
Node *unpickle(FILE *fd);

/* convenience func */
void nlappend(Node ***nl, size_t *len, Node *n);

/* backend functions */
void gen();
