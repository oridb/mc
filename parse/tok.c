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
#include <errno.h>
#include <unistd.h>

#include "util.h"
#include "parse.h"

#include "gram.h"

#define End (-1)

char *filename;
Srcloc curloc;
Tok *curtok;

/* the file contents are stored globally */
static int fidx;
static int fbufsz;
static char *fbuf;

static int
peekn(int n)
{
	if (fidx + n >= fbufsz)
		return End;
	else
		return fbuf[fidx + n];
}

static int
peek(void)
{
	return peekn(0);
}

static int
next(void)
{
	int c;

	c = peek();
	fidx++;
	return c;
}

static void
unget(void)
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
static int
match(char c)
{
	if (peek() == c) {
		next();
		return 1;
	} else {
		return 0;
	}
}

static Tok *
mktok(int tt)
{
	Tok *t;

	t = zalloc(sizeof(Tok));
	t->type = tt;
	t->loc = curloc;
	return t;
}

static int
identchar(int c) {
    return isalnum(c) || c == '_' || c == '$';
}

static void
eatcomment(void)
{
	int depth;
	int startln;
	int c;

	depth = 0;
	startln = curloc.line;
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
		case '\n': curloc.line++; break;
		case End:
			   lfatal(curloc, "File ended within comment starting at line %d", startln);
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
static void
eatspace(void)
{
	int c;
	int ignorenl;

	ignorenl = 0;
	while (1) {
		c = peek();
		if (!ignorenl && c == '\n') {
			break;
		} else if (c == '\\') {
			ignorenl = 1;
			next();
		} else if (ignorenl && c == '\n') {
			next();
			curloc.line++;
			ignorenl = 0;
		} else if (isspace(c)) {
			next();
		} else if (c == '/' && peekn(1) == '*') {
			eatcomment();
		} else if (c == '/' && peekn(1) == '/') {
			while (peek() != End && peek() != '\n')
				next();
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
static int
kwd(char *s)
{
	static const struct {
		char *kw;
		int tt;
	} kwmap[] = {
		{"$noret", Tattr},
		{"_", Tgap},
		{"auto", Tauto},
		{"break", Tbreak},
		{"const", Tconst},
		{"continue", Tcontinue},
		{"elif", Telif},
		{"else", Telse},
		{"extern", Tattr},
		{"false", Tboollit},
		{"for", Tfor},
		{"generic", Tgeneric},
		{"goto", Tgoto},
		{"if", Tif},
		{"impl", Timpl},
		{"match", Tmatch},
		{"pkg", Tpkg},
		{"pkglocal", Tattr},
		{"sizeof", Tsizeof},
		{"struct", Tstruct},
		{"trait", Ttrait},
		{"true", Tboollit},
		{"type", Ttype},
		{"union", Tunion},
		{"use", Tuse},
		{"var", Tvar},
		{"void", Tvoidlit},
		{"while", Twhile},
	};

	size_t min, max, mid;
	int cmp;

	min = 0;
	max = sizeof(kwmap) / sizeof(kwmap[0]);
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

static int
identstr(char *buf, size_t sz)
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

static Tok *
kwident(void)
{
	char buf[1024];
	Tok *t;

	if (!identstr(buf, sizeof buf))
		return NULL;
	t = mktok(kwd(buf));
	t->id = strdup(buf);
	return t;
}

static void
append(char **buf, size_t *len, size_t *sz, int c)
{
	if (!*sz) {
		*sz = 16;
		*buf = malloc(*sz);
	}
	if (*len == *sz - 1) {
		*sz = *sz * 2;
		*buf = realloc(*buf, *sz);
	}

	buf[0][*len] = c;
	(*len)++;
}

static void
encode(char *buf, size_t len, uint32_t c)
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
static void
appendc(char **buf, size_t *len, size_t *sz, uint32_t c)
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
		lfatal(curloc, "invalid utf character '\\u{%x}'", c);

	encode(charbuf, charlen, c);
	for (i = 0; i < charlen; i++)
		append(buf, len, sz, charbuf[i]);
}

static int
ishexval(char c)
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
static int
hexval(char c)
{
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else if (c >= '0' && c <= '9')
		return c - '0';
	lfatal(curloc, "passed non-hex value '%c' to where hex was expected", c);
	return -1;
}

/* \u{abc} */
static int32_t
unichar(void)
{
	uint32_t v;
	int c;

	/* we've already seen the \u */
	if (next() != '{')
		lfatal(curloc, "\\u escape sequence without initial '{'");
	v = 0;
	while (ishexval(peek())) {
		c = next();
		v = 16 * v + hexval(c);
		if (v > 0x10FFFF)
			lfatal(curloc, "invalid codepoint for \\u escape sequence");
	}
	if (next() != '}')
		lfatal(curloc, "\\u escape sequence without ending '}'");
	return v;
}

/*
 * decodes an escape code. These are
 * shared between strings and characters.
 * Unknown escape codes are ignored.
 */
static int
decode(char **buf, size_t *len, size_t *sz)
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
			lfatal(curloc, "expected hex digit, got %c", c1);
		c2 = next();
		if (!isxdigit(c2))
			lfatal(curloc, "expected hex digit, got %c", c1);
		v = 16 * hexval(c1) + hexval(c2);
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
	default: lfatal(curloc, "unknown escape code \\%c", c);
	}
	append(buf, len, sz, v);
	return v;
}

static Tok *
strlit(void)
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
			lfatal(curloc, "Unexpected EOF within string");
		else if (c == '\n')
			lfatal(curloc, "Newlines not allowed in strings");
		else if (c == '\\')
			decode(&buf, &len, &sz);
		else
			append(&buf, &len, &sz, c);
	};
	t = mktok(Tstrlit);
	t->strval.len = len;

	/* null terminator should not count towards length */
	append(&buf, &len, &sz, '\0');
	t->strval.buf = buf;
	t->id = buf;
	return t;
}

