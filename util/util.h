#ifdef __GNUC__
#define FATAL __attribute__((noreturn))
#else
#define FATAL
#endif

typedef uint8_t byte;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long vlong;
typedef unsigned long long uvlong;

typedef struct Htab Htab;
typedef struct Bitset Bitset;
typedef struct Optctx Optctx;
typedef struct Str Str;
typedef struct Strbuf Strbuf;

struct Htab {
	size_t nelt;
	size_t ndead;
	size_t sz;
	ulong (*hash)(void *k);
	int (*cmp)(void *a, void *b);
	void **keys;
	void **vals;
	ulong *hashes;
	char *dead;
};

struct Bitset {
	size_t nchunks;
	size_t *chunks;
};

struct Optctx {
	/* public exports */
	char *optarg;
	char **args;
	size_t nargs;

	/* internal state */
	char *optstr;
	char **optargs;
	size_t noptargs;
	size_t argidx;
	int optdone; /* seen -- */
	int finished;
	char *curarg;
};

struct Str {
	size_t len;
	char *buf;
};

struct Strbuf {
	char *buf;
	size_t sz;
	size_t len;
};

/* string buffer */
Strbuf *mksb();
char *sbfin(Strbuf *sb);
void sbputs(Strbuf *sb, char *s);
void sbputb(Strbuf *sb, char b);

/* bit sets */
ulong bshash(Bitset *bs);
Bitset *mkbs(void);
void bsfree(Bitset *bs);
Bitset *bsdup(Bitset *bs);
Bitset *bsclear(Bitset *bs);
void delbs(Bitset *bs);
void bsput(Bitset *bs, size_t elt);
void bsdel(Bitset *bs, size_t elt);
void bsunion(Bitset *a, Bitset *b);
void bsintersect(Bitset *a, Bitset *b);
void bsdiff(Bitset *a, Bitset *b);
int bseq(Bitset *a, Bitset *b);
int bsissubset(Bitset *set, Bitset *sub);
int bsisempty(Bitset *set);
int bsiter(Bitset *bs, size_t *elt);
size_t bsmax(Bitset *bs);
size_t bscount(Bitset *bs);

/* inline for speed */
static inline int bshas(Bitset *bs, size_t elt)
{
	size_t eltidx, eltshift;

	if (elt >= bs->nchunks * 8 * sizeof(size_t))
		return 0;
	eltidx = elt / (8 * sizeof(size_t));
	eltshift = elt % (8 * sizeof(size_t));
	return (bs->chunks[eltidx] & (1ULL << eltshift)) != 0;
}


/* hash tables */
Htab *mkht(ulong (*hash)(void *key), int (*cmp)(void *k1, void *k2));
void htfree(Htab *ht);
int htput(Htab *ht, void *k, void *v);
void htdel(Htab *ht, void *k);
void *htget(Htab *ht, void *k);
int hthas(Htab *ht, void *k);
void **htkeys(Htab *ht, size_t *nkeys);
ulong strhash(void *key);
int streq(void *a, void *b);
ulong strlithash(void *key);
int strliteq(void *a, void *b);
ulong ptrhash(void *key);
int ptreq(void *a, void *b);
ulong inthash(uint64_t key);
int inteq(uint64_t a, uint64_t b);
ulong murmurhash2(void *buf, size_t sz);

/* util functions */
void *zalloc(size_t size);
void *xalloc(size_t size);
void *zrealloc(void *p, size_t oldsz, size_t size);
void *xrealloc(void *p, size_t size);
void die(char *msg, ...) FATAL;
char *strdupn(char *s, size_t len);
char *xstrdup(char *s);
char *strjoin(char *u, char *v);
void *memdup(void *mem, size_t len);
size_t bprintf(char *buf, size_t len, char *fmt, ...);

/* indented printf */
void indentf(int depth, char *fmt, ...);
void findentf(FILE *fd, int depth, char *fmt, ...);
void vfindentf(FILE *fd, int depth, char *fmt, va_list ap);

/* serializing/unserializing */
void be64(vlong v, byte buf[8]);
vlong host64(byte buf[8]);
void be32(long v, byte buf[4]);
long host32(byte buf[4]);
static inline intptr_t ptoi(void *p) { return (intptr_t)p; }
static inline void *itop(intptr_t i) { return (void *)i; }

void wrbuf(FILE *fd, void *buf, size_t sz);
void rdbuf(FILE *fd, void *buf, size_t sz);
char rdbyte(FILE *fd);
void wrbyte(FILE *fd, char val);
char rdbyte(FILE *fd);
void wrint(FILE *fd, long val);
long rdint(FILE *fd);
void wrstr(FILE *fd, char *val);
char *rdstr(FILE *fd);
void wrlenstr(FILE *fd, Str str);
void rdlenstr(FILE *fd, Str *str);
void wrflt(FILE *fd, double val);
double rdflt(FILE *fd);
void wrbool(FILE *fd, int val);
int rdbool(FILE *fd);

size_t max(size_t a, size_t b);
size_t min(size_t a, size_t b);
size_t align(size_t sz, size_t a);

/* list funcs */
/* hack; nl is void* b/c void*** is incompatible with T*** */
void lappend( void *l, size_t *len, void *n);
void lcat(void *dst, size_t *ndst, void *src, size_t nsrc);
void linsert(void *l, size_t *len, size_t idx, void *n);
void *lpop(void *l, size_t *len);
void ldel(void *l, size_t *len, size_t idx);
void lfree(void *l, size_t *len);

/* suffix replacement */
char *swapsuffix(char *buf, size_t sz, char *s, char *suf, char *swap);

