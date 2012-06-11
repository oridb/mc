typedef struct Cfg Cfg;
typedef struct Bb Bb;

struct  Cfg {
    Bb **bb;
    size_t nbb;

    /* for building bb */
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
    Bitset *in;
    Bitset *out;
};

/* Takes a reduced block, and returns a flow graph. */
Cfg *mkcfg(Node **nl, int nn);
void flow(Cfg *cfg);
