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

/* string tables */
static char *insnfmt[] = {
#define Insn(val, gasfmt, p9fmt, use, def) p9fmt,
#include "insns.def"
#undef Insn
};

static char *regnames[] = {
#define Reg(r, gasname, p9name, mode) p9name,
#include "regs.def"
#undef Reg
};

static char* modenames[] = {
  [ModeB] = "B",
  [ModeW] = "W",
  [ModeL] = "L",
  [ModeQ] = "Q",
  [ModeF] = "S",
  [ModeD] = "D"
};

static size_t writeblob(FILE *fd, char *name, size_t off, Htab *globls, Htab *strtab, Node *blob);
static void locprint(FILE *fd, Loc *l, char spec);

static void fillglobls(Stab *st, Htab *globls)
{
    void **k;
    size_t i, nk;
    Stab *stab;
    Node *s;

    k = htkeys(st->dcl, &nk);
    for (i = 0; i < nk; i++) {
        s = htget(st->dcl, k[i]);
        htput(globls, s, asmname(s));
    }
    free(k);

    k = htkeys(st->ns, &nk);
    for (i = 0; i < nk; i++) {
        stab = htget(st->ns, k[i]);
        fillglobls(stab, globls);
    }
    free(k);
}

static void initconsts(Htab *globls)
{
    Type *ty;
    Node *name;
    Node *dcl;

    tyintptr = mktype(Zloc, Tyuint64);
    tyword = mktype(Zloc, Tyuint);
    tyvoid = mktype(Zloc, Tyvoid);

    ty = mktyfunc(Zloc, NULL, 0, mktype(Zloc, Tyvoid));
    name = mknsname(Zloc, "_rt", "abort_oob");
    dcl = mkdecl(Zloc, name, ty);
    dcl->decl.isconst = 1;
    dcl->decl.isextern = 1;
    htput(globls, dcl, asmname(dcl));

    abortoob = mkexpr(Zloc, Ovar, name, NULL);
    abortoob->expr.type = ty;
    abortoob->expr.did = dcl->decl.did;
    abortoob->expr.isconst = 1;
}

static void printmem(FILE *fd, Loc *l, char spec)
{
    if (l->type == Locmem) {
        if (l->mem.constdisp)
            fprintf(fd, "%ld", l->mem.constdisp);
    } else if (l->mem.lbldisp) {
        fprintf(fd, "%s", l->mem.lbldisp);
    }
    if (!l->mem.base || l->mem.base->reg.colour == Rrip) {
        fprintf(fd, "+0(SB)");
    } else {
        fprintf(fd, "(");
        locprint(fd, l->mem.base, 'r');
        fprintf(fd, ")");
    }
    if (l->mem.idx) {
        fprintf(fd, "(");
        locprint(fd, l->mem.idx, 'r');
        fprintf(fd, "*%d", l->mem.scale);
        fprintf(fd, ")");
    }
}

static void locprint(FILE *fd, Loc *l, char spec)
{
    spec = tolower(spec);
    assert(l->mode);
    switch (l->type) {
        case Loclitl:
            assert(spec == 'i' || spec == 'x' || spec == 'u');
            fprintf(fd, "$%s", l->lbl);
            break;
        case Loclbl:
            assert(spec == 'm' || spec == 'v' || spec == 'x');
            fprintf(fd, "%s", l->lbl);
            break;
        case Locreg:
            assert((spec == 'r' && isintmode(l->mode)) || 
                   (spec == 'f' && isfloatmode(l->mode)) ||
                   spec == 'v' ||
                   spec == 'x' ||
                   spec == 'u');
            if (l->reg.colour == Rnone)
                fprintf(fd, "%%P.%zd%s", l->reg.id, modenames[l->mode]);
            else
                fprintf(fd, "%s", regnames[l->reg.colour]);
            break;
        case Locmem:
        case Locmeml:
            assert(spec == 'm' || spec == 'v' || spec == 'x');
            printmem(fd, l, spec);
            break;
        case Loclit:
            assert(spec == 'i' || spec == 'x' || spec == 'u');
            fprintf(fd, "$%ld", l->lit);
            break;
        case Locnone:
            die("Bad location in locprint()");
            break;
    }
}

static int issubreg(Loc *a, Loc *b)
{
    return rclass(a) == rclass(b) && a->mode != b->mode;
}

