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
    Loclbl,
    Locreg,
    Locmem,
    Locmeml, /* label offset */
    Loclit,
} LocType;

typedef enum {
    ModeNone,
    ModeB,
    ModeS,
    ModeL,
    ModeQ,
    Nmode
} Mode;

typedef struct Insn Insn;
typedef struct Loc Loc;
typedef struct Func Func;
typedef struct Blob Blob;

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
        long  lit;
        /* disp(base + index) */
        struct {
            /* only one of lbldisp and constdisp may be used */
            char *lbldisp;
            long constdisp;
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

/* entry points */
void genasm(FILE *fd, Func *fn, Htab *globls);
void gen(Node *file, char *out);

/* location generation */
Loc *loclbl(Loc *l, Node *lbl);
Loc *locreg(Loc *l, Reg r);
Loc *locmem(Loc *l, long disp, Reg base, Reg idx, Mode mode);
Loc *locmeml(Loc *l, char *disp, Reg base, Reg idx, Mode mode);
Loc *loclit(Loc *l, long val);

/* useful functions */
size_t size(Node *n);
