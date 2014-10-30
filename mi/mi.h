typedef struct Cfg Cfg;
typedef struct Bb Bb;

struct  Cfg {
    Node *fn;
    Bb **bb;
    Bb *start;
    Bb *end;
    size_t nbb;

    /* for building bb */
    int nextbbid;
    Htab *lblmap; /* label => Bb mapping */
    Node **fixjmp;
    size_t nfixjmp;
    Bb **fixblk;
    size_t nfixblk;
};

struct Bb {
    int id;
    char **lbls;
    size_t nlbls;
    Node **nl;
    size_t nnl;
    Bitset *pred;
    Bitset *succ;
};

/* expression folding */
Node *fold(Node *n, int foldvar);
/* Takes a reduced block, and returns a flow graph. */
Cfg *mkcfg(Node *fn, Node **nl, size_t nn);
void dumpcfg(Cfg *c, FILE *fd);
void check(Cfg *cfg);

/* pattern matching */
Node *gensimpmatch(Node *m);
