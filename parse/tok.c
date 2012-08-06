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
    static const struct {char* kw; int tt;} kwmap[] = {
        {"castto",      Tcast},
        {"const",       Tconst},
        {"default",     Tdefault},
        {"elif",        Telif},
        {"else",        Telse},
        {"export",      Texport},
        {"extern",      Textern},
        {"false",       Tboollit},
        {"for",         Tfor},
        {"generic",     Tgeneric},
        {"goto",        Tgoto},
        {"if",          Tif},
        {"match",       Tmatch},
        {"pkg",         Tpkg},
        {"protect",     Tprotect},
        {"sizeof",      Tsizeof},
        {"struct",      Tstruct},
        {"true",        Tboollit},
        {"type",        Ttype},
        {"union",       Tunion},
        {"use",         Tuse},
        {"var",         Tvar},
        {"while",       Twhile},
    };

    size_t min, max, mid;
    int cmp;


    min = 0;
    max = sizeof(kwmap)/sizeof(kwmap[0]);
    while (max > min) {
        mid = (max + min) / 2;
        cmp = strcmp(s, kwmap[mid].kw);
        if (cmp == 0)
            return kwmap[mid].tt;
        else if (cmp > 0)
            min = mid + 1;
        else if (cmp < 0)
            max = mid;
    }
    return Tident;
}

static int identstr(char *buf, size_t sz)
{
    int i;
    char c;
    
    i = 0;
    for (c = peek(); i < 1023 && identchar(c); c = peek()) {
        next();
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

static Tok *kwident(void)
{
    char buf[1024];
    Tok *t;

    if (!identstr(buf, sizeof buf))
        return NULL;
    t = mktok(kwd(buf));
    t->str = strdup(buf);
    return t;
}

static void append(char **buf, size_t *len, size_t *sz, int c)
{
    if (!*sz) {
        *sz = 16;
        *buf = malloc(*sz);
    }
    if (*len == *sz - 1) {
        *sz = *sz * 2;
        *buf = realloc(*buf, *sz);
    }

    buf[0][len[0]++] = c;
}

static int hexval(char c)
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= '0' && c <= '9')
        return c - '0';
    die("passed non-hex value to hexval()");
    return -1;
}

static void decode(char **buf, size_t *len, size_t *sz)
{
    char c, c1, c2;
    int v;

    c = next();
    /* we've already seen the '\' */
    switch (c) {
        case 'x': /* arbitrary hex */
            c1 = next();
            if (!isxdigit(c1))
                fatal(line, "expected hex digit, got %c", c1);
            c2 = next();
            if (!isxdigit(c2))
                fatal(line, "expected hex digit, got %c", c1);
            v = 16*hexval(c1) + hexval(c2);
            append(buf, len, sz, v);
            break;
        case 'n': append(buf, len, sz, '\n'); break;
        case 'r': append(buf, len, sz, '\r'); break;
        case 't': append(buf, len, sz, '\t'); break;
        case 'b': append(buf, len, sz, '\b'); break;
        case '"': append(buf, len, sz, '\"'); break;
        case '\'': append(buf, len, sz, '\''); break;
        case 'v': append(buf, len, sz, '\v'); break;
        case '\\': append(buf, len, sz, '\\'); break;
        case '0': append(buf, len, sz, '\0'); break;
        default: fatal(line, "unknown escape code \\%c", c);
    }
}

static Tok *strlit()
{
    Tok *t;
    int c;
    size_t len, sz;
    char *buf;

    assert(next() == '"');

    buf = NULL;
    len = 0;
    sz = 0;
    while (1) {
        c = next();
        /* we don't unescape here, but on output */
        if (c == '"')
            break;
        else if (c == '\0')
            fatal(line, "Unexpected EOF within string");
        else if (c == '\n')
            fatal(line, "Newlines not allowed in strings");
        else if (c == '\\')
            decode(&buf, &len, &sz);
        else
            append(&buf, &len, &sz, c);
    };
    buf[len] = '\0';

    t = mktok(Tstrlit);
    t->str = buf;
    return t;
}

static Tok *charlit()
{
    Tok *t;
    int c;
    size_t len, sz;
    char *buf;

    assert(next() == '\'');

    buf = NULL;
    len = 0;
    sz = 0;
    while (1) {
        c = next();
        /* we don't unescape here, but on output */
        if (c == '\'')
            break;
        else if (c == '\0')
            fatal(line, "Unexpected EOF within char lit");
        else if (c == '\n')
            fatal(line, "Newlines not allowed in char lit");
        else if (c == '\\')
            decode(&buf, &len, &sz);
        else
            append(&buf, &len, &sz, c);

    };
    buf[len] = '\0';

    t = mktok(Tchrlit);
    t->str = buf;
    return t;
}

