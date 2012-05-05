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
} Reg;


typedef enum {
    Loclbl,
    Locpseudo,
    Locreg,
    Locmem,
    Locmeml, /* label offset */
    Loclit,
} LocType;

typedef enum {
    ModeB,
    ModeS,
    ModeL,
    ModeQ,
} Mode;

typedef struct Insn Insn;
typedef struct Loc Loc;

struct Loc {
    LocType type;
    Mode mode;
    union {
        char *lbl;
        Reg   reg;
        long  lit;
        long  pseudo;
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

void genasm(Node **nl, int nn);
