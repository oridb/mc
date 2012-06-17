#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "opt.h"

int islit(Node *n, vlong *v)
{
    Node *l;

    if (exprop(n) != Olit)
	return 0;
    l = n->expr.args[0];
    if (l->lit.littype != Lint)
	return 0;
    *v = l->lit.intval;
    return 1;
}

int isval(Node *n, vlong val)
{
    vlong v;

    if (!islit(n, &v))
	return 0;
    return v == val;
}

Node *val(int line, vlong val)
{
    Node *n;

    n = mkint(line, val);
    n = mkexpr(line, Olit, n, NULL);
    return n;
}

Node *fold(Node *n)
{
    Node **args, *r;
    vlong a, b;
    size_t i;
    
    assert(n->type == Nexpr);

    r = NULL;
    args = n->expr.args;
    if (exprop(n) != Ovar && exprop(n) != Ocjmp &&
	exprop(n) != Ojmp && exprop(n) != Olit)
	for (i = 0; i < n->expr.nargs; i++)
	    args[i] = fold(args[i]);
    switch (exprop(n)) {
	case Ovar:
	    /* FIXME: chase small consts */
	    break;
	case Oadd:
	    /* x + 0 = 0 */
	    if (isval(args[0], 0))
		r = args[1];
	    if (isval(args[1], 0))
		r = args[0];
	    if (islit(args[0], &a) && islit(args[1], &b))
		r = val(n->line, a + b);
	    break;
	case Osub:
	    /* x - 0 = 0 */
	    if (isval(args[1], 0))
		r = args[0];
	    if (islit(args[0], &a) && islit(args[1], &b))
		r = val(n->line, a - b);
	    break;
	case Omul:
	    /* 1 * x = x */
	    if (isval(args[0], 1))
		r = args[1];
	    if (isval(args[1], 1))
		r = args[0];
	    /* 0 * x = 0 */
	    if (isval(args[0], 0))
		r = args[0];
	    if (isval(args[1], 0))
		r = args[1];
	    if (islit(args[0], &a) && islit(args[1], &b))
		r = val(n->line, a * b);
	    break;
	case Odiv:
	    /* x/1 = x */
	    if (isval(args[1], 1))
		r = args[0];
	    /* 0/x = 0 */
	    if (isval(args[1], 0))
		r = args[1];
	    if (islit(args[0], &a) && islit(args[1], &b))
		r = val(n->line, a / b);
	    break;
	case Omod:
	    /* x%1 = x */
	    if (isval(args[1], 0))
		r = args[0];
	    if (islit(args[0], &a) && islit(args[1], &b))
		r = val(n->line, a % b);
	    break;
	case Oneg:
	    if (islit(args[0], &a))
		r = val(n->line, -a);
	    break;
	default:
	    break;
    }

    if (r)
	return r;
    else
	return n;
}
