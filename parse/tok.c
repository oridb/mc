#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>

#include "parse.h"

#include "gram.h"

char *filename;
int line;
int ignorenl;
Tok *curtok;

static int   fidx;
static int   fbufsz;
static char *fbuf;

static int peekn(int n)
{
    if (fidx + n >= fbufsz)
        return '\0';
    else
        return fbuf[fidx + n];
}

static int peek(void)
{
    return peekn(0);
}

static int next(void)
{
    int c;

    c = peek();
    fidx++;
    return c;
}

static void unget(void)
{
    fidx--;
    assert(fidx >= 0);
}

static int match(char c)
{
    if (peek() == c) {
        next();
        return 1;
    } else {
        return 0;
    }
}

static Tok *mktok(int tt)
{
    Tok *t;

    t = zalloc(sizeof(Tok));
    t->type = tt;
    t->line = line;
    return t;
}

static int identchar(int c)
{
    return isalnum(c) || c == '_';
}

static void eatcomment(void)
{
    int depth;
    int startln;
    int c;

    depth = 0;
    startln = line;
    while (1) {
        c = next();
        switch (c) {
            /* enter level of nesting */
            case '/':
                if (match('*'))
                    depth++;
                break;
            /* leave level of nesting */
            case '*':
                if (match('/'))
                    depth--;
                break;
            /* have to keep line numbers synced */
            case '\n':
                line++;
                break;
            case '\0':
                fatal(line, "File ended within comment starting at line %d", startln);
                break;
        }
        if (depth == 0)
            break;
    }
}

static void eatspace(void)
{
    int c;

    while (1) {
        c = peek();
        if ((!ignorenl && c == '\n'))
            break;
        else if (isspace(c))
            next();
        else if (c == '/' && peekn(1) == '*')
            eatcomment();
        else
            break;
    }
}

static int kwd(char *s)
{
    int i;
    struct {char* kw; int tt;} kwmap[] = {
        {"type",        TType},
        {"for",         TFor},
        {"while",       TWhile},
        {"if",          TIf},
        {"else",        TElse},
        {"elif",        TElif},
        {"match",       TMatch},
        {"default",     TDefault},
        {"goto",        TGoto},
        {"enum",        TEnum},
        {"struct",      TStruct},
        {"union",       TUnion},
        {"const",       TConst},
        {"var",         TVar},
        {"extern",      TExtern},
        {"export",      TExport},
        {"protect",     TProtect},
        {"use",         TUse},
        {"pkg",         TPkg},
        {"sizeof",      TSizeof},
        {"true",        TBoollit},
        {"false",       TBoollit},
        {NULL, 0}
    };

    for (i = 0; kwmap[i].kw; i++)
        if (!strcmp(kwmap[i].kw, s))
            return kwmap[i].tt;

    return TIdent;
}

static Tok *kwident(void)
{
    char buf[1024];
    char c;
    int i;
    Tok *t;

    i = 0;
    for (c = peek(); i < 1023 && identchar(c); c = peek()) {
        next();
        buf[i++] = c;
    }
    buf[i] = '\0';
    t = mktok(kwd(buf));
    t->str = strdup(buf);
    return t;
}

static Tok *strlit()
{
    Tok *t;
    int sstart; /* start of string within input buf */
    int c;

    assert(next() == '"');

    sstart = fidx;
    while (1) {
        c = next();
        /* we don't unescape here, but on output */
        if (c == '"')
            break;
        else if (c == '\\')
            c = next();

        if (c == '\0')
            fatal(line, "Unexpected EOF within string");
        else if (c == '\n')
            fatal(line, "Newlines not allowed in strings");
    };
    t = mktok(TStrlit);
    t->str = strdupn(&fbuf[sstart], fidx - sstart);
    return t;
}

static Tok *charlit()
{
    Tok *t;
    int sstart; /* start of string within input buf */
    int c;

    assert(next() == '\'');

    sstart = fidx;
    while (1) {
        c = next();
        /* we don't unescape here, but on output */
        if (c == '\'')
            break;
        else if (c == '\\')
            c = next();

        if (c == '\0')
            fatal(line, "Unexpected EOF within char lit");
        else if (c == '\n')
            fatal(line, "Newlines not allowed in char lit");
    };
    t = mktok(TStrlit);
    t->str = strdupn(&fbuf[sstart], fidx - sstart);
    return t;
}

