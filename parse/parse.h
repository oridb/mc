#define FATAL __attribute__((noreturn))

typedef uint8_t         byte;
typedef unsigned int    uint;
typedef unsigned long   ulong;
typedef long long       vlong;
typedef unsigned long long uvlong;

typedef struct Bitset Bitset;
typedef struct Htab Htab;

typedef struct Tok Tok;
typedef struct Node Node;
typedef struct Ucon Ucon;
typedef struct Stab Stab;

typedef struct Type Type;
typedef struct Cstr Cstr;

typedef enum {
#define O(op, pure) op,
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
#define Ty(t, n) t,
#include "types.def"
#undef Ty
    Ntypes
} Ty;

typedef enum {
#define Tc(c, n) c,
#include "cstr.def"
#undef Tc
} Tc;

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

struct Bitset {
    size_t nchunks;
    size_t *chunks;
};

struct Htab {
    size_t nelt;
    size_t sz;
    ulong (*hash)(void *k);
    int (*cmp)(void *a, void *b);
    void **keys;
    void **vals;
    ulong *hashes;
    char  *dead;
};

struct Tok {
    int type;
    int line;
    char *str;

    /* values parsed out */
    vlong intval;
    double fltval;
    uint32_t chrval;
};

struct Stab {
    Stab *super;
    Node *name;

    /* Contents of stab.
     * types and values are in separate namespaces. */
    Htab *dcl;
    Htab *closure; /* the syms we close over */
    Htab *ns;
    Htab *ty;
    Htab *uc;
};

struct Type {
    Ty type;
    int tid;
    int line;
    Vis vis;

    int resolved;       /* Have we resolved the subtypes? Prevents infinite recursion. */
    int fixed;          /* Have we fixed the subtypes? Prevents infinite recursion. */

    Bitset *cstrs;      /* the type constraints matched on this type */
    Node **cstrlist;    /* The names of the constraints on the type. Used to fill the bitset */
    size_t ncstrlist;   /* The length of the constraint list above */

    int  issynth;       /* Tyname: whether this is synthesized or not */
    int  ishidden;      /* Tyname: whether this is hidden or not */
    Type **param;       /* Tyname: type parameters that match the type args */
    size_t nparam;      /* Tyname: count of type parameters */
    Type **arg;         /* Tyname: type arguments instantiated */
    size_t narg;        /* Tyname: count of type arguments */
    Type **inst;        /* Tyname: instances created */
    size_t ninst;       /* Tyname: count of instances created */

    Type **sub;         /* sub-types; shared by all composite types */
    size_t nsub;        /* For compound types */
    size_t nmemb;       /* for aggregate types (struct, union) */
    union {
        Node *name;     /* Tyname: unresolved name. Tyalias: alias name */
        Node *asize;    /* array size */
        char *pname;    /* Typaram: name of type parameter */
        Node **sdecls;  /* Tystruct: decls in struct */
        Ucon **udecls;  /* Tyunion: decls in union */
    };
};

struct Ucon {
    int line;   /* line declared on */
    size_t id;  /* unique id */
    int synth;  /* is it generated? */
    Node *name; /* ucon name */
    Type *utype;        /* type of the union this is an element of */
    Type *etype;        /* type for the element */
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
    int nid;
    union {
        struct {
            char  *name;
            size_t nuses;
            Node **uses;
            size_t nlibdeps;
            char **libdeps;
            size_t nstmts;
            Node **stmts;
            Stab  *globls;
            Stab  *exports;
        } file;

        struct {
            Op op;
            Type *type;
            int isconst;
            size_t did; /* for Ovar, we want a mapping to the decl id */
            size_t nargs;
            Node *idx; /* used when this is in an indexed initializer */
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
            Type    *type;
            size_t   nelt;
            union {
                uvlong   intval;
                double   fltval;
                uint32_t chrval;
                char    *strval;
                char    *lblval;
                int      boolval;
                Node    *fnval;
                Node    **seqval;
            };
        } lit;

        struct {
            Node *init;
            Node *cond;
            Node *step;
            Node *body;
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
            Node **impls;
            size_t nimpls;
            char  vis;
            char  isglobl;
            char  isconst;
            char  isgeneric;
            char  istrait;
            char  isextern;
            char  ishidden;
        } decl;

        struct {
            long  uid;
            Node *name;
            Type *elt;
            Type *alt;
        } uelt;

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

            
    };
};