static void iprintf(FILE *fd, Insn *insn)
{
    char *p;
    int i;
    int idx;

    /* x64 has a quirk; it has no movzlq because mov zero extends. This
     * means that we need to do a movl when we really want a movzlq. Since
     * we don't know the name of the reg to use, we need to sub it in when
     * writing... */
    switch (insn->op) {
        case Imovzx:
            if (insn->args[0]->mode == ModeL && insn->args[1]->mode == ModeQ) {
                if (insn->args[1]->reg.colour) {
                    insn->op = Imov;
                    insn->args[1] = coreg(insn->args[1]->reg.colour, ModeL);
                }
            }
            break;
        case Imovs:
            if (insn->args[0]->reg.colour == Rnone || insn->args[1]->reg.colour == Rnone)
                break;
            /* moving a reg to itself is dumb. */
            if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
                return;
            break;
        case Imov:
            assert(!isfloatmode(insn->args[0]->mode));
            if (insn->args[0]->type != Locreg || insn->args[1]->type != Locreg)
                break;
            if (insn->args[0]->reg.colour == Rnone || insn->args[1]->reg.colour == Rnone)
                break;
            /* if one reg is a subreg of another, we can just use the right
             * mode to move between them. */
            if (issubreg(insn->args[0], insn->args[1]))
                insn->args[0] = coreg(insn->args[0]->reg.colour, insn->args[1]->mode);
            /* moving a reg to itself is dumb. */
            if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
                return;
            break;
        default:
            break;
    }
    p = insnfmt[insn->op];
    i = 0; /* NB: this is 1 based indexing */
    for (; *p; p++) {
        if (*p !=  '%') {
            fputc(*p, fd);
            continue;
        }

        /* %-formating */
        p++;
        idx = i;
again:
        switch (*p) {
            case '\0':
                goto done; /* skip the final p++ */
                break;
            case 'R': /* int register */
            case 'F': /* float register */
            case 'M': /* memory */
            case 'I': /* imm */
            case 'V': /* reg/mem */
            case 'U': /* reg/imm */
            case 'X': /* reg/mem/imm */
                locprint(fd, insn->args[idx], *p);
                i++;
                break;
            case 'T':
                fputs(modenames[insn->args[idx]->mode], fd);
                break;
            default:
                /* the  asm description uses 1-based indexing, so that 0
                 * can be used as a sentinel. */
                if (!isdigit(*p))
                    die("Invalid %%-specifier '%c'", *p);
                idx = strtol(p, &p, 10) - 1;
                goto again;
                break;
        }
    }
done:
    return;
}


static size_t writebytes(FILE *fd, char *name, size_t off, char *p, size_t sz)
{
    size_t i, len;

    if (sz == 0)
        fprintf(fd, "DATA %s<>+%zd(SB)/%zd,$\"\"\n", name, off, sz);
    for (i = 0; i < sz; i++) {
        len = min(sz - i, 8);
        if (i % 8 == 0)
            fprintf(fd, "DATA %s<>+%zd(SB)/%zd,$\"", name, off + i, len);
        if (p[i] == '"' || p[i] == '\\')
            fprintf(fd, "\\");
        if (isprint(p[i]))
            fprintf(fd, "%c", p[i]);
        else
            fprintf(fd, "\\%03o", (uint8_t)p[i] & 0xff);
        /* line wrapping for readability */
        if (i % 8 == 7 || i == sz - 1)
            fprintf(fd, "\"\n");
    }

    return sz;
}