static Tok *oper(void)
{
    int tt;
    char c;

    c = next();
    switch (c) {
        case '{': tt = TObrace; break;
        case '}': tt = TCbrace; break;
        case '(': tt = TOparen; break;
        case ')': tt = TCparen; break;
        case '[': tt = TOsqbrac; break;
        case ']': tt = TCsqbrac; break;
        case ',': tt = TComma; break;
        case ':': tt = TColon; break;
        case '~': tt = TBnot; break;
        case ';':
                  if (match(';'))
                      tt = TEndblk;
                  else
                      tt = TEndln;
                  break;
        case '.':
                  if (match('.')) {
                      if (match('.')) {
                          tt = TEllipsis;
                      } else { 
                          unget();
                          tt = TDot;
                      }
                  } else {
                      tt = TDot;
                  }
                  break;
        case '+':
                  if (match('='))
                      tt = TAddeq;
                  else if (match('+'))
                      tt = TInc;
                  else
                      tt = TPlus;
                  break;
        case '-':
                  if (match('='))
                      tt = TSubeq;
                  else if (match('-'))
                      tt = TDec;
                  else if (match('>'))
                      tt = TRet;
                  else
                      tt = TMinus;
                  break;
        case '*':
                  if (match('='))
                      tt = TMuleq;
                  else
                      tt = TStar;
                  break;
        case '/':
                  if (match('='))
                      tt = TDiveq;
                  else
                      tt = TDiv;
                  break;
        case '%':
                  if (match('='))
                      tt = TModeq;
                  else
                      tt = TMod;
                  break;
        case '=':
                  if (match('='))
                      tt = TEq;
                  else
                      tt = TAsn;
                  break;
        case '|':
                  if (match('='))
                      tt = TBoreq;
                  else if (match('|'))
                      tt = TLor;
                  else
                      tt = TBor;
                  break;
        case '&':
                  if (match('='))
                      tt = TBandeq;
                  else if (match('|'))
                      tt = TLand;
                  else
                      tt = TBand;
                  break;
        case '^':
                  if (match('='))
                      tt = TBxoreq;
                  else
                      tt = TBxor;
                  break;
        case '<':
                  if (match('=')) {
                      tt = TLe;
                  } else if (match('<')) {
                      if (match('='))
                          tt = TBsleq;
                      else
                          tt = TBsl;
                  } else {
                      tt = TLt;
                  }
                  break;
        case '>':
                  if (match('=')) {
                      tt = TGe;
                  } else if (match('<')) {
                      if (match('='))
                          tt = TBsreq;
                      else
                          tt = TBsr;
                  } else {
                      tt = TGt;
                  }
                  break;

        case '!':
                  if (match('='))
                      tt = TNe;
                  else
                      tt = TLnot;
                  break;
        default:
                  tt = TError;
                  fatal(line, "Junk character %c", c);
                  break;
    }
    return mktok(tt);
};

static Tok *number(int base)
{
    Tok *t;
    int start;
    char *endp;
    int c;
    int isfloat;

    t = NULL;
    isfloat = 0;
    start = fidx;
    for (c = peek(); isxdigit(c) || c == '.'; c = peek()) {
        next();
        if (c == '.')
            isfloat = 1;
    }

    /* we only support base 10 floats */
    if (isfloat && base == 10) {
        strtod(&fbuf[start], &endp);
        if (endp == &fbuf[fidx]) {
            t = mktok(TFloatlit);
            t->str = strdupn(&fbuf[start], fidx - start);
        }
    } else {
        strtol(&fbuf[start], &endp, base);
        if (endp == &fbuf[fidx]) {
            t = mktok(TIntlit);
            t->str = strdupn(&fbuf[start], fidx - start);
        }
    }

    return t;
}

static Tok *numlit(void)
{
    Tok *t;

    /* check for 0x or 0b prefix */
    if (match('0')) {
        if (match('x'))
            t = number(16);
        else if (match('b'))
            t = number(2);
        else
            t = number(10);
    } else {
        t = number(10);
    }

    return t;
}

static Tok *toknext()
{
    Tok *t;
    int c;

    eatspace();
    c = peek();
    if (c == '\0') {
        t =  mktok(0);
    } else if (c == '\n') {
        line++;
        next();
        t =  mktok(TEndln);
    } else if (isalpha(c) || c == '_') {
        t =  kwident();
    } else if (c == '"') {
        t =  strlit();
    } else if (c == '\'') {
        t = charlit();
    } else if (isdigit(c)) {
        t =  numlit();
    } else {
        t = oper();
    }

    if (!t || t->type == TError)
        fatal(line, "Unable to parse token starting with %c", c);
    return t;
}

void tokinit(char *file)
{
    int fd;
    int n;
    int nread;


    fd = open(file, O_RDONLY);
    if (fd == -1)
        err(errno, "Unable to open file %s", file);

    nread = 0;
    fbuf = malloc(4096);
    while (1) {
        n = read(fd, fbuf + nread, 4096);
        if (n < 0)
            fatal(errno, "Error reading file %s", file);
        if (n == 0)
            break;
        if (!fbuf)
            die("Out of memory reading %s", file);
        nread += n;
        fbuf = xrealloc(fbuf, nread + 4096);
    }

    fbufsz = nread;
    line = 1;
    fidx = 0;
    close(fd);
    filename = strdup(file);
}

int yylex(void)
{
    curtok = toknext();
    yylval.tok = curtok;
    return curtok->type;
}
