typedef uint8_t         byte;
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
    Htab *closure; /* the syms we close over */
    Htab *ty;
};

struct Sym {
};

struct Type {
    Ty type;
    int tid;
    int line;
    int resolved;     /* Have we resolved the subtypes? Idempotent, but slow to repeat. */
    Bitset *cstrs;    /* the type constraints matched on this type */
    size_t nsub;      /* For compound types */
    size_t nmemb;     /* for aggregate types (struct, union) */
    Type **sub;       /* sub-types; shared by all composite types */
    union {
        Node *name;    /* Tyname: unresolved name */
        Node *asize;   /* array size */
        char *pname;   /* Typaram: name of type parameter */
        Node **sdecls; /* Tystruct: decls in struct */
        Node **udecls; /* Tyunion: decls in union */
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
    int nid;
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
            long   did; /* for Ovar, we want a mapping to the decl id */
            size_t nargs;
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
            long  did;
            int   isconst;
            int   isgeneric;
            int   isextern;
            Node *name;
            Type *type;
            Node *init;
        } decl;

        struct {
            Stab *scope;
            Type *type;
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
extern Type **tytab;    /* type -> type map used by inference. size maintained by type creation code */
extern int ntypes;
extern Cstr **cstrtab;  /* int -> cstr map */
extern int ncstrs;
extern int maxnid;      /* the maximum node id generated so far */
extern int maxdid;      /* the maximum decl id generated so far */

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
void *htget(Htab *ht, void *k);
int hthas(Htab *ht, void *k);
void **htkeys(Htab *ht, size_t *nkeys);
/* useful key types */
ulong strhash(void *str);
int streq(void *s1, void *s2);
ulong ptrhash(void *ptr);
int ptreq(void *s1, void *s2);

/* util functions */
void *zalloc(size_t size);
void *xalloc(size_t size);
void *zrealloc(void *p, size_t oldsz, size_t size);
void *xrealloc(void *p, size_t size);
void  die(char *msg, ...);
void  fatal(int line, char *fmt, ...);
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
void updatetype(Stab *st, Node *n, Type *t);
void putdcl(Stab *st, Node *s);

Stab *getns(Stab *st, Node *n);
Node *getdcl(Stab *st, Node *n);
Type *gettype(Stab *st, Node *n);

Stab *curstab(void);
void pushstab(Stab *st);
void popstab(void);

/* type creation */
void tyinit(Stab *st); /* sets up built in types */

Type *mkty(int line, Ty ty);
Type *tydup(Type *t); /* shallow duplicate; all subtypes/members/... kept */
Type *mktyvar(int line);
Type *mktyparam(int line, char *name);
Type *mktynamed(int line, Node *name);
Type *mktyarray(int line, Type *base, Node *sz);
Type *mktyslice(int line, Type *base);
Type *mktyidxhack(int line, Type *base);
Type *mktyptr(int line, Type *base);
Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret);
Type *mktystruct(int line, Node **decls, size_t ndecls);
Type *mktyunion(int line, Node **decls, size_t ndecls);
Cstr *mkcstr(int line, char *name, Node **memb, size_t nmemb, Node **funcs, size_t nfuncs);
Type *tylike(Type *t, Ty ty); /* constrains tyvar t like it was builtin ty */
int   istysigned(Type *t);

/* type manipulation */
int hascstr(Type *t, Cstr *c);
int cstreq(Type *t, Cstr **cstrs, size_t len);
int setcstr(Type *t, Cstr *c);
char *tyfmt(char *buf, size_t len, Type *t);
char *tystr(Type *t);

/* node creation */
Node *mknode(int line, Ntype nt);
Node *mkfile(char *name);
Node *mkuse(int line, char *use, int islocal);
Node *mkexpr(int line, Op op, ...); /* NULL terminated */
Node *mkcall(int line, Node *fn, Node **args, size_t nargs);
Node *mkif(int line, Node *cond, Node *iftrue, Node *iffalse);
Node *mkloop(int line, Node *init, Node *cond, Node *incr, Node *body);
Node *mkblock(int line, Stab *scope);
Node *mkintlit(int line, uvlong val);

Node *mkbool(int line, int val);
Node *mkint(int line, uint64_t val);
Node *mkchar(int line, uint32_t val);
Node *mkstr(int line, char *s);
Node *mkfloat(int line, double flt);
Node *mkfunc(int line, Node **args, size_t nargs, Type *ret, Node *body);
Node *mkarray(int line, Node **vals);
Node *mkname(int line, char *name);
Node *mknsname(int line, char *ns, char *name);
Node *mkdecl(int line, Node *name, Type *ty);
Node *mklbl(int line, char *lbl);
Node *mkslice(int line, Node *base, Node *off);

/* node util functions */
char *namestr(Node *name);
char *declname(Node *n);
Type *decltype(Node *n);
Type *exprtype(Node *n);
Type *nodetype(Node *n);
void addstmt(Node *file, Node *stmt);
void setns(Node *n, char *ns);
void updatens(Stab *st, char *ns);
Op exprop(Node *n);
Node **aggrmemb(Type *t, size_t *n);

/* specialize generics */
Node *specializedcl(Node *n, Type *to, Node **name);

/* usefiles */
void readuse(Node *use, Stab *into);
void writeuse(Node *file, FILE *out);

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

/* serialization/usefiles */
void typickle(Type *t, FILE *fd);
void sympickle(Node *s, FILE *fd);
void pickle(Node *n, FILE *fd);
Type *tyunpickle(FILE *fd);
Node *symunpickle(FILE *fd);
Node *unpickle(FILE *fd);

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

/* suffix replacement */
char *swapsuffix(char *buf, size_t sz, char *s, char *suf, char *swap);

/* Options to control the compilation */
extern int debug;
extern char debugopt[128];
extern int asmonly;
extern char *outfile;
extern char **incpaths;
extern size_t nincpaths;
