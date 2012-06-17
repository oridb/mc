#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"

int loaduse(FILE *f, Stab *st)
{
    char *pkg;
    Stab *s;
    Sym *dcl;
    Type *t;
    Node *n;
    int c;

    if (fgetc(f) != 'U')
	return 0;
    pkg = rdstr(f);
    /* if the package names match up, or the usefile has no declared
     * package, then we simply add to the current stab. Otherwise,
     * we add a new stab under the current one */
    if (st->name) {
        if (pkg && !strcmp(pkg, namestr(st->name))) {
            s = st;
        } else {
            s = mkstab();
            s->name = mkname(-1, pkg);
            putns(st, s);
        }
    } else {
        if (pkg) {
            s = mkstab();
            s->name = mkname(-1, pkg);
            putns(st, s);
        } else {
            s = st;
        }
    }
    while ((c = fgetc(f)) != 'Z') {
	switch(c) {
	    case 'G':
                die("We didn't implement generics yet!");
                break;
	    case 'D':
                dcl = symunpickle(f);
                putdcl(s, dcl);
                break;
	    case 'T':
                n = mkname(-1, rdstr(f));
                t = tyunpickle(f);
                puttype(s, n, t);
                break;
	    case EOF:
		break;
	}
    }
    return 1;
}

void readuse(Node *use, Stab *st)
{
    size_t i;
    FILE *fd;
    char *p;

    /* local (quoted) uses are always relative to the cwd */
    fd = NULL;
    if (use->use.islocal) {
	fd = fopen(use->use.name, "r");
    /* nonlocal (barename) uses are always searched on the include path */
    } else {
	for (i = 0; i < nincpaths; i++) {
	    p = strjoin(incpaths[i], use->use.name);
	    fd = fopen(p, "r");
	    if (fd) {
		free(p);
		break;
	    }
	}
    }

    if (!loaduse(fd, st))
	die("Could not load usefile %s", use->use.name);
}

/* Usefile format:
 * U<pkgname>
 * T<typename><pickled-type>
 * D<picled-decl>
 * G<pickled-decl><pickled-initializer>
 * Z
 */
void writeuse(Node *file, FILE *f)
{
    Stab *st;
    void **k;
    Type *t;
    Sym *s;
    size_t i, n;

    st = file->file.exports;
    wrbyte(f, 'U');
    if (st->name)
	wrstr(f, namestr(st->name));
    else
	wrstr(f, NULL);

    k = htkeys(st->ty, &n);
    for (i = 0; i < n; i++) {
	t = htget(st->ty, k[i]);
	wrbyte(f, 'T');
        wrstr(f, namestr(k[i]));
	typickle(t, f);
    }
    free(k);
    k = htkeys(st->dcl, &n);
    for (i = 0; i < n; i++) {
	s = getdcl(st, k[i]);
	if (s->isgeneric)
	    wrbyte(f, 'G');
	else
	    wrbyte(f, 'D');
	sympickle(s, f);
    }
    free(k);
    wrbyte(f, 'Z');
}