static uint32_t
readutf(char c, char **buf, size_t *buflen, size_t *sz)
{
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
		lfatal(curloc, "Invalid utf8 encoded character constant");

	val = c & ((1 << (8 - len)) - 1);
	append(buf, buflen, sz, c);
	for (i = 1; i < len; i++) {
		c = next();
		if ((c & 0xc0) != 0x80)
			lfatal(curloc, "Invalid utf8 codepoint in character literal");
		val = (val << 6) | (c & 0x3f);
		append(buf, buflen, sz, c);
	}
	return val;
}

static Tok *
charlit(void)
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
		lfatal(curloc, "Unexpected EOF within char lit");
	else if (c == '\n')
		lfatal(curloc, "Newlines not allowed in char lit");
	else if (c == '\\')
		val = decode(&buf, &len, &sz);
	else
		val = readutf(c, &buf, &len, &sz);
	append(&buf, &len, &sz, '\0');
	if (next() != '\'')
		lfatal(curloc, "Character constant with multiple characters");

	t = mktok(Tchrlit);
	t->chrval = val;
	t->id = buf;
	return t;
}

static Tok *
oper(void)
{
	int tt;
	char c;

	tt = 0;
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
	case '?': tt = Tqmark; break;
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
		  lfatal(curloc, "Junk character %c", c);
		  break;
	}
	return mktok(tt);
}

