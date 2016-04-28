#define Maxarg 4                /* maximum number of args an insn can have */
#define Wordsz 4                /* the size of a "natural int" */
#define Ptrsz 8                 /* the size of a machine word (ie, pointer size) */

typedef struct Blob Blob;

typedef enum {
	Bti8,
	Bti16,
	Bti32,
	Bti64,
	Btimin,     /* byte-packed uint */
	Btref,
	Btbytes,
	Btseq,
	Btpad,
} Blobtype;

typedef enum {
	Mbyte,
	Mhalf,
	Mword,
	Mlong,
	Msingle,
	Mdouble,
	Mmem,
} Mode;

struct Blob {
	Blobtype type;
	size_t align;
	char *lbl;  /* may be null */
	char isglobl;
	char iscomdat;
	union {
		uint64_t npad;
		uint64_t ival;
		struct {
			char *str;
			char isextern;
			size_t off;
		} ref;
		struct {
			size_t len;
			char *buf;
		} bytes;
		struct {
			size_t nsub;
			Blob **sub;
		} seq;
	};
};


void gen(Node *file, char *out);

/* type info stuff */
size_t tysize(Type *t);
size_t tyalign(Type *t);
size_t alignto(size_t sz, Type *t);
size_t size(Node *n);
ssize_t tyoffset(Type *ty, Node *memb);
ssize_t offset(Node *aggr, Node *memb);

/* blob stuff */
Blob *mkblobpad(size_t sz);
Blob *mkblobi(Blobtype type, uint64_t ival);
Blob *mkblobbytes(char *buf, size_t len);
Blob *mkblobseq(Blob **sub, size_t nsub);
Blob *mkblobref(char *lbl, size_t off, int isextern);
void blobfree(Blob *b);

Blob *tydescblob(Type *t);
Blob *litblob(Htab *globls, Htab *strtab, Node *n);
size_t blobsz(Blob *b);
char *tydescid(char *buf, size_t bufsz, Type *ty);