/* globals */
extern char *filename;
extern Tok *curtok;     /* the last token we tokenized */
extern int line;        /* the last line number we tokenized */
extern Node *file;      /* the current file we're compiling */
extern Type **tytab;    /* type -> type map used by inference. size maintained by type creation code */
extern Type **types;
extern size_t ntypes;
extern Cstr **cstrtab;  /* int -> cstr map */
extern size_t ncstrs;
extern Node **decls;    /* decl id -> decl map */
extern size_t ndecls;
extern size_t maxnid;      /* the maximum node id generated so far */

extern int ispureop[];

/* data structures */
Bitset *mkbs(void);
void    bsfree(Bitset *bs);
Bitset *bsdup(Bitset *bs);
Bitset *bsclear(Bitset *bs);
void delbs(Bitset *bs);
void bsput(Bitset *bs, size_t elt);
void bsdel(Bitset *bs, size_t elt);
void bsunion(Bitset *a, Bitset *b);
void bsintersect(Bitset *a, Bitset *b);
void bsdiff(Bitset *a, Bitset *b);
int  bshas(Bitset *bs, size_t elt);
int  bseq(Bitset *a, Bitset *b);
int  bsissubset(Bitset *set, Bitset *sub);
int  bsiter(Bitset *bs, size_t *elt);
size_t bsmax(Bitset *bs);
size_t bscount(Bitset *bs);

Htab *mkht(ulong (*hash)(void *key), int (*cmp)(void *k1, void *k2));
void htfree(Htab *ht);
int htput(Htab *ht, void *k, void *v);
void htdel(Htab *ht, void *k);
void *htget(Htab *ht, void *k);
int hthas(Htab *ht, void *k);
void **htkeys(Htab *ht, size_t *nkeys);
/* useful key types */
ulong strhash(void *key);
int streq(void *a, void *b);
ulong ptrhash(void *key);
int ptreq(void *a, void *b);
ulong inthash(uint64_t key);
int inteq(uint64_t a, uint64_t b);
ulong tyhash(void *t);
int tyeq(void *a, void *b);
ulong namehash(void *t);
int nameeq(void *a, void *b);

/* util functions */
void *zalloc(size_t size);
void *xalloc(size_t size);
void *zrealloc(void *p, size_t oldsz, size_t size);
void *xrealloc(void *p, size_t size);
void  die(char *msg, ...) FATAL;
void  fatal(int line, char *fmt, ...) FATAL;
char *strdupn(char *s, size_t len);
char *strjoin(char *u, char *v);
void *memdup(void *mem, size_t len);

/* parsing etc */
void tokinit(char *file);
int yylex(void);
int yyparse(void);

/* stab creation */
Stab *mkstab(void);

void putns(Stab *st, Stab *scope);
void puttype(Stab *st, Node *n, Type *ty);
void putcstr(Stab *st, Node *n, Cstr *cstr);
void updatetype(Stab *st, Node *n, Type *t);
void putdcl(Stab *st, Node *dcl);
void forcedcl(Stab *st, Node *dcl);
void putucon(Stab *st, Ucon *uc);

Stab *getns(Stab *st, Node *n);
Stab *getns_str(Stab *st, char *n);
Node *getdcl(Stab *st, Node *n);
Type *gettype_l(Stab *st, Node *n);
Type *gettype(Stab *st, Node *n);
Cstr *getcstr(Stab *st, Node *n);
Ucon *getucon(Stab *st, Node *n);

Stab *curstab(void);
void pushstab(Stab *st);
void popstab(void);

/* type creation */
void tyinit(Stab *st); /* sets up built in types */

Type *mktype(int line, Ty ty);
Type *tydup(Type *t); /* shallow duplicate; all subtypes/members/... kept */
Type *mktyvar(int line);
Type *mktyparam(int line, char *name);
Type *mktyname(int line, Node *name, Type **params, size_t nparams, Type *base);
Type *mktyunres(int line, Node *name, Type **params, size_t nparams);
Type *mktyarray(int line, Type *base, Node *sz);
Type *mktyslice(int line, Type *base);
Type *mktyidxhack(int line, Type *base);
Type *mktyptr(int line, Type *base);
Type *mktytuple(int line, Type **sub, size_t nsub);
Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret);
Type *mktystruct(int line, Node **decls, size_t ndecls);
Type *mktyunion(int line, Ucon **decls, size_t ndecls);
Cstr *mkcstr(int line, char *name, Node **memb, size_t nmemb, Node **funcs, size_t nfuncs);
Type *mktylike(int line, Ty ty); /* constrains tyvar t like it was builtin ty */
int   istysigned(Type *t);
int   istyfloat(Type *t);
int   isgeneric(Type *t);
int   hasparams(Type *t);

