typedef struct Cfg Cfg;
typedef struct Bb Bb;
typedef struct Reaching Reaching;
typedef struct Dtree Dtree;

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

struct Dtree {
	int id;
	Srcloc loc;

	/* values for matching */
	Node *lbl;
	Node *load;
	size_t nconstructors;
	char accept;
	char emitted;

	/* the decision tree step */
	Node **pat;
	size_t npat;
	Dtree **next;
	size_t nnext;
	Dtree *any;

	/* This is only used in the MATCH node (accept == 1)
	 * It generates assignments for the captured variables
	 * before jumping into the block of a match arm .*/
	Node **cap;
	size_t ncap;

	size_t refcnt;
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
