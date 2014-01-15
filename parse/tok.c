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

#define End (-1)

char *filename;
int line;
Tok *curtok;

/* the file contents are stored globally */
static int   fidx;
static int   fbufsz;
static char *fbuf;

static int peekn(int n)
{
    if (fidx + n >= fbufsz)
        return End;
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

/*
 * Consumes the character iff
 * the character is equal to 'c'.
 * returns true if there was a match,
 * false otherwise.
 */
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
            case End:
                fatal(line, "File ended within comment starting at line %d", startln);
                break;
        }
        if (depth == 0)
            break;
    }
}

/*
 * Consumes all forms of whitespace,
 * including comments. If we are in a
 * state where we should ignore newlines,
 * we also consume '\n'. ';' is still
 * accepted as a line ending.
 */
static void eatspace(void)
{
    int c;
    int ignorenl;

    ignorenl = 0;
    while (1) {
        c = peek();
        if (!ignorenl && c == '\n') {
            ignorenl = 0;
            break;
        } else if (c == '\\') {
            ignorenl = 1;
            next();
        } else if (isspace(c) || (ignorenl && c == '\n')) {
            next();
        } else if (c == '/' && peekn(1) == '*') {
            eatcomment();
        } else {
            break;
        }
    }
}

/*
 * Decides if an identifier is a
 * keyword or not. Returns the
 * token type to use for the
 * identifier.
 */
static int kwd(char *s)
{
    static const struct {char* kw; int tt;} kwmap[] = {
        {"break",       Tbreak},
        {"castto",      Tcast},
        {"const",       Tconst},
        {"continue",    Tcontinue},
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
        {"impl",        Timpl},
        {"in",          Tin},
        {"match",       Tmatch},
        {"pkg",         Tpkg},
        {"protect",     Tprotect},
        {"sizeof",      Tsizeof},
        {"struct",      Tstruct},
        {"trait",       Ttrait},
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
    size_t i;
    char c;

    i = 0;
    for (c = peek(); i < sz && identchar(c); c = peek()) {
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


static void encode(char *buf, size_t len, uint32_t c)
{
    int mark;
    size_t i;

    assert(len > 0 && len < 5);
    if (len == 1)
        mark = 0;
    else
        mark = (((1 << (8 - len)) - 1) ^ 0xff);
    for (i = len - 1; i > 0; i--) {
        buf[i] = (c & 0x3f) | 0x80;
        c >>= 6;
    }
    buf[0] = (c | mark);
}

/*
 * Appends a unicode codepoint 'c' to a growable buffer 'buf',
 * resizing if needed.
 */
static void appendc(char **buf, size_t *len, size_t *sz, uint32_t c)
{
    size_t i, charlen;
    char charbuf[5] = {0};

    if (c < 0x80)
        charlen = 1;
    else if (c < 0x800)
        charlen = 2;
    else if (c < 0x10000)
        charlen = 3;
    else if (c < 0x200000)
        charlen = 4;
    else
        fatal(line, "invalid utf character '\\u{%x}'", c);

    encode(charbuf, charlen, c);
    for (i = 0; i < charlen; i++)
         append(buf, len, sz, charbuf[i]);
}

static int ishexval(char c)
{
    if (c >= 'a' && c <= 'f')
        return 1;
    else if (c >= 'A' && c <= 'F')
        return 1;
    else if (c >= '0' && c <= '9')
        return 1;
    return 0;
}

/*
 * Converts a character to its hex value.
 */
static int hexval(char c)
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= '0' && c <= '9')
        return c - '0';
    fatal(line, "passed non-hex value '%c' to where hex was expected", c);
    return -1;
}

/* \u{abc} */
static int32_t unichar()
{
    uint32_t v;
    int c;

    /* we've already seen the \u */
    if (next() != '{')
        fatal(line, "\\u escape sequence without initial '{'");
    v = 0;
    while (ishexval(peek())) {
        c = next();
        v = 16*v + hexval(c);
        if (v > 0x10FFFF)
            fatal(line, "invalid codepoint for \\u escape sequence");
    }
    if (next() != '}')
        fatal(line, "\\u escape sequence without ending '}'");
    return v;
}

/*
 * decodes an escape code. These are
 * shared between strings and characters.
 * Unknown escape codes are ignored.
 */
static int decode(char **buf, size_t *len, size_t *sz)
{
    char c, c1, c2;
    int32_t v;

    c = next();
    /* we've already seen the '\' */
    switch (c) {
        case 'u':
            v = unichar();
            appendc(buf, len, sz, v);
            return v;
        case 'x': /* arbitrary hex */
            c1 = next();
            if (!isxdigit(c1))
                fatal(line, "expected hex digit, got %c", c1);
            c2 = next();
            if (!isxdigit(c2))
                fatal(line, "expected hex digit, got %c", c1);
            v = 16*hexval(c1) + hexval(c2);
            break;
        case 'n': v = '\n'; break;
        case 'r': v = '\r'; break;
        case 't': v = '\t'; break;
        case 'b': v = '\b'; break;
        case '"': v = '\"'; break;
        case '\'': v = '\''; break;
        case 'v': v = '\v'; break;
        case '\\': v = '\\'; break;
        case '0': v = '\0'; break;
        default: fatal(line, "unknown escape code \\%c", c);
    }
    append(buf, len, sz, v);
    return v;
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
        else if (c == End)
            fatal(line, "Unexpected EOF within string");
        else if (c == '\n')
            fatal(line, "Newlines not allowed in strings");
        else if (c == '\\')
            decode(&buf, &len, &sz);
        else
            append(&buf, &len, &sz, c);
    };
    append(&buf, &len, &sz, '\0');

    t = mktok(Tstrlit);
    t->str = buf;
    return t;
}