/* type manipulation */
Type *tybase(Type *t);
int hascstr(Type *t, Cstr *c);
int cstreq(Type *t, Cstr **cstrs, size_t len);
int setcstr(Type *t, Cstr *c);
char *tyfmt(char *buf, size_t len, Type *t);
int cstrfmt(char *buf, size_t len, Type *t);
char *cstrstr(Type *t);
char *tystr(Type *t);

/* node creation */
Node *mknode(int line, Ntype nt);
Node *mkfile(char *name);
Node *mkuse(int line, char *use, int islocal);
Node *mksliceexpr(int line, Node *sl, Node *base, Node *off);
Node *mkexprl(int line, Op op, Node **args, size_t nargs);
Node *mkexpr(int line, Op op, ...); /* NULL terminated */
Node *mkcall(int line, Node *fn, Node **args, size_t nargs);
Node *mkifstmt(int line, Node *cond, Node *iftrue, Node *iffalse);
Node *mkloopstmt(int line, Node *init, Node *cond, Node *incr, Node *body);
Node *mkiterstmt(int line, Node *elt, Node *seq, Node *body);
Node *mkmatchstmt(int line, Node *val, Node **matches, size_t nmatches);
Node *mkmatch(int line, Node *pat, Node *body);
Node *mkblock(int line, Stab *scope);
Node *mktrait(int line, Node *name, Node **funcs, size_t nfuncs, Node **membs, size_t nmembs);
Node *mkintlit(int line, uvlong val);
Node *mkidxinit(int line, Node *idx, Node *init);

Node *mkbool(int line, int val);
Node *mkint(int line, uint64_t val);
Node *mkchar(int line, uint32_t val);
Node *mkstr(int line, char *s);
Node *mkfloat(int line, double flt);
Node *mkfunc(int line, Node **args, size_t nargs, Type *ret, Node *body);
Node *mkname(int line, char *name);
Node *mknsname(int line, char *ns, char *name);
Node *mkdecl(int line, Node *name, Type *ty);
Node *mklbl(int line, char *lbl);
Node *mkslice(int line, Node *base, Node *off);
Ucon *mkucon(int line, Node *name, Type *ut, Type *uet);

/* node util functions */
char *namestr(Node *name);
char *declname(Node *n);
Type *decltype(Node *n);
Type *exprtype(Node *n);
Type *nodetype(Node *n);
void addstmt(Node *file, Node *stmt);
void setns(Node *n, char *ns);
void updatens(Stab *st, char *ns);
ulong varhash(void *dcl);
int vareq(void *a, void *b);
Op exprop(Node *n);

/* specialize generics */
Node *specializedcl(Node *n, Type *to, Node **name);
Type *tyspecialize(Type *t, Htab *tymap);

/* usefiles */
int  loaduse(FILE *f, Stab *into);
void readuse(Node *use, Stab *into);
void writeuse(FILE *fd, Node *file);
void tagexports(Stab *st);

/* typechecking/inference */
void infer(Node *file);

/* debug */
void dump(Node *t, FILE *fd);
void dumpsym(Node *s, FILE *fd);
void dumpstab(Stab *st, FILE *fd);
char *opstr(Op o);
char *nodestr(Ntype nt);
char *litstr(Littype lt);
char *tidstr(Ty tid);

/* convenience funcs */
void lappend(void *l, size_t *len, void *n); /* hack; nl is void* b/c void*** is incompatible with T*** */
void linsert(void *l, size_t *len, size_t idx, void *n);
void *lpop(void *l, size_t *len);
void ldel(void *l, size_t *len, size_t idx);
void lfree(void *l, size_t *len);

/* serializing/unserializing */
void be64(vlong v, byte buf[8]);
vlong host64(byte buf[8]);
void be32(long v, byte buf[4]);
long host32(byte buf[4]);

void wrbuf(FILE *fd, void *buf, size_t sz);
void rdbuf(FILE *fd, void *buf, size_t sz);
char rdbyte(FILE *fd);
void wrbyte(FILE *fd, char val);
char rdbyte(FILE *fd);
void wrint(FILE *fd, long val);
long rdint(FILE *fd);
void wrstr(FILE *fd, char *val);
char *rdstr(FILE *fd);
void wrflt(FILE *fd, double val);
double rdflt(FILE *fd);
void wrbool(FILE *fd, int val);
int rdbool(FILE *fd);

size_t max(size_t a, size_t b);
size_t min(size_t a, size_t b);
size_t align(size_t sz, size_t a);

/* suffix replacement */
char *swapsuffix(char *buf, size_t sz, char *s, char *suf, char *swap);

/* Options to control the compilation */
extern int yydebug;
extern char debugopt[128];
extern int asmonly;
extern char *outfile;
extern char **incpaths;
extern size_t nincpaths;
