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

static void loaduses(Node *n)
{
    int i;
    /* uses only allowed at top level. Do we want to keep it this way? */
    for (i = 0; i < n->file.nuses; i++)
        readuse(n->file.uses[i], n->file.globls);
}

static void inferdecl(Node *n)
{
}

static void inferexpr(Node *n)
{
}

static void inferlit(Node *n)
{
}

static void inferfunc(Node *n)
{
}

static void infernode(Node *n)
{
    int i;

    switch (n->type) {
        case Nfile:
            for (i = 0; i < n->file.nuses; i++)
                infernode(n->file.uses[i]);
            for (i = 0; i < n->file.nstmts; i++)
                infernode(n->file.stmts[i]);
            break;
        case Ndecl:
            inferdecl(n);
            break;
        case Nblock:
            for (i = 0; i < n->block.nstmts; i++)
                infernode(n->block.stmts[i]);
            break;
        case Nifstmt:
            infernode(n->ifstmt.cond);
            infernode(n->ifstmt.iftrue);
            infernode(n->ifstmt.iffalse);
            break;
        case Nloopstmt:
            infernode(n->loopstmt.cond);
            infernode(n->loopstmt.init);
            infernode(n->loopstmt.step);
            infernode(n->loopstmt.body);
            break;
        case Nexpr:
            inferexpr(n);
        case Nlit:
            inferlit(n);
        case Nfunc:
            inferfunc(n);
        case Nname:
        case Nuse:
            break;
    }
}

static void infercompn(Node *n)
{
}

static void checkcast(Node *n)
{
}

static void typesub(Node *n)
{
}

void infer(Node *file)
{
    assert(file->type == Nfile);
    loaduses(file);
    infernode(file);
    infercompn(file);
    checkcast(file);
    typesub(file);
}
