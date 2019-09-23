#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "parse.h"
#include "mi.h"
#include "dtree.h"

#define V1
/**
 * TODO
 * Recursively check the types of the pattern and the subject tree.
 *
 */

File file;
char debugopt[128];

typedef struct Dtree Dtree;
extern Dtree *gendtree(Node *m, Node *val, Node **lbl, size_t nlbl, int startid);
extern Dtree *gendtree2(Node *m, Node *val, Node **lbl, size_t nlbl, int startid);
extern void dtreedump(FILE *fd, Dtree *dt);


static char *
errorf(char *fmt, ...)
{
	va_list ap;
	char *p;
	size_t n;

	p = malloc(2048);
	if (p == NULL) {
		return "(NULL)";
	}
	va_start(ap, fmt);
	n = vsnprintf(p, 2048, fmt, ap);
	va_end(ap);
	if (n > 0) {
		p = realloc(p, n+1);
	} else {
		free(p);
		p = "(NULL)";
	}
	return p;
}

static Type *
installucons(Type *t)
{
	Type *b;
	size_t i;

	if (!t)
		return NULL;
	b = tybase(t);
	switch (b->type) {
	case Tystruct:
		for (i = 0; i < b->nmemb; i++)
			installucons(b->sdecls[i]->decl.type);
		break;
	case Tyunion:
		for (i = 0; i < b->nmemb; i++) {
			if (!b->udecls[i]->utype)
				b->udecls[i]->utype = b;
			b->udecls[i]->id = i;
		}
		break;
	default:
		break;
	}
	return t;
}

static Node *
_m(Node *p)
{
	Node *b;

	b = mkblock(Zloc, mkstab(0));
	b->block.stmts = NULL;
	b->block.nstmts = 0;

	return mkmatch(Zloc, p, b);
}

static Node *
ty(Node *n, Type *t)
{
	switch (n->type) {
	case Nlit:
		n->lit.type = t;
		break;
	case Nexpr:
		n->expr.type = t;
	default:
		;
	}
	return n;
}

static char *
exprequal(Node *a, Node *b, int depth)
{
	size_t i;
	char *err;

	if (a == NULL && b == NULL) {
		return NULL;
	}
	if (a == NULL || b == NULL) {
		return "either one is null";
	}

	if (false && a->nid != b->nid) {
		return errorf("nid is not matched. (%d) want %d, got %d", depth, a->nid, b->nid);
	}

	if (a->type != b->type) {
		return errorf("ntype is not matched. (%d) want %s, got %s", depth, nodestr[a->type], nodestr[b->type]);
	}

	switch (a->type) {
	case Nexpr:
		if (exprop(a) != exprop(b)) {
			return errorf("op is not matched. (%d) want %s, got %s", depth, opstr[exprop(a)], opstr[exprop(b)]);
		}

		if (a->expr.nargs != b->expr.nargs) {
			fprintf(stderr, "op:%s\n", opstr[exprop(a)]);
			return errorf("nargs is not matched. (%d) want %d, got %d", depth, a->expr.nargs, b->expr.nargs);
		}

		for (i = 0; i < a->expr.nargs; i++) {
			err = exprequal(a->expr.args[i], b->expr.args[i], depth+1);
			if (err != NULL) {
				return err;
			}
		}
		break;
	case Nlit:
		switch (a->lit.littype) {
		case Lint:
			if (a->lit.intval != b->lit.intval) {
				return errorf("lit.intval is not matched. (%d) want %d, got %d", depth, a->lit.intval, b->lit.intval);
			}
			break;
		default:
		return errorf("unexpected littype: %s", nodestr[a->type]);
		}
		break;
	case Nname:
		break;
	default:
		return errorf("unexpected ntype: %s", nodestr[a->type]);
	}

	return NULL;
}

