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

void gen(Asmsyntax syn, Node *file, char *out)
{
    switch (syn) {
        case Plan9:     genp9(file, out);       break;
        case Gnugas:    gengas(file, out);      break;
        default:        die("unknown target");  break;
    }
}
