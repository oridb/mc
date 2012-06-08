#define MaxArg 4

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
    Locpseu, /* pseudo-reg */
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

typedef struct Insn Insn;
typedef struct Loc Loc;
typedef struct Func Func;
typedef struct Blob Blob;
typedef struct Isel Isel;

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
        Reg   reg;
        long  pseudo;
        long  lit;
        /* disp(base + index) */
        struct {
            /* only one of lbldisp and constdisp may be used */
            char *lbldisp;
            long constdisp;
            int scale;
            Reg base;
            Reg idx;
        } mem;
    };
};

struct Insn {
    AsmOp op;
    Loc args[MaxArg];
    int narg;
};

struct Func {
    char *name;
    int   isglobl;
    size_t stksz;
    Htab *locs;
    Node *ret;
    Node **nl;
    size_t nn;
};

/* instruction selection state */
struct Isel {
    Insn **il;
    size_t ni;
    Node *ret;
    Htab *locs; /* decl id => int stkoff */
    Htab *globls; /* decl id => char *globlname */

    /* 6 general purpose regs */
    int rtaken[Nreg];
};

/* entry points */
void genasm(FILE *fd, Func *fn, Htab *globls);
void gen(Node *file, char *out);

/* location generation */
Loc *loclbl(Loc *l, Node *lbl);
Loc *locstrlbl(Loc *l, char *lbl);
Loc *locreg(Loc *l, Reg r);
Loc *locmem(Loc *l, long disp, Reg base, Reg idx, Mode mode);
Loc *locmeml(Loc *l, char *disp, Reg base, Reg idx, Mode mode);
Loc *locmems(Loc *l, long disp, Reg base, Reg idx, int scale, Mode mode);
Loc *locmemls(Loc *l, char *disp, Reg base, Reg idx, int scale, Mode mode);
Loc *loclit(Loc *l, long val);
void locprint(FILE *fd, Loc *l);
void iprintf(FILE *fd, Insn *insn);

/* register allocation */
Loc getreg(Isel *s, Mode m);
void freeloc(Isel *s, Loc l);
Loc claimreg(Isel *s, Reg r);
void freereg(Isel *s, Reg r);
extern const char *regnames[];
extern const Mode regmodes[];


/* useful functions */
size_t size(Node *n);
void breakhere();
