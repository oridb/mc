#define Maxarg 4
#define K 4 /* 4 general purpose regs with all modes available */

typedef size_t regid;

typedef struct Insn Insn;
typedef struct Loc Loc;
typedef struct Func Func;
typedef struct Blob Blob;
typedef struct Isel Isel;
typedef struct Asmbb Asmbb;

typedef enum {
#define Insn(val, fmt, use, def) val,
#include "insns.def"
#undef Insn
} AsmOp;

typedef enum {
#define Reg(r, name, mode) r,
#include "regs.def"
#undef Reg
    Nreg
} Reg;

typedef enum {
    Locnone,
    Loclbl,  /* label */
    Locreg,  /* register */
    Locmem,  /* reg offset mem */
    Locmeml, /* label offset mem */
    Loclit,
} LocType;

typedef enum {
    ModeNone,
    ModeB, /* byte */
    ModeS, /* short */
    ModeL, /* long */
    ModeF, /* float32 */
    ModeD, /* float64 */
    Nmode,
} Mode;

struct Blob {
    char *name; /* mangled asm name */
    void *data;
    size_t ndata;
};

struct Loc {
    LocType type;
    Mode mode;
    union {
        char *lbl;
        struct {
            regid id;
            Reg   colour;
        } reg;
        long  lit;
        /* disp(base + index) */
        struct {
            /* only one of lbldisp and constdisp may be used */
            char *lbldisp;
            long constdisp;
            int scale; /* 0,1,2,4, or 8 */
            Loc *base; /* needed */
            Loc *idx;  /* optional */
        } mem;
    };
};

struct Insn {
    AsmOp op;
    Loc *args[Maxarg];
    size_t nargs;
};

struct Func {
    char *name;
    int   isglobl;
    size_t stksz;
    Htab *locs;
    Node *ret;
    Cfg  *cfg;
};

struct Asmbb {
    int id;
    char **lbls;
    size_t nlbls;
    Insn **il;
    size_t ni;

    Bitset *pred;
    Bitset *succ;
    Bitset *use;
    Bitset *def;
    Bitset *livein;
    Bitset *liveout;
};


/* instruction selection state */
struct Isel {
    Cfg  *cfg;          /* cfg built with nodes */

    Asmbb **bb;         /* 1:1 mappings with the Node bbs in the CFG */
    size_t nbb;
    Asmbb *curbb;

    Node *ret;          /* we store the return into here */
    Htab *locs;         /* decl id => int stkoff */
    Htab *globls;       /* decl id => char *globlname */

    /* increased when we spill */
    Loc *stksz;

    /* register allocator state */
    Bitset *prepainted; /* locations that need to be a specific colour */

    size_t *gbits;      /* igraph matrix repr */
    Bitset **gadj;      /* igraph adj set repr */
    int *degree;        /* degree of nodes */
    Loc **aliasmap;   /* mapping of aliases */

    Loc  **selstk;
    size_t nselstk;

    Bitset *coalesced;
    Bitset *spilled;

    Insn ***rmoves;
    size_t *nrmoves;

    /* move sets */
    Insn **mcoalesced;
    size_t nmcoalesced;

    Insn **mconstrained;
    size_t nmconstrained;

    Insn **mfrozen;
    size_t nmfrozen;

    Insn **mactive;
    size_t nmactive;


    /* worklists */
    Insn **wlmove;
    size_t nwlmove;

    Loc **wlspill;
    size_t nwlspill;

    Loc **wlfreeze;
    size_t nwlfreeze;

    Loc **wlsimp;
    size_t nwlsimp;
};

/* entry points */
void genasm(FILE *fd, Func *fn, Htab *globls);
void gen(Node *file, char *out);

/* location generation */
extern size_t maxregid;
extern Loc **locmap; /* mapping from reg id => Loc * */

Loc *loclbl(Node *lbl);
Loc *locstrlbl(char *lbl);
Loc *locreg(Mode m);
Loc *locphysreg(Reg r);
Loc *locmem(long disp, Loc *base, Loc *idx, Mode mode);
Loc *locmeml(char *disp, Loc *base, Loc *idx, Mode mode);
Loc *locmems(long disp, Loc *base, Loc *idx, int scale, Mode mode);
Loc *locmemls(char *disp, Loc *base, Loc *idx, int scale, Mode mode);
Loc *loclit(long val);

void locprint(FILE *fd, Loc *l);
void iprintf(FILE *fd, Insn *insn);

/* register allocation */
extern char *regnames[]; /* name table */
extern Mode regmodes[];  /* mode table */

void regalloc(Isel *s);


/* useful functions */
size_t size(Node *n);
void breakhere();
void dumpasm(Isel *s, FILE *fd);