static char *
dtequal(Dtree *a, Dtree *b, int depth)
{
	size_t i;
	char *err;

	if (a == NULL && b == NULL) {
		return NULL;
	}
	if (a == NULL || b == NULL) {
		return "either one is null";
	}

	if (a->id != b->id) {
		return errorf("id is not matched. (depth:%d) want %d, got %d", depth, a->id, b->id);
	}
	if (a->nconstructors != b->nconstructors) {
		return errorf("nconstructors is not matched. (depth:%d id:%d) want %ld, got %ld", depth, a->id, a->nconstructors, b->nconstructors);
	}
	if (a->accept != b->accept) {
		return errorf("accept is not matched. (depth:%d id:%d) want %ld, got %ld", depth, a->id, a->accept, b->accept);
	}
	if (a->emitted != b->emitted) {
		return errorf("emitted is not matched. (depth:%d id:%d) want %ld, got %ld", depth, a->id, a->emitted, b->emitted);
	}
	if (a->ptrwalk != b->ptrwalk) {
		return errorf("ptrwalk is not matched. (depth:%d id:%d) want %ld, got %ld", depth, a->id, a->ptrwalk, b->ptrwalk);
	}

	err = exprequal(a->load, b->load, 0);
	if (err != NULL) {
		fprintf(stderr, "WANT:\n");
		dumpn(a->load, stderr);
		fprintf(stderr, "GOT:\n");
		dumpn(b->load, stderr);
		return errorf("load is not matched. (depth:%d id:%d) want %p, got %p, %s", depth, a->id, a->load, b->load, err);
	}

	if (a->nnext != b->nnext) {
		return errorf("nnext is not matched. (depth:%d id:%d) want %d, got %d", depth, a->id, a->nnext, b->nnext);
	}
	if (a->npat != b->npat) {
		return errorf("npat is not matched. (depth:%d id:%d) want %d, got %d", depth, a->id, a->npat, b->npat);
	}
	if (a->ncap != b->ncap) {
		return errorf("ncap is not matched. (depth:%d id:%d) want %d, got %d", depth, a->id, a->ncap, b->ncap);
	}

	for (i = 0; i < a->npat; i++) {
		err = exprequal(a->pat[i], b->pat[i], 0);
		if (err != NULL) {
			fprintf(stderr, "WANT:\n");
			dumpn(a->pat[i], stderr);
			fprintf(stderr, "GOT:\n");
			dumpn(b->pat[i], stderr);
			return errorf("pat is not matched. (depth:%d id:%d) want %p, got %p, %s", depth, a->id, a->pat[i], b->pat[i], err);
		}
	}
	for (i = 0; i < a->nnext; i++) {
		err = dtequal(a->next[i], b->next[i], depth+1);
		if (err != NULL) {
			return errorf("next[%d] is not matched. (depth:%d id:%d) want %p, got %p, %s", i, depth, a->id, a->next[i], b->next[i], err);
		}
	}
	err = dtequal(a->any, b->any, depth+1);
	if (err != NULL) {
		return errorf("any is not matched. (depth:%d id:%d) want %p, got %p, %s", depth, a->id, a->any, b->any, err);
	}

	for (i = 0; i < a->ncap; i++) {
		err = exprequal(a->cap[i], b->cap[i], 0);
		if (err != NULL) {
			return err;
		}
	}

	return NULL;
}



static char *
test_match(int idx, Node *val, Node **pat, Dtree *want)
{
	Dtree *dt;
	Node *m, *v, *r, **lbl, **matches;
	size_t i, nlbl, npat, nmatches;
	char *err;

	matches = NULL;
	nmatches = 0;
	for (npat = 0; pat[npat] != NULL; npat++) {
		lappend(&matches, &nmatches, _m(pat[npat]));
	}

	m = mkmatchstmt(Zloc, val, matches, nmatches);
	r = val;
	v = gentemp(r->loc, r->expr.type, NULL);


	if (getenv("D")) {
		fprintf(stderr, "[%.3d]>\n", idx);
		dumpn(m, stdout);
	}

	if (1) {
		lbl = NULL;
		nlbl = 0;
		for (i = 0; i < nmatches; i++) {
			lappend(&lbl, &nlbl, genlbl(pat[i]->match.block->loc));
		}

		dt = gendtree2(m, v, lbl, nlbl, 0);
		if (getenv("d")) {
			fprintf(stderr, "dtree >>\n");
			dtreedump(stderr, dt);
		}

		err = dtequal(want, dt, 0);
		if (err) {
			return err;
		}
	}

	if (getenv("G")) {
		Node **matches = NULL;
		size_t nmatches = 0;
		genmatch(m, v, &matches, &nmatches);

		fprintf(stdout, "===========\n");
		for (i = 0; i < nmatches; i++) {
			dumpn(matches[i], stdout);
		}
	}

	return NULL;
}

typedef struct Test {
	char *name;
	char *desc;
	Node *val;
	Node **pats;
	Dtree *dt;
} Test;