static Tok *oper(void)
{
    int tt;
    char c;

    c = next();
    switch (c) {
        case '{': tt = Tobrace; break;
        case '}': tt = Tcbrace; break;
        case '(': tt = Toparen; break;
        case ')': tt = Tcparen; break;
        case '[': tt = Tosqbrac; break;
        case ']': tt = Tcsqbrac; break;
        case ',': tt = Tcomma; break;
        case '`': tt = Ttick; break;
        case '#': tt = Thash; break;
        case ':':
                  if (match(':'))
                      tt = Ttrait;
                  else
                      tt = Tcolon;
                  break;
        case '~': tt = Tbnot; break;
        case ';':
                  if (match(';'))
                      tt = Tendblk;
                  else
                      tt = Tendln;
                  break;
        case '.':
                  if (match('.')) {
                      if (match('.')) {
                          tt = Tellipsis;
                      } else {
                          unget();
                          tt = Tdot;
                      }
                  } else {
                      tt = Tdot;
                  }
                  break;
        case '+':
                  if (match('='))
                      tt = Taddeq;
                  else if (match('+'))
                      tt = Tinc;
                  else
                      tt = Tplus;
                  break;
        case '-':
                  if (match('='))
                      tt = Tsubeq;
                  else if (match('-'))
                      tt = Tdec;
                  else if (match('>'))
                      tt = Tret;
                  else
                      tt = Tminus;
                  break;
        case '*':
                  if (match('='))
                      tt = Tmuleq;
                  else
                      tt = Tstar;
                  break;
        case '/':
                  if (match('='))
                      tt = Tdiveq;
                  else
                      tt = Tdiv;
                  break;
        case '%':
                  if (match('='))
                      tt = Tmodeq;
                  else
                      tt = Tmod;
                  break;
        case '=':
                  if (match('='))
                      tt = Teq;
                  else
                      tt = Tasn;
                  break;
        case '|':
                  if (match('='))
                      tt = Tboreq;
                  else if (match('|'))
                      tt = Tlor;
                  else
                      tt = Tbor;
                  break;
        case '&':
                  if (match('='))
                      tt = Tbandeq;
                  else if (match('&'))
                      tt = Tland;
                  else
                      tt = Tband;
                  break;
        case '^':
                  if (match('='))
                      tt = Tbxoreq;
                  else
                      tt = Tbxor;
                  break;
        case '<':
                  if (match('=')) {
                      tt = Tle;
                  } else if (match('<')) {
                      if (match('='))
                          tt = Tbsleq;
                      else
                          tt = Tbsl;
                  } else {
                      tt = Tlt;
                  }
                  break;
        case '>':
                  if (match('=')) {
                      tt = Tge;
                  } else if (match('>')) {
                      if (match('='))
                          tt = Tbsreq;
                      else
                          tt = Tbsr;
                  } else {
                      tt = Tgt;
                  }
                  break;

        case '!':
                  if (match('='))
                      tt = Tne;
                  else
                      tt = Tlnot;
                  break;
        default:
                  tt = Terror;
                  fatal(line, "Junk character %c", c);
                  break;
    }
    return mktok(tt);
};

static int ord(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'z')
        return c - 'a';
    else
        return c - 'A';
}

static Tok *number(int base)
{
    Tok *t;
    int start;
    int c;
    int isfloat;

    t = NULL;
    isfloat = 0;
    start = fidx;
    for (c = peek(); isxdigit(c) || c == '.'; c = peek()) {
        next();
        if (c == '.')
            isfloat = 1;
        if (ord(c) > base)
            fatal(line, "Integer digit '%c' outside of base %d", c, base);
    }

    /* we only support base 10 floats */
    if (isfloat && base == 10) {
        t = mktok(Tfloatlit);
        t->str = strdupn(&fbuf[start], fidx - start);
	t->fltval = strtod(t->str, NULL);
    } else {
        t = mktok(Tintlit);
        t->str = strdupn(&fbuf[start], fidx - start);
	t->intval = strtol(t->str, NULL, base);
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

static Tok *typaram()
{
    Tok *t;
    char buf[1024];

    t = NULL;
    if (!match('@'))
        return NULL;
    if (!identstr(buf, 1024))
        return NULL;
    t = mktok(Ttyparam);
    t->str = strdup(buf);
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
        t =  mktok(Tendln);
    } else if (isalpha(c) || c == '_') {
        t =  kwident();
    } else if (c == '"') {
        t =  strlit();
    } else if (c == '\'') {
        t = charlit();
    } else if (isdigit(c)) {
        t =  numlit();
    } else if (c == '@') {
        t = typaram();
    } else {
        t = oper();
    }

    if (!t || t->type == Terror)
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
