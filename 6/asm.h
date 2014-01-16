#define Maxarg 4                /* maximum number of args an insn can have */
#define Maxuse (2*Maxarg)       /* maximum number of registers an insn can use or def */
#define Maxdef (2*Maxarg)       /* maximum number of registers an insn can use or def */
#define Wordsz 4                /* the size of a "natural int" */
#define Ptrsz 8                 /* the size of a machine word (ie, pointer size) */
#define Nsaved 13               /* number of registers saved in the ABI */

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
    Loclit,  /* literal value */
    Loclitl  /* label address */
} Loctype;

typedef enum {
    ModeNone,
    ModeB, /* byte */
    ModeW, /* short */
    ModeL, /* long */
    ModeQ, /* quad */
    ModeF, /* float32 */
    ModeD, /* float64 */
    Nmode,
} Mode;

typedef enum {
    Classbad,
    Classint,
    Classflt,
    Nclass,
} Rclass;

/* a register, label, or memory location */
struct Loc {
    Loctype type; /* the type of loc */
    Mode mode;    /* the mode of this location */
    union {
        char *lbl;  /* for Loclbl, Loclitl */
        struct {    /* for Locreg */
            regid id;
            Reg   colour;
        } reg;
        long  lit;  /* for Loclit */
        /*
         * for Locmem, Locmeml.
         * address format is
         *    disp(base + index)
         */
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
    char *name;   /* function name */
    int   isexport; /* is this exported from the asm? */
    size_t stksz; /* stack size */
    Type *type;   /* type of function */
    Htab *stkoff; /* Loc* -> int stackoff map */
    Node *ret;    /* return value */
    Cfg  *cfg;    /* flow graph */
};

struct Asmbb {
    int id;       /* unique identifier */
    char **lbls;  /* list of BB labels */
    size_t nlbls; /* number of labels */
    Insn **il;    /* instructions */
    size_t ni;    /* number of instructions */

    Bitset *pred; /* set of predecessor BB ids */
    Bitset *succ; /* set of successor BB ids */
    Bitset *use;  /* registers used by this BB */
    Bitset *def;  /* registers defined by this BB */
    Bitset *livein; /* variables live on entrance to BB */
    Bitset *liveout;  /* variables live on exit from BB */
};


/* instruction selection state */
struct Isel {
    Cfg  *cfg;          /* cfg built with nodes */

    Asmbb **bb;         /* 1:1 mappings with the Node bbs in the CFG */
    size_t nbb;
    Asmbb *curbb;

    Node *ret;          /* we store the return into here */
    Htab *spillslots;   /* reg id  => int stkoff */
    Htab *reglocs;      /* decl id => Loc *reg */
    Htab *stkoff;       /* decl id => int stkoff */
    Htab *globls;       /* decl id => char *globlname */

    /* increased when we spill */
    Loc *stksz;
    Loc *calleesave[Nsaved];

    /* register allocator state */
    Bitset *prepainted; /* locations that need to be a specific colour */
    Bitset *initial;    /* initial set of locations used by this fn */

    size_t *gbits;      /* igraph matrix repr */
    Bitset **gadj;      /* igraph adj set repr */
    int *degree;        /* degree of nodes */
    Loc **aliasmap;     /* mapping of aliases */

    Loc  **selstk;
    size_t nselstk;

    Bitset *coalesced;
    Bitset *spilled;
    Bitset *shouldspill;        /* the first registers we should try to spill */
    Bitset *neverspill;        /* registers we should never spill */

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
void genblob(FILE *fd, Node *blob, Htab *globls, Htab *strtab);
void genasm(FILE *fd, Func *fn, Htab *globls, Htab *strtab);
void genstrings(FILE *fd, Htab *strtab);
void gen(Node *file, char *out);

/* location generation */
extern size_t maxregid;
extern Loc **locmap; /* mapping from reg id => Loc * */

char *genlblstr(char *buf, size_t sz);
Node *genlbl(void);
Loc *loclbl(Node *lbl);
Loc *locstrlbl(char *lbl);
Loc *locreg(Mode m);
Loc *locphysreg(Reg r);
Loc *locmem(long disp, Loc *base, Loc *idx, Mode mode);
Loc *locmeml(char *disp, Loc *base, Loc *idx, Mode mode);
Loc *locmems(long disp, Loc *base, Loc *idx, int scale, Mode mode);
Loc *locmemls(char *disp, Loc *base, Loc *idx, int scale, Mode mode);
Loc *loclit(long val, Mode m);
Loc *loclitl(char *lbl);
Loc *coreg(Reg r, Mode m);

void locprint(FILE *fd, Loc *l, char spec);
void iprintf(FILE *fd, Insn *insn);

/* emitting instructions */
Insn *mkinsn(AsmOp op, ...);

/* register allocation */
extern char *regnames[]; /* name table */
extern Mode regmodes[];  /* mode table */
extern size_t modesize[]; /* mode size table */
void regalloc(Isel *s);
Rclass rclass(Loc *l);


/* useful functions */
size_t tysize(Type *t);
size_t size(Node *n);
int stacktype(Type *t);
int floattype(Type *t);
int stacknode(Node *n);
int floatnode(Node *n);
void breakhere();
void dumpasm(Isel *s, FILE *fd);

size_t tyalign(size_t sz, size_t eltsz);