static size_t writelit(FILE *fd, char *name, size_t off, Htab *strtab, Node *v, Type *ty)
{
    char buf[128];
    char *lbl;
    size_t sz;
    union {
        float fv;
        double dv;
        uint64_t qv;
        uint32_t lv;
    } u;

    assert(v->type == Nlit);
    sz = tysize(ty);
    switch (v->lit.littype) {
        case Lint:
            fprintf(fd, "DATA %s+%zd(SB)/%zd,$%lld\n", name, off, sz, v->lit.intval);
            break;
        case Lbool:
            fprintf(fd, "DATA %s+%zd(SB)/%zd,$%d\n", name, off, sz, v->lit.boolval);
            break;
        case Lchr:
            fprintf(fd, "DATA %s+%zd(SB)/%zd,$%d\n", name, off, sz, v->lit.chrval);
            break;
        case Lflt:
            if (tybase(v->lit.type)->type == Tyflt32) {
                u.fv = v->lit.fltval;
                fprintf(fd, "DATA %s+%zd(SB)/%zd, $0x%llx\n", name, off, sz, (vlong)u.lv);
            } else if (tybase(v->lit.type)->type == Tyflt64) {
                u.dv = v->lit.fltval;
                fprintf(fd, "DATA %s+%zd(SB)/%zd, $0x%llx\n", name, off, sz, (vlong)u.qv);
            }
            break;
        case Lstr:
           if (hthas(strtab, &v->lit.strval)) {
               lbl = htget(strtab, &v->lit.strval);
           } else {
               lbl = genlblstr(buf, sizeof buf);
               htput(strtab, &v->lit.strval, strdup(lbl));
           }
           fprintf(fd, "DATA %s+%zd(SB)/8,$%s<>+0(SB)\n", name, off, lbl);
           fprintf(fd, "DATA %s+%zd(SB)/8,$%zd\n", name, off+8, v->lit.strval.len);
           break;
        case Lfunc:
            die("Generating this shit ain't ready yet ");
            break;
        case Llbl:
            die("Can't generate literal labels, ffs. They're not data.");
            break;
    }
    return sz;
}

static size_t writepad(FILE *fd, char *name, size_t off, size_t sz)
{
    size_t n;

    assert((ssize_t)sz >= 0);
    while (sz > 0) {
        n = min(sz, 8);
        fprintf(fd, "DATA %s+%zd(SB)/%zd,$0\n", name, off, n);
        sz -= n;
    }
    return sz;
}

static size_t getintlit(Node *n, char *failmsg)
{
    if (exprop(n) != Olit)
        fatal(n, "%s", failmsg);
    n = n->expr.args[0];
    if (n->lit.littype != Lint)
        fatal(n, "%s", failmsg);
    return n->lit.intval;
}

static size_t writeslice(FILE *fd, char *name, size_t off, Htab *globls, Htab *strtab, Node *n)
{
    Node *base, *lo, *hi;
    ssize_t loval, hival, sz;
    char *lbl;

    base = n->expr.args[0];
    lo = n->expr.args[1];
    hi = n->expr.args[2];

    /* by this point, all slicing operations should have had their bases
     * pulled out, and we should have vars with their pseudo-decls in their
     * place */
    if (exprop(base) != Ovar || !base->expr.isconst)
        fatal(base, "slice base is not a constant value");
    loval = getintlit(lo, "lower bound in slice is not constant literal");
    hival = getintlit(hi, "upper bound in slice is not constant literal");
    sz = tysize(tybase(exprtype(base))->sub[0]);

    lbl = htget(globls, base);
    fprintf(fd, "DATA %s+%zd(SB)/8,$%s+%zd(SB)\n", name, off, lbl, loval*sz);
    fprintf(fd, "DATA %s+%zd(SB)/8,$%zd\n", name, off, (hival - loval));
    return size(n);
}

static size_t writestruct(FILE *fd, char *name, size_t off, Htab *globls, Htab *strtab, Node *n)
{
    Type *t;
    Node **dcl;
    int found;
    size_t i, j;
    size_t pad, start, end;
    size_t ndcl;

    start = off;
    t = tybase(exprtype(n));
    assert(t->type == Tystruct);
    dcl = t->sdecls;
    ndcl = t->nmemb;
    for (i = 0; i < ndcl; i++) {
        pad = alignto(off, decltype(dcl[i]));
        off += writepad(fd, name, off, pad - off);
        found = 0;
        for (j = 0; j < n->expr.nargs; j++)
            if (!strcmp(namestr(n->expr.args[j]->expr.idx), declname(dcl[i]))) {
                found = 1;
                off += writeblob(fd, name, off, globls, strtab, n->expr.args[j]);
            }
        if (!found)
            off += writepad(fd, name, off, size(dcl[i]));
    }
    end = alignto(off, t);
    off += writepad(fd, name, off, end - off);
    return off - start;
}
static size_t writeblob(FILE *fd, char *name, size_t off, Htab *globls, Htab *strtab, Node *n)
{
    size_t i, sz;

    switch(exprop(n)) {
        case Otup:
        case Oarr:
            sz = 0;
            for (i = 0; i < n->expr.nargs; i++)
                sz += writeblob(fd, name, off, globls, strtab, n->expr.args[i]);
            break;
        case Ostruct:
            sz = writestruct(fd, name, off, globls, strtab, n);
            break;
        case Olit:
            sz = writelit(fd, name, off, strtab, n->expr.args[0], exprtype(n));
            break;
        case Oslice:
            sz = writeslice(fd, name, off, globls, strtab, n);
            break;
        default:
            dump(n, stdout);
            die("Nonliteral initializer for global");
            break;
    }
    return sz;
}

