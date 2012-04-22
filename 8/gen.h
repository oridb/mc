typedef struct Comp Comp;
typedef struct Blob Blob;
typedef struct Fn   Fn;
typedef struct Bb   Bb;

struct Comp {
    /* we generate blobs and functions */
    Blob **globl;
    size_t nglobl;
    Fn **func;
    size_t nfunc;
};

struct Blob {
    char *name; /* mangled asm name */
    void *data;
    size_t ndata;
};

struct Fn {
    char *name; /* assembly name; mangled */
    
    Htab *bbnames; /* char* => Bb* map */
    Bb *start;
    Bb *end;
    Bb *cur;
    Bb **bb;
    size_t nbb;

    /* we can't know all the edges as we
     * construct the bb list, so we fix up later */
    Bb **fixup;
    size_t nfixup;
};

struct Bb {
    int id;

    /* nodes in bb */
    Node **n;
    size_t nn;

    /* edges */
    Bb **in;
    size_t nin;
    Bb *out;
    size_t nout;
};

/* toplevel code gen */
Comp *mkcomp(Node *f);
void gen(Node *file, char *out);
void assemble(char *asmfile, char *out);

/* cfg */
Fn *mkfn(char *name);
Bb *mkbb(void);
void edge(Bb *from, Bb *to);
