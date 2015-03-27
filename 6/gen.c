#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
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

static int islocal(Node *dcl)
{
    if (dcl->decl.vis != Visintern)
        return 0;
    if (dcl->decl.isimport || dcl->decl.isextern)
        return 0;
    return 1;
}

static int nextlbl;
char *genlocallblstr(char *buf, size_t sz)
{
    if (asmsyntax == Plan9)
        snprintf(buf, 128, ".L%d<>", nextlbl++);
    else
        snprintf(buf, 128, ".L%d", nextlbl++);
    return buf;
}

char *genlblstr(char *buf, size_t sz)
{
    snprintf(buf, 128, ".L%d", nextlbl++);
    return buf;
}


/* 
 * For x86, the assembly names are generated as follows:
 *      local symbols: .name
 *      un-namespaced symbols: <symprefix>name
 *      namespaced symbols: <symprefix>namespace$name
 *      local symbols on plan9 have the file-unique suffix '<>' appended
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
    if (asmsyntax == Plan9)
        if (islocal(dcl))
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

char *tydescid(char *buf, size_t bufsz, Type *ty)
{
    char *sep, *ns;

    sep = "";
    ns = "";
    if (ty->type == Tyname) {
        if (ty->name->name.ns) {
            ns = ty->name->name.ns;
            sep = "$";
        }
        if (ty->vis == Visexport || ty->isimport)
            snprintf(buf, bufsz, "_tydesc$%s%s%s", ns, sep, ty->name->name.name);
        else
            snprintf(buf, bufsz, "_tydesc$%s%s%s$%d", ns, sep, ty->name->name.name, ty->tid);
    } else {
        if (file->file.globls->name) {
            ns = file->file.globls->name;
            sep = "$";
        }
        snprintf(buf, bufsz, "_tydesc%s%s$%d",sep, ns, ty->tid);
    }
    return buf;
}

void gen(Node *file, char *out)
{
    switch (asmsyntax) {
        case Plan9:     genp9(file, out);       break;
        case Gnugas:    gengas(file, out);      break;
        default:        die("unknown target");  break;
    }
}