static Tok *
number(int base)
{
	Tok *t;
	int start;
	int c;
	int isfloat;
	int maybefloat;
	int unsignedval;
	/* because we allow '_' in numbers, and strtod/stroull don't, we
	 * need a buffer that holds the number without '_'.
	 */
	char buf[2048];
	char *endp;
	size_t nbuf;

	t = NULL;
	isfloat = 0;
	start = fidx;
	nbuf = 0;
	/* allow floating point literals only if the previous token was
	 * not a dot; this lets the user write "foo.1.2" to access nested
	 * tuple fields.
	 */
	maybefloat = !curtok || (curtok->type != Tdot);
	for (c = peek(); isxdigit(c) || (maybefloat && c == '.') || c == '_'; c = peek()) {
		next();
		if (c == '_')
			continue;
		if (nbuf >= sizeof buf - 1) {
			buf[nbuf - 1] = '\0';
			lfatal(curloc, "number %s... too long to represent", buf);
		}

		/* float radix */
		if (c == '.') {
			isfloat = 1;
			buf[nbuf++] = c;
		/* exponential notation */
		} else if (base == 10 && (c == 'e' || c == 'E')) {
			isfloat = 1;
			buf[nbuf++] = c;
			if ((peek() == '+' || peek() == '-') && nbuf < sizeof buf - 1)
				buf[nbuf++] = next();
		/* out of range */
		} else if (hexval(c) < 0 || hexval(c) >= base) {
			lfatal(curloc, "Integer digit '%c' outside of base %d", c, base);
		/* just a number */
		} else {
			buf[nbuf++] = c;
		}
	}
	buf[nbuf] = '\0';

	/* we only support base 10 floats */
	if (isfloat) {
		if (base != 10)
			lfatal(curloc, "%s is not a valid floating point value", buf);
		t = mktok(Tfloatlit);
		t->id = strdupn(&fbuf[start], fidx - start);
		t->fltval = strtod(buf, &endp);
		if (endp == buf)
			lfatal(curloc, "%s is not a valid floating point value", buf);
	} else {
		t = mktok(Tintlit);
		t->id = strdupn(&fbuf[start], fidx - start);
		t->intval = strtoull(buf, &endp, base);
		if (endp == buf)
			lfatal(curloc, "%s is not a valid integer value", buf);
			
		/* check suffixes:
		 *   u -> unsigned
		 *   l -> 64 bit
		 *   i -> 32 bit
		 *   w -> 16 bit
		 *   b -> 8 bit
		 */
		unsignedval = 0;
nextsuffix
:
		switch (peek()) {
		case 'u':
			if (unsignedval == 1)
				lfatal(curloc, "Duplicate 'u' integer specifier");
			next();
			unsignedval = 1;
			goto nextsuffix;
		case 'l':
			next();
			if (unsignedval)
				t->inttype = Tyuint64;
			else
				t->inttype = Tyint64;
			break;
		case 'i':
			next();
			if (unsignedval)
				t->inttype = Tyuint32;
			else
				t->inttype = Tyint32;
			break;
		case 's':
			next();
			if (unsignedval)
				t->inttype = Tyuint16;
			else
				t->inttype = Tyint16;
			break;
		case 'b':
			next();
			if (unsignedval)
				t->inttype = Tyuint8;
			else
				t->inttype = Tyint8;
			break;
		default:
			if (unsignedval)
				lfatal(
						curloc, "Unrecognized character int type specifier after 'u'");
			break;
		}
	}

	return t;
}

static Tok *
numlit(void)
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
		else {
			unget();
			t = number(10);
		}
	} else {
		t = number(10);
	}

	return t;
}

static Tok *
typaram(void)
{
	Tok *t;
	char buf[1024];

	t = NULL;
	if (!match('@'))
		return NULL;
	if (!identstr(buf, 1024))
		return NULL;
	t = mktok(Ttyparam);
	t->id = strdup(buf);
	return t;
}

static Tok *
toknext(void)
{
	Tok *t;
	int c;

	eatspace();
	c = peek();
	if (c == End) {
		t = mktok(0);
	} else if (c == '\n') {
		curloc.line++;
		next();
		t = mktok(Tendln);
	} else if (isalpha(c) || c == '_' || c == '$') {
		t = kwident();
	} else if (c == '"') {
		t = strlit();
	} else if (c == '\'') {
		t = charlit();
	} else if (isdigit(c)) {
		t = numlit();
	} else if (c == '@') {
		t = typaram();
	} else {
		t = oper();
	}

	if (!t)
		lfatal(curloc, "Unable to parse token starting with %c", c);
	return t;
}

void
tokinit(char *file)
{
	int fd;
	int n;
	int nread;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Unable to open file %s\n", file);
		exit(1);
	}

	nread = 0;
	fbuf = malloc(4096);
	while (1) {
		n = read(fd, fbuf + nread, 4096);
		if (n < 0)
			fatal(0, 0, "Error reading file %s", file);
		if (n == 0)
			break;
		if (!fbuf)
			die("Out of memory reading %s", file);
		nread += n;
		fbuf = xrealloc(fbuf, nread + 4096);
	}

	fbufsz = nread;
	curloc.line = 1;
	curloc.file = 0;
	close(fd);
	if(fbufsz > 2 && fbuf[0] == '#' && fbuf[1] == '!') {
		for (fidx = 0; fidx < fbufsz; fidx++)  {
			if(fbuf[fidx] == '\n') {
				curloc.line++;
				fidx++;
				break;
			}
		}
	}
	filename = strdup(file);
}

/* Interface to yacc */
int
yylex(void)
{
	curtok = toknext();
	yylval.tok = curtok;
	return curtok->type;
}

void
yyerror(const char *s)
{
	fprintf(stderr, "%s:%d: %s", filename, curloc.line, s);
	if (curtok->id)
		fprintf(stderr, " near \"%s\"", curtok->id);
	fprintf(stderr, "\n");
	exit(1);
}
