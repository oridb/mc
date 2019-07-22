typedef struct Cfg Cfg;
typedef struct Bb Bb;
typedef struct Reaching Reaching;

struct Cfg {
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

struct Reaching {
	Bitset **in;
	Bitset **out;
	size_t **defs;
	size_t *ndefs;
	size_t nbb;
};

/* dataflow analysis */
Reaching *reaching(Cfg *cfg);
void reachingfree(Reaching *r);

Node *assignee(Node *n);

/* Takes a reduced block, and returns a flow graph. */
Cfg *mkcfg(Node *fn, Node **nl, size_t nn);
void dumpcfg(Cfg *c, FILE *fd);
void check(Cfg *cfg);

/* pattern matching */
void genmatch(Node *m, Node *val, Node ***out, size_t *nout);
void genonematch(Node *pat, Node *val, Node *iftrue, Node *iffalse, Node ***out, size_t *nout, Node ***cap, size_t *ncap);

/* tree flattening */
Node *flattenfn(Node *dcl);
int isconstfn(Node *n);