static uint32_t readutf(char c, char **buf, size_t *buflen, size_t *sz) {
    size_t i, len;
    uint32_t val;

    if ((c & 0x80) == 0)
        len = 1;
    else if ((c & 0xe0) == 0xc0)
        len = 2;
    else if ((c & 0xf0) == 0xe0)
        len = 3;
    else if ((c & 0xf8) == 0xf0)
        len = 4;
    else
        fatal(line, "Invalid utf8 encoded character constant");

    val = c & ((1 << (8 - len)) - 1);
    append(buf, buflen, sz, c);
    for (i = 1; i < len; i++) {
        c = next();
        if ((c & 0xc0) != 0x80)
            fatal(line, "Invalid utf8 codepoint in character literal");
        val = (val << 6) | (c & 0x3f);
        append(buf, buflen, sz, c);
    }
    return val;
}

static Tok *charlit()
{
    Tok *t;
    int c;
    uint32_t val;
    size_t len, sz;
    char *buf;


    assert(next() == '\'');

    buf = NULL;
    len = 0;
    sz = 0;
    val = 0;
    c = next();
    if (c == End)
        fatal(line, "Unexpected EOF within char lit");
    else if (c == '\n')
        fatal(line, "Newlines not allowed in char lit");
    else if (c == '\\')
        val = decode(&buf, &len, &sz);
    else
        val = readutf(c, &buf, &len, &sz);
    append(&buf, &len, &sz, '\0');
    if (next() != '\'')
        fatal(line, "Character constant with multiple characters");

    t = mktok(Tchrlit);
    t->chrval = val;
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
        case '#': tt = Tderef; break;
        case ':':
                  if (match(':'))
                      tt = Twith;
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
                      tt = Tmul;
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

static Tok *number(int base)
{
    Tok *t;
    int start;
    int c;
    int isfloat;

    t = NULL;
    isfloat = 0;
    start = fidx;
    for (c = peek(); isxdigit(c) || c == '.' || c == '_'; c = peek()) {
        next();
        if (c == '_')
            continue;
        if (c == '.')
            isfloat = 1;
        else if (hexval(c) < 0 || hexval(c) > base)
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
        else if (match('o'))
            t = number(8);
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
    if (c == End) {
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

/* Interface to yacc */
int yylex(void)
{
    curtok = toknext();
    yylval.tok = curtok;
    return curtok->type;
}