int
main(int argc, char **argv)
{
	size_t i;
	char *err;
	Node *t, *p_, *p0, *p1, *p2;

#if 0
#define D() fprintf(stderr, "__%d__\n", __LINE__)
#else
#define D()
#endif

#define P(x) ({D();p##x;})

#define __P0(p) ({D();p0 = (p);})
#define __P1(p) ({D();p1 = (p);})
#define __P2(p) ({D();p2 = (p);})
#define __P_(p) ({D();p_ = (p);})


#define __T(v) ({t = (v);})
#define T ({t;})


	Type *_int32 = mktype(Zloc, Tyint32);
	Type *_int64 = mktype(Zloc, Tyint64);

	Type *_int32t1 = mktytuple(Zloc, (Type*[]){_int32}, 1);
	Type *_int32t2 = mktytuple(Zloc, (Type*[]){_int32, _int32}, 2);
	Type *_int32t3 = mktytuple(Zloc, (Type*[]){_int32, _int32, _int32}, 3);

	Type *_int32s1 = mktystruct(Zloc, (Node*[]){mkdecl(Zloc, mkname(Zloc, "foo"), _int32)}, 1);
	Type *_enum1 = installucons(mktyunion(Zloc, (Ucon*[]){mkucon(Zloc, mkname(Zloc, "Foo"), NULL, NULL)}, 1));
	Type *_enum2 = installucons(mktyunion(Zloc,
					      (Ucon*[]){
						      mkucon(Zloc, mkname(Zloc, "Foo"), NULL, NULL),
						      mkucon(Zloc, mkname(Zloc, "Bar"), NULL, NULL)},
						      2));
	Type *_enum3 = installucons(mktyunion(Zloc,
					      (Ucon*[]){
						      mkucon(Zloc, mkname(Zloc, "Foo"), NULL, NULL),
						      mkucon(Zloc, mkname(Zloc, "Bar"), NULL, NULL),
						      mkucon(Zloc, mkname(Zloc, "Baz"), NULL, NULL)},
						      3));

	Type *_int32u1 = mktyunion(Zloc, (Ucon*[]){
		mkucon(Zloc, mkname(Zloc, "Foo"), NULL, _int32t1),
	}, 1);

	Type *_int32a1 = mktyslice(Zloc, _int32);


	Type *_bug001u = installucons(mktyunion(Zloc, (Ucon*[]){
			mkucon(Zloc, mkname(Zloc, "Foo"), NULL, _int32t1),
			mkucon(Zloc, mkname(Zloc, "Bar"), NULL, NULL)
		}, 2));
	Type *_bug001s = mktystruct(Zloc, (Node*[]){
		mkdecl(Zloc, mkname(Zloc, "s1_slice"), _int32a1),
		mkdecl(Zloc, mkname(Zloc, "s1_int32"), _int32),
	}, 2);

	Type *_bug002s = mktystruct(Zloc, (Node*[]){
		mkdecl(Zloc, mkname(Zloc, "s2_ufoo"), _bug001u),
		mkdecl(Zloc, mkname(Zloc, "s2_sbar"), _bug001s),
		mkdecl(Zloc, mkname(Zloc, "s2_int32"), _int32),
	}, 3);

	(void)_bug002s;



	Dtree *dt11 = &(Dtree){
		.id = 11,
		.any = NULL,
	};
	Dtree *dt8 = &(Dtree){
		.id = 8,
		.any =dt11,
	};
	dt11->any = dt8;

	Test tests[] = {
		{
			.name = "int32 matched by 1 wildcard",
			.val  = gentemp(Zloc, _int32, NULL),
			.pats = (Node*[]){
				ty(mkexpr(Zloc, Ogap, NULL), _int32),
				NULL,
			},
			.dt = &(Dtree){
				.id = 0,
				.accept = 1,
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0,
			//	.load = NULL,
			//	.nnext = 0,
			//	.npat = 0,
			//	.any = &(Dtree){
			//		.id = 1,
			//		.accept = 1,
			//	},
			//}
		},
		{
			.name = "int32 matched by 1 capture variable",
			.val = __T(gentemp(Zloc, _int32, NULL)),
			.pats = (Node*[]){
				__P0(ty(mkexpr(Zloc, Ovar, mkname(Zloc, "foovar"), NULL), _int32)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 0,
				.accept = 1,
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0,
			//	.load = NULL,
			//	.nnext = 0,
			//	.npat = 0,
			//	.any = &(Dtree){
			//		.id = 1,
			//		.accept = 1,
			//	},
			//},
		},
		{
			.name = "int32 matched by 1 literals",
			.val = gentemp(Zloc, _int32, NULL),
			.pats = (Node*[]){
				__P0(ty(mkexpr(Zloc, Olit, mkint(Zloc, 123), NULL), _int32)),
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _int32)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 2,
				.load = gentemp(Zloc, _int32, NULL),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					[0] = p0,
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
				},
				.any = &(Dtree){
					.id = 1,
					.accept = 1,
				},
			},

//			.dt = &(Dtree){
//				.id = 0,
//				.nconstructors = 0x100000000,
//				.load = gentemp(Zloc, _int32, NULL),
//				.nnext = 1,
//				.npat = 1,
//				.pat = (Node*[]){
//					[0] = p0,//t2p0,
//				},
//				.next = (Dtree*[]){
//					[0] = &(Dtree){
//						.id = 1,
//						.accept = 1,
//					},
//				},
//				.any = &(Dtree){
//					.id = 2,
//					.accept = 1,
//				},
//			}
		},
		{
			.name = "int32 matched by 2 literals",
			.val = gentemp(Zloc, _int32, NULL),
			.pats = (Node*[]){
				__P0(ty(mkexpr(Zloc, Olit, mkint(Zloc, 123), NULL), _int32)),
				__P1(ty(mkexpr(Zloc, Olit, mkint(Zloc, 456), NULL), _int32)),
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _int32)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 3,
				.load = gentemp(Zloc, _int32, NULL),
				.npat = 2,
				.nnext = 2,
				.pat = (Node*[]){
					[0] = P(0),
					[1] = P(1),
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
					[1] = &(Dtree){
						.id = 1,
						.accept = 1,
					},
				},
				.any = &(Dtree){
					.id = 2,
					.accept = 1,
				},
			},


			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0x100000000,
			//	.load = gentemp(Zloc, _int32, NULL),
			//	.nnext = 2,
			//	.npat = 2,
			//	.pat = (Node*[]){
			//		[0] = P(0),
			//		[1] = P(1),
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 1,
			//			.accept = 1,
			//		},
			//		[1] = &(Dtree){
			//			.id = 2,
			//			.accept = 1,
			//		},
			//	},
			//	.any = &(Dtree){
			//		.id = 3,
			//		.accept = 1,
			//	},
			//}
		},
		{
			.name = "1-tuple, matched by wildcard only",
			.val = gentemp(Zloc, _int32t1, NULL),
			.pats = (Node*[]){
				ty(mkexpr(Zloc, Otup, ty(mkexpr(Zloc, Ogap, NULL), _int32), NULL), _int32t1),
				NULL,
			},
			.dt = &(Dtree){
				.id = 0,
				.accept = 1,
				.nnext = 0,
				.npat = 0,
				.any = NULL,
			},


			//.dt = &(Dtree){
			//	.id = 0,
			//	/*
			//	 * We compute the .nconstructors and .load only for literal and union patterns.
			//	 */
			//	.nconstructors = 0,
			//	.load = NULL,
			//	.nnext = 0,
			//	.npat = 0,
			//	.pat = (Node*[]){
			//	},
			//	.next = (Dtree*[]){
			//	},
			//	.any = &(Dtree){
			//		.id = 1,
			//		.accept = 1,
			//	},
			//},
		},
		{
			.name = "1-tuple",
			.val = ({t = gentemp(Zloc, _int32t1, NULL);}),
			.pats = (Node *[]){
				__P0(ty(mkexpr(Zloc, Otup, ty(mkexpr(Zloc, Olit, mkint(Zloc, 123), NULL), _int32), NULL), _int32t1)),
				__P1(ty(mkexpr(Zloc, Ogap, NULL), _int32t1)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 2,
				.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 0), NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat =  (Node*[]){
					P(0)->expr.args[0],
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
				},
				.any = &(Dtree){
					.id = 1,
					.accept = 1
				},
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	/*
			//	 * We compute the .nconstructors and .load only for literal and union patterns.
			//	 */
			//	.nconstructors = 0x100000000,
			//	.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 0), NULL), _int32),
			//	.nnext = 1,
			//	.npat = 1,
			//	.pat = (Node*[]){
			//		P(0)->expr.args[0],
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 1,
			//			.accept = 1,
			//		},
			//	},
			//	.any = &(Dtree){
			//		.id = 2,
			//		.accept = 1,
			//	},
			//},
		},
		{
			.name = "2-tuple",
			.val = ({t = gentemp(Zloc, _int32t2, NULL);}),
			.pats = (Node *[]){
				/**
				 * | (123, 456):
				 */
				__P0(ty(mkexpr(Zloc, Otup,
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 123), NULL), _int32),
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 456), NULL), _int32),
					     NULL), _int32t2)),
				/**
				 * | (_, _):
				 */
				__P1(ty(mkexpr(Zloc, Ogap, NULL), _int32t2)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 4,
				.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 0), NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					P(0)->expr.args[0],
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 2,
						.load =ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 1), NULL), _int32),
						.nnext = 1,
						.npat = 1,
						.pat = (Node*[]){
							P(0)->expr.args[1],
						},
						.next = (Dtree*[]){
							[0] = &(Dtree){
								.id = 0,
								.accept = 1,
							},
						},
						.any = &(Dtree){
							.id = 1,
							.accept = 1,
						},
					},
				},
				.any = &(Dtree){
					.id = 3,
					.accept = 1,
				}
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0x100000000,
			//	.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 0), NULL), _int32),
			//	.nnext = 1,
			//	.npat = 1,
			//	.pat = (Node*[]){
			//		P(0)->expr.args[0],
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 2,
			//			.nconstructors = 0x100000000,
			//			.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 1), NULL), _int32),
			//			.nnext = 1,
			//			.npat = 1,
			//			.pat = (Node*[]){
			//				P(0)->expr.args[1],
			//			},
			//			.next = (Dtree*[]){
			//				[0] = &(Dtree){
			//					.id = 1,
			//					.accept = 1,
			//				},
			//			},
			//			.any = &(Dtree){
			//				.id = 3,
			//				.accept = 1,
			//			},
			//		},
			//	},
			//	.any = &(Dtree){
			//		.id = 4,
			//		.any = &(Dtree){
			//			.id = 3,
			//			.accept = 1,
			//		},
			//	},
			//},
		},
		{
			.name = "3-tuple",
			.val = ({t = gentemp(Zloc, _int32t3, NULL);}),
			.pats = (Node *[]){
				/**
				 * | (123, 456):
				 */
				__P0(ty(mkexpr(Zloc, Otup,
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 123), NULL), _int32),
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 456), NULL), _int32),
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 789), NULL), _int32),
					     NULL), _int32t3)),
				/**
				 * | (_, _):
				 */
				__P1(ty(mkexpr(Zloc, Ogap, NULL), _int32t3)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 6,
				.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 0), NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					P(0)->expr.args[0],
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 4,
						.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 1), NULL), _int32),
						.nnext = 1,
						.npat = 1,
						.pat = (Node*[]){
							P(0)->expr.args[1],
						},
						.next = (Dtree*[]){
							[0] = &(Dtree){
								.id = 2,
								.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 2), NULL), _int32),
								.nnext = 1,
								.npat = 1,
								.pat = (Node*[]){
									P(0)->expr.args[2],
								},
								.next = (Dtree*[]){
									[0] = &(Dtree){
										.id = 0,
										.accept = 1,
									}
								},
								.any = &(Dtree){
									.id = 1,
									.accept = 1,
								},
							},
						},
						.any = &(Dtree){
							.id = 3,
							.accept = 1,
						},
					},
				},
				.any = &(Dtree){
					.id = 5,
					.accept =1,
				},
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0x100000000,
			//	.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 0), NULL), _int32),
			//	.nnext = 1,
			//	.npat = 1,
			//	.pat = (Node*[]){
			//		P(0)->expr.args[0],
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 2,
			//			.nconstructors = 0x100000000,
			//			.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 1), NULL), _int32),
			//			.nnext = 1,
			//			.npat = 1,
			//			.pat = (Node*[]){
			//				P(0)->expr.args[1],
			//			},
			//			.next = (Dtree*[]){
			//				[0] = &(Dtree){
			//					.id = 3,
			//					.nconstructors = 0x100000000,
			//					.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 2), NULL), _int32),
			//					.nnext = 1,
			//					.npat = 1,
			//					.pat = (Node*[]){
			//						P(0)->expr.args[2],
			//					},
			//					.next =  (Dtree*[]){
			//						[0] = &(Dtree){
			//							.id = 1,
			//							.accept = 1,
			//						},
			//					},
			//					.any = &(Dtree){
			//						.id = 4,
			//						.accept = 1,
			//					},
			//				},
			//			},
			//			.any = &(Dtree){
			//				.id = 6,
			//				.any = &(Dtree){
			//					.id = 4,
			//					.accept = 1,
			//				},
			//			},
			//		},
			//	},
			//	.any = &(Dtree){
			//		.id = 5,
			//		.any = &(Dtree){
			//			.id = 6,
			//			.any = &(Dtree){
			//				.id = 4,
			//				.accept = 1,
			//			},
			//		},
			//	},
			//},
		},
		{
			.name = "3-tuple-3-pat",
			.val = ({t = gentemp(Zloc, _int32t3, NULL);}),
			.pats = (Node *[]){
				/**
				 * | (123, 456):
				 */
				__P0(ty(mkexpr(Zloc, Otup,
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 101), NULL), _int32),
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 102), NULL), _int32),
					     ty(mkexpr(Zloc, Olit, mkint(Zloc, 103), NULL), _int32),
					     NULL), _int32t3)),
				__P1(ty(mkexpr(Zloc, Otup,
					       ty(mkexpr(Zloc, Olit, mkint(Zloc, 201), NULL), _int32),
					       ty(mkexpr(Zloc, Olit, mkint(Zloc, 202), NULL), _int32),
					       ty(mkexpr(Zloc, Olit, mkint(Zloc, 203), NULL), _int32),
					       NULL), _int32t3)),
				__P2(ty(mkexpr(Zloc, Otup,
					       ty(mkexpr(Zloc, Olit, mkint(Zloc, 301), NULL), _int32),
					       ty(mkexpr(Zloc, Olit, mkint(Zloc, 302), NULL), _int32),
					       ty(mkexpr(Zloc, Olit, mkint(Zloc, 303), NULL), _int32),
					       NULL), _int32t3)),

				/**
				 * | (_, _):
				 */
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _int32t3)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 16,
				.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 0), NULL), _int32),
				.nnext = 3,
				.npat = 3,
				.pat = (Node*[]){
					P(0)->expr.args[0],
					P(1)->expr.args[0],
					P(2)->expr.args[0],
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 4,
						.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 1), NULL), _int32),
						.nnext = 1,
						.npat = 1,
						.pat = (Node*[]){
							P(0)->expr.args[1],
						},
						.next = (Dtree*[]){
							[0] = &(Dtree){
								.id = 2,
								.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 2), NULL), _int32),
								.nnext = 1,
								.npat = 1,
								.pat = (Node*[]){
									P(0)->expr.args[2],
								},
								.next = (Dtree*[]){
									[0] = &(Dtree){
										.id = 0,
										.accept = 1,
									},
								},
								.any = &(Dtree){
									.id = 1,
									.accept = 1,
								},
							},
						},
						.any = &(Dtree){
							.id = 3,
							.accept = 1,
						},
					},
					[1] = &(Dtree){
						.id = 9,
						.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 1), NULL), _int32),
						.nnext = 1,
						.npat = 1,
						.pat = (Node*[]){
							P(1)->expr.args[1],
						},
						.next = (Dtree*[]){
							[0] = &(Dtree){
								.id = 7,
								.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 2), NULL), _int32),
								.nnext = 1,
								.npat = 1,
								.pat = (Node*[]){
									P(1)->expr.args[2],
								},
								.next = (Dtree*[]){
									[0] = &(Dtree){
										.id = 5,
										.accept = 1,
									},
								},
								.any = &(Dtree){
									.id = 6,
									.accept = 1,
								},
							},
						},
						.any = &(Dtree){
							.id = 8,
							.accept = 1,
						},
					},
					[2] = &(Dtree){
						.id = 14,
						.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 1), NULL), _int32),
						.nnext = 1,
						.npat = 1,
						.pat = (Node*[]){
							P(2)->expr.args[1],
						},
						.next = (Dtree*[]){
							[0] = &(Dtree){
								.id = 12,
								.load = ty(mkexpr(Zloc, Otupget, T, mkintlit(Zloc, 2), NULL), _int32),
								.nnext = 1,
								.npat = 1,
								.pat = (Node*[]){
									P(2)->expr.args[2],
								},
								.next = (Dtree*[]){
									[0] = &(Dtree){
										.id = 10,
										.accept = 1,
									},
								},
								.any = &(Dtree){
									.id = 11,
									.accept = 1,
								},
							},
						},
						.any = &(Dtree){
							.id = 13,
							.accept = 1,
						},
					},
				},
				.any = &(Dtree){
					.id = 15,
					.accept = 1,
				},
			},
		},

		{
			.name = "1-int32-struct",
			.val = ({t = gentemp(Zloc, _int32s1, NULL);}),
			.pats = (Node*[]){
				__P0(ty(mkexprl(Zloc, Ostruct, (Node*[]){mkidxinit(Zloc, mkname(Zloc, "foo"), mkintlit(Zloc, 123))}, 1), _int32s1)),
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _int32s1)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 2,
				.nconstructors = 0,
				.load = ty(mkexpr(Zloc, Omemb, T, tybase(exprtype(P(0)))->sdecls[0]->decl.name, NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					P(0)->expr.args[0],
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
				},
				.any = &(Dtree){
					.id = 1,
					.accept = 1,
				},
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0,
			//	.load = ty(mkexpr(Zloc, Omemb, T, tybase(exprtype(P(0)))->sdecls[0]->decl.name, NULL), _int32),
			//	.nnext = 1,
			//	.npat = 1,
			//	.pat = (Node*[]){
			//		P(0)->expr.args[0],
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 1,
			//			.accept = 1,
			//		},
			//	},
			//	.any = &(Dtree){
			//		.id = 2,
			//		.accept = 1,
			//	},
			//},
		},
		{
			.name = "1-enum, matched by wildcard only",
			.val = gentemp(Zloc, _enum1, NULL),
			.pats = (Node*[]){
				__P_( ty(mkexpr(Zloc, Ogap, NULL), _enum1)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 0,
				.accept = 1,
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0,
			//	.load = NULL,
			//	.nnext = 0,
			//	.npat = 0,
			//	.pat = (Node*[]){
			//	},
			//	.next = (Dtree*[]){
			//	},
			//	.any = &(Dtree){
			//		.id = 1,
			//		.accept = 1,
			//	},
			//},
		},
		{
			.name = "1-enum, matched by a valid constructor",
			.val = ({t = gentemp(Zloc, _enum1, NULL);}),
			.pats = (Node*[]){
				__P0(ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Foo"), NULL), _enum1)),
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _enum1)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 2,
				.nconstructors = 0,
				.load = ty(mkexpr(Zloc, Outag, T, NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					/*
					 * the matcher will convert the Oucon expr to an Nlit for the Dtree
					 */
					ty(mkintlit(Zloc, finducon(_enum1, P(0)->expr.args[0])->id), _int32),
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
				},
				.any = &(Dtree){
					.id = 1,
					.accept = 1,
				},
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 1,
			//	.load = ty(mkexpr(Zloc, Outag, T, NULL), _int32),
			//	.nnext = 1,
			//	.npat = 1,
			//	.pat = (Node*[]){
			//		/*
			//		 * the matcher will convert the Oucon expr to an Nlit for the Dtree
			//		 */
			//		ty(mkintlit(Zloc, finducon(_enum1, P(0)->expr.args[0])->id), _int32),
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 1,
			//			.accept = 1,
			//		},
			//	},
			//	.any = &(Dtree){
			//		.id = 2,
			//		.accept = 1,
			//	},
			//},
		},
		/**
		 * match v : _enum2
		 * | `Foo
		 * | `Bar
		 * ;;
		 *
		 */
		{
			.name = "2-enum, matched by 2 valid constructors",
			.val = ({t = gentemp(Zloc, _enum2, NULL);}),
			.pats = (Node*[]) {
				__P0(ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Foo"), NULL), _enum2)),
				__P1(ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Bar"), NULL), _enum2)),
				//p_ = ty(mkexpr(Zloc, Ogap, NULL), _enum1),

				NULL,
			},
			.dt = &(Dtree){
				.id = 2,
				.load = ty(mkexpr(Zloc, Outag, T, NULL), _int32),
				.nnext = 2,
				.npat = 2,
				.pat = (Node*[]){
					ty(mkintlit(Zloc, finducon(_enum2, P(0)->expr.args[0])->id), _int32),
					ty(mkintlit(Zloc, finducon(_enum2, P(1)->expr.args[0])->id), _int32),
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
					[1] = &(Dtree){
						.id = 1,
						.accept = 1,
					},
				},
				.any = NULL,
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 2,
			//	.load = ty(mkexpr(Zloc, Outag, T, NULL), _int32),
			//	.nnext = 2,
			//	.npat = 2,
			//	.pat = (Node*[]){
			//		ty(mkintlit(Zloc, finducon(_enum2, P(0)->expr.args[0])->id), _int32),
			//		ty(mkintlit(Zloc, finducon(_enum2, P(1)->expr.args[0])->id), _int32),
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 1,
			//			.accept = 1,
			//		},
			//		[1] = &(Dtree){
			//			.id = 2,
			//			.accept = 1,
			//		},
			//	},
			//	.any = NULL,
			//},
		},
		{
			.name = "3-enum, matched by 3 valid constructors",
			.val = ({t = gentemp(Zloc, _enum3, NULL);}),
			.pats = (Node*[]) {
				__P0(ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Foo"), NULL), _enum3)),
				__P1(ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Bar"), NULL), _enum3)),
				__P2(ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Baz"), NULL), _enum3)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 3,
				.load = ty(mkexpr(Zloc, Outag, T, NULL), _int32),
				.nnext = 3,
				.npat = 3,
				.pat = (Node*[]){
					ty(mkintlit(Zloc, finducon(_enum3, P(0)->expr.args[0])->id), _int32),
					ty(mkintlit(Zloc, finducon(_enum3, P(1)->expr.args[0])->id), _int32),
					ty(mkintlit(Zloc, finducon(_enum3, P(2)->expr.args[0])->id), _int32),
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
					[1] = &(Dtree){
						.id = 1,
						.accept = 1,
					},
					[2] = &(Dtree){
						.id = 2,
						.accept = 1,
					},
				},
				.any = NULL,
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 3,
			//	.load = ty(mkexpr(Zloc, Outag, T, NULL), _int32),
			//	.nnext = 3,
			//	.npat = 3,
			//	.pat = (Node*[]){
			//		ty(mkintlit(Zloc, finducon(_enum3, P(0)->expr.args[0])->id), _int32),
			//		ty(mkintlit(Zloc, finducon(_enum3, P(1)->expr.args[0])->id), _int32),
			//		ty(mkintlit(Zloc, finducon(_enum3, P(2)->expr.args[0])->id), _int32),
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 1,
			//			.accept = 1,
			//		},
			//		[1] = &(Dtree){
			//			.id = 2,
			//			.accept = 1,
			//		},
			//		[2] = &(Dtree){
			//			.id = 3,
			//			.accept = 1,
			//		},
			//	},
			//	.any = NULL,
			//},
		},
		{
			.name = "1-int32-array, matched by an element",
			.val = __T(gentemp(Zloc, _int32a1, NULL)),
			.pats = (Node*[]) {
				__P0(ty(mkexpr(Zloc, Oarr,
						/**
						 * {.[0] = 123}
						 */
						mkidxinit(Zloc, mkintlit(Zloc, 0), mkintlit(Zloc, 123)),
						NULL),
					_int32s1)),
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _int32a1)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 2,
				.nconstructors = 0,
				.load = ty(mkexpr(Zloc, Oidx, T, ty(mkintlit(Zloc, 0), _int64), NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					P(0)->expr.args[0],
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 0,
						.accept = 1,
					},
				},
				.any = &(Dtree){
					.id = 1,
					.accept = 1,
				},
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0,
			//	.load = ty(mkexpr(Zloc, Oidx, T, ty(mkintlit(Zloc, 0), _int64), NULL), _int32),
			//	.nnext = 1,
			//	.npat = 1,
			//	.pat = (Node*[]){
			//		P(0)->expr.args[0],
			//	},
			//	.next = (Dtree*[]){
			//		[0] = &(Dtree){
			//			.id = 1,
			//			.accept = 1,
			//		},
			//	},
			//	.any = &(Dtree){
			//		.id = 2,
			//		.accept = 1,
			//	},
			//},
		},
		/**
		 * | `Foo (int32)
		 */
		{
			.name = "1-union of 1-tuple, matched by wildcard only",
			.val = gentemp(Zloc, _int32u1, NULL),
			.pats = (Node*[]){
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _int32u1)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 0,
				.accept = 1,
			},

			//.dt = &(Dtree){
			//	.id = 0,
			//	.nconstructors = 0,
			//	.load = NULL,
			//	.nnext = 0,
			//	.npat = 0,
			//	.pat = (Node*[]){
			//	},
			//	.next = (Dtree*[]){
			//	},
			//	.any = &(Dtree){
			//		.id = 1,
			//		.accept = 1,
			//	},
			//},
		},
		{
			.name = "1-union of 1-tuple",
			.val = __T(gentemp(Zloc, _int32u1, NULL)),
			.pats = (Node*[]){
				/**
				 * `Foo (123)
				 */
				__P0(ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Foo"),
					       ty(mkexpr(Zloc, Otup, ty(mkexpr(Zloc, Olit, mkint(Zloc, 123), NULL), _int32), NULL), _int32t1), NULL), _int32u1)),
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _int32u1)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 4,
				.load = ty(mkexpr(Zloc, Outag, T, NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					/*
					 * the matcher will convert the Oucon expr to an Nlit for the Dtree
					 */
					ty(mkintlit(Zloc, finducon(_enum1, P(0)->expr.args[0])->id), _int32),
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 2,
						.load = ty(mkexpr(Zloc, Otupget,
								  ty(mkexpr(Zloc, Oudata, T, NULL), _int32t1),
								  mkintlit(Zloc, 0),
								  NULL), _int32),
						.nnext = 1,
						.npat = 1,
						.pat = (Node*[]){
							P(0)->expr.args[1]->expr.args[0],
						},
						.next = (Dtree*[]){
							&(Dtree){
								.id = 0,
								.accept = 1,
							},
						},
						.any = &(Dtree){
							.id = 1,
							.accept = 1,
						},
					},
				},
				.any = &(Dtree){
					.id = 3,
					.accept = 1,
				},
			},
		},
		{
			.name = "bug1",
			.val = gentemp(Zloc, _bug002s, NULL),
			.pats = (Node*[]){
				__P0(ty(mkexprl(Zloc, Ostruct, (Node*[]){
					mkidxinit(Zloc, mkname(Zloc, "s2_ufoo"),
						  ty(mkexpr(Zloc, Oucon, mkname(Zloc, "Foo"),
							    mkintlit(Zloc, 123), NULL), _int32u1)
						 )}, 1), _bug002s)),
				__P_(ty(mkexpr(Zloc, Ogap, NULL), _bug002s)),
				NULL,
			},
			.dt = &(Dtree){
				.id = 4,
				.load = ty(mkexpr(Zloc, Outag,
						  ty(mkexpr(Zloc, Omemb, T, tybase(exprtype(P(0)))->sdecls[0]->decl.name, NULL), _bug001u),
						  NULL), _int32),
				.nnext = 1,
				.npat = 1,
				.pat = (Node*[]){
					ty(mkintlit(Zloc, finducon(_bug001u, P(0)->expr.args[0]->expr.args[0])->id), _int32),
				},
				.next = (Dtree*[]){
					[0] = &(Dtree){
						.id = 2,
						.load = ty(mkexpr(Zloc, Oudata, ty(mkexpr(Zloc, Omemb, T, tybase(exprtype(P(0)))->sdecls[0]->decl.name, NULL), _int32), NULL), _int32t1),
						.nnext = 1,
						.npat = 1,
						.pat = (Node*[]){
							P(0)->expr.args[0]->expr.args[1],
						},
						.next = (Dtree*[]){
							[0] = &(Dtree){
								.id = 0,
								.accept = 1,
							},
						},
						.any = &(Dtree){
							.id = 1,
							.accept = 1,
						}
					},
				},
				.any = &(Dtree){
					.id = 3,
					.accept = 1,
				},
			},
		},

	};

	for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
		Test *t = &tests[i];
		char *name = t->name;
		char *filter = getenv("N");
		if (filter && !strstr(name, filter)) {
			continue;
		}
		initfile(&file, errorf("(test-%d-%s)", i, name));
		fprintf(stderr, "[%03lu]------ %s ##\n", i, name);
		err = test_match(i, t->val, t->pats, t->dt);
		if (err) {
			fprintf(stderr, "FAILED id: %ld name: %s\n", i, name);
			fprintf(stderr, "%s\r\n", err);
			break;
		}
	}
	return 0;
}

