#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "mi.h"
#include "asm.h"
#include "../config.h"

/* For x86, the assembly names are generated as follows:
 *      local symbols: .name
 *      un-namespaced symbols: <symprefix>name
 *      namespaced symbols: <symprefix>namespace$name
 */
char *asmname(Node *dcl)
{
    char buf[1024];
    char *vis, *pf, *ns, *name, *sep;
    Node *n;

    n = dcl->decl.name;
    pf = Symprefix;
    ns = n->name.ns;
    name = n->name.name;
    vis = "";
    sep = "";
    if (dcl->decl.vis == Vishidden && asmsyntax == Plan9)
        vis = "<>";
    if (!ns || !ns[0])
        ns = "";
    else
        sep = "$";
    if (name[0] == '.')
        pf = "";

    snprintf(buf, sizeof buf, "%s%s%s%s%s", pf, ns, sep, name, vis);
    return strdup(buf);
}


void gen(Node *file, char *out)
{
    Asmsyntax syn = Gnugas;
    switch (syn) {
        case Plan9:     genp9(file, out);       break;
        case Gnugas:    gengas(file, out);      break;
        default:        die("unknown target");  break;
    }
}