static void genstrings(FILE *fd, Htab *strtab)
{
    void **k;
    char *lbl;
    Str *s;
    size_t i, nk;

    k = htkeys(strtab, &nk);
    for (i = 0; i < nk; i++) {
        s = k[i];
        lbl = htget(strtab, k[i]);
        fprintf(fd, "GLOBL %s<>+0(SB),$%lld\n", lbl, (vlong)s->len);
        writebytes(fd, lbl, 0, s->buf, s->len);
    }
}


static void writeasm(FILE *fd, Isel *s, Func *fn)
{
    size_t i, j;
    char *hidden;

    hidden = "";
    if (fn->isexport || streq(fn->name, Symprefix "main"))
        hidden = "";
    fprintf(fd, "TEXT %s%s+0(SB),$%zd\n", fn->name, hidden, fn->stksz);
    for (j = 0; j < s->cfg->nbb; j++) {
        if (!s->bb[j])
            continue;
        for (i = 0; i < s->bb[j]->nlbls; i++)
            fprintf(fd, "%s:\n", s->bb[j]->lbls[i]);
        for (i = 0; i < s->bb[j]->ni; i++)
            iprintf(fd, s->bb[j]->il[i]);
    }
}

static void genblob(FILE *fd, Node *blob, Htab *globls, Htab *strtab)
{
    char *lbl;

    /* lits and such also get wrapped in decls */
    assert(blob->type == Ndecl);

    lbl = htget(globls, blob);
    fprintf(fd, "GLOBL %s+0(SB),$%zd\n", lbl, size(blob));
    if (blob->decl.init)
        writeblob(fd, lbl, 0, globls, strtab, blob->decl.init);
    else
        writepad(fd, lbl, 0, size(blob));
}

/* genfunc requires all nodes in 'nl' to map cleanly to operations that are
 * natively supported, as promised in the output of reduce().  No 64-bit
 * operations on x32, no structures, and so on. */
static void genfunc(FILE *fd, Func *fn, Htab *globls, Htab *strtab)
{
    Isel is = {0,};

    is.reglocs = mkht(varhash, vareq);
    is.stkoff = fn->stkoff;
    is.globls = globls;
    is.ret = fn->ret;
    is.cfg = fn->cfg;

    selfunc(&is, fn, globls, strtab);
    if (debugopt['i'])
        writeasm(stdout, &is, fn);
    writeasm(fd, &is, fn);
}

void genp9(Node *file, char *out)
{
    Htab *globls, *strtab;
    Node *n, **blob;
    Func **fn;
    size_t nfn, nblob;
    size_t i;
    FILE *fd;

    /* ensure that all physical registers have a loc created before any
     * other locs, so that locmap[Physreg] maps to the Loc for the physreg
     * in question */
    for (i = 0; i < Nreg; i++)
        locphysreg(i);

    fn = NULL;
    nfn = 0;
    blob = NULL;
    nblob = 0;
    globls = mkht(varhash, vareq);
    initconsts(globls);

    /* We need to define all global variables before use */
    fillglobls(file->file.globls, globls);

    pushstab(file->file.globls);
    for (i = 0; i < file->file.nstmts; i++) {
        n = file->file.stmts[i];
        switch (n->type) {
            case Nuse: /* nothing to do */ 
            case Nimpl:
                break;
            case Ndecl:
                simpglobl(n, globls, &fn, &nfn, &blob, &nblob);
                break;
            default:
                die("Bad node %s in toplevel", nodestr(n->type));
                break;
        }
    }
    popstab();

    fd = fopen(out, "w");
    if (!fd)
        die("Couldn't open fd %s", out);

    strtab = mkht(strlithash, strliteq);
    for (i = 0; i < nblob; i++)
        genblob(fd, blob[i], globls, strtab);
    for (i = 0; i < nfn; i++)
        genfunc(fd, fn[i], globls, strtab);
    genstrings(fd, strtab);
    fclose(fd);
}
