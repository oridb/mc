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

int loaduse(FILE *fd, Stab *st)
{
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

    if (loaduse(fd, st))
	die("Could not load usefile %s", use->use.name);
}

void writeuse(Node *file, FILE *out)
{
}

