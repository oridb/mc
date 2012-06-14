#define MaxArg 4

typedef struct Insn Insn;
typedef struct Loc Loc;
typedef struct Func Func;
typedef struct Blob Blob;
typedef struct Isel Isel;
typedef struct Asmbb Asmbb;

typedef enum {
#define Insn(val, fmt, attr) val,
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
            long  pseudo;
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
    Loc *args[MaxArg];
    int narg;
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

    Bitset *in;
    Bitset *out;
    Bitset *livein;
    Bitset *liveout;
};


/* instruction selection state */
struct Isel {
    Cfg  *cfg;
    Asmbb **bb;
    size_t nbb;
    Asmbb *curbb;
    Node *ret;
    Htab *locs; /* decl id => int stkoff */
    Htab *globls; /* decl id => char *globlname */

    /* increased when we spill */
    Loc *stksz;
};

/* entry points */
void genasm(FILE *fd, Func *fn, Htab *globls);
void gen(Node *file, char *out);

/* location generation */
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
extern const char *regnames[];
extern const Mode regmodes[];


/* useful functions */
size_t size(Node *n);
void breakhere();
