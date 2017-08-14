import sys
import re
import os
import traceback
import pycparser
import StringIO
from pycparser import c_parser, c_ast
from pycparser.plyparser import Coord
#from pycparser import c_parser, c_ast, parse_file

#from pycparser.plyparser import Coord

ctypes = {}
myrtypes = {
    'void':                     'void',
    'char':                     'byte',
    'short':                    'int16',
    'int':                      'int',
    'long':                     'int64',
    'long long':                'int64',

    'unsigned char':            'byte',
    'unsigned short':           'uint16',
    'unsigned':                 'uint',
    'unsigned int':             'uint',
    'unsigned long':            'uint64',
    'unsigned long long':       'uint64',

    'unsigned short int':       'uint16',
    'unsigned long int':        'uint64',
    'unsigned long long int':   'uint64',

    'short int':                'int16',
    'long int':                 'int64',
    'long long int':            'int64',

    'int8_t':                   'byte',
    'int16_t':                  'int16',
    'int32_t':                  'int32',
    'int64_t':                  'int64',

    'uint8_t':                  'byte',
    'uint16_t':                 'uint16',
    'uint32_t':                 'uint32',
    'uint64_t':                 'uint64',

    'size_t':                   'size',
    'ssize_t':                  'size', # myrddin sizes are signed

    # kernel-defined types?
    'u_char':                   'byte',
    'u_short':                  'uint16',
    'u_int':                    'uint',
    'u_long':                   'uint64',

    'u_int8':                   'uint8',
    'u_int16':                  'uint16',
    'u_int32':                  'uint32',
    'u_int64':                  'uint64',

    'int8':                     'int8',
    'int16':                    'int16',
    'int32':                    'int32',
    'int64':                    'int64',

    'i8':                       'int8',
    'i16':                      'int16',
    'i32':                      'int32',
    'i64':                      'int64',

    'u8':                       'uint8',
    'u16':                      'uint16',
    'u32':                      'uint32',
    'u64':                      'uint64',

    '__int64_t':                'int64',

    'char':                     'byte',
    'short':                    'int16',
    'int':                      'int',
    'long':                     'int64',

    'caddr_t':                  'void#',
}

have = set()

#valid for all unixes we support...
sizes = {
        'char':                     1,
        'short':                    2,
        'int':                      4,
        'long':                     8,
        'long int':                 8,
        'long long':                8,
        'long long int':            8,
        'unsigned char':            1,
        'unsigned short':           2,
        'unsigned short int':       2,
        'unsigned int':             4,
        'unsigned long':            8,
        'unsigned long int':        8,
        'unsigned long long':       8,
        'unsigned long long int':   8,
        'void*':                    1,
}

def fullydefined(t):
    return type(t) == c_ast.Struct and t.decls

def collectdef(n, t):
    if t in [c_ast.Decl, c_ast.TypeDecl] and type(n.type) == c_ast.Struct:
        if fullydefined(n.type):
            ctypes[n.type.name] = n.type
    elif t == c_ast.Typedef:
        # typedef struct foo foo is valid.
        if n.name not in ctypes:
            ctypes[n.name] = n
        # as is 'typedef struct foo { ... } bar'
        if type(n.type) is c_ast.TypeDecl:
            if type(n.type.type) is c_ast.Struct:
                collectdef(n.type, type(n.type))

def collect(ast):
    for n in ast.ext:
        collectdef(n, type(n))

def typename(t):
    if type(t) == c_ast.Struct:
        return t.name
    elif type(t) == c_ast.IdentifierType:
        return ' '.join(t.names)
    elif type(t) == c_ast.Typedef:
        return t.name
    elif type(t) == c_ast.Struct:
        return t.name
    elif type(t) == c_ast.TypeDecl:
        return typename(t.type)
    elif type(t) == c_ast.TypeDecl:
        return typename(t.type)
    else:
        t.show()
        raise ValueError('unknown type')

def deprefix(decls):
    pfx = None
    for d in decls:
        name = d.name
        if not name:
            return
        if name.startswith('__'):
            name = name[2:]
        sp = name.split('_', 1)
        if len(sp) != 2:
            return
        if pfx and sp[0] != pfx:
            return
        pfx = sp[0]

    for d in decls:
        if d.name.startswith('__'):
            d.name = d.name[2:]
        (hd, tl) = d.name.split('_', 1)
        d.name = tl

def myrname(n):
    namemap = {
        'acl_type_t':           'acltype',
	'acl_tag_t':	        'acltag',
	'acl_perm_t':	        'aclperm',
	'acl_entry_type_t':     'aclenttype',
	'mode_t':               'filemode',
	'ssize_t':              'size',
	'__socklen_t':          'int32',
	'socklen_t':            'int32',
	'fd_set':               'fdset',
	'__ucontext':           'ucontext',
	'cap_rights_t':         'caprights',
	'stack_t':              'sigstack',
        'thr_param':            'thrparam',
        'stat':                 'statbuf',
        'nstat':                'statbuf',
        'newstat':              'statbuf',
    }
    if n in namemap:
        return namemap[n]
    if n.endswith('_t'):
        n = n[:-2]
    if n.startswith('__'):
        n = n[2:]
    return n

def tysize(t):
    if type(t) == c_ast.Typename:
        return tysize(t.type)
    if type(t) == c_ast.TypeDecl:
        return tysize(t.type)
    if type(t) == c_ast.Typedef:
        return tysize(t.type)
    if type(t) == c_ast.Typedef:
        return tysize(t.type)
    if type(t) == c_ast.PtrDecl:
        return sizes['void*']
    if type(t) == c_ast.ArrayDecl:
        return tysize(t.type) * fold(t.dim)
    elif type(t) == c_ast.IdentifierType:
        name = ' '.join(t.names)
        if name in ctypes:
            return tysize(ctypes[name])
        else:
            return sizes[name]
    else:
        raise ValueError('unsupported type' + str(t))

def fold(expr):
    try:
        if type(expr) is c_ast.BinaryOp:
            if expr.op == '+':  return fold(expr.left) + fold(expr.right)
            if expr.op == '-':  return fold(expr.left) - fold(expr.right)
            if expr.op == '*':  return fold(expr.left) * fold(expr.right)
            if expr.op == '/':  return fold(expr.left) / fold(expr.right)
            if expr.op == '%':  return fold(expr.left) % fold(expr.right)
            if expr.op == '&':  return fold(expr.left) & fold(expr.right)
            if expr.op == '|':  return fold(expr.left) | fold(expr.right)
            if expr.op == '^':  return fold(expr.left) >> fold(expr.right)
            if expr.op == '<<': return fold(expr.left) << fold(expr.right)
            if expr.op == '>>': return fold(expr.left) >> fold(expr.right)
        elif type(expr) is c_ast.UnaryOp:
            if expr.op == '+':  return fold(expr.expr)
            if expr.op == '-':  return -fold(expr.expr)
            if expr.op == 'sizeof':
                return tysize(expr.expr)
        elif type(expr) is c_ast.Cast:
            return fold(expr.expr)
        else:
            return int(expr.value)
    except Exception:
        raise

def emitstruct(t, defname):
    fields = []
    if t.name:
        name = myrname(t.name)
    else:
        name = defname
    deprefix(t.decls)
    for d in t.decls:
        if not d.name:
            raise ValueError('null name in type')
        etype = emit(d.type)
        fname = d.name
        if fname == 'type':
            fname = 'kind'
        fields.append('{}\t: {}'.format(fname, etype))
    structs.write('type {} = struct\n\t{}\n\n;;\n\n'.format(name, '\n\t'.join(fields)))
    return name

def emit(t, defname=None):
    if type(t) == c_ast.Struct:
        if t.name:
            if myrname(t.name) in have:
                return t.name
            # linked lists can be a problem...
            have.add(myrname(t.name))
        if not t.decls:
            t = ctypes[t.name]
        return emitstruct(t, defname)
    elif type(t) == c_ast.IdentifierType:
        name = ' '.join(t.names)
        if name in myrtypes:
            return myrtypes[' '.join(t.names)]
        return myrtype(name, defname)
    elif type(t) == c_ast.Typedef:
        if t.name in myrtypes:
            return myrtypes[t.name]
        name = myrname(t.name)
        if name in have:
            return name
        # fold chains of typedefs
        sub = emit(t.type, name)
        if defname:
            return myrname(sub)
        else:
            if name != sub and name not in have:
                have.add(name)
                defs.write('type {} = {}\n'.format(name, sub))
            return name
    elif type(t) == c_ast.TypeDecl:
        return emit(t.type, defname)
    elif type(t) == c_ast.ArrayDecl:
        ret = emit(t.type)
        return '{}[{}]'.format(ret, fold(t.dim))
    elif type(t) == c_ast.PtrDecl:
        # Myrddin functions aren't easy to translate to C.
        if type(t.type) == c_ast.FuncDecl:
            return 'void#'
        return emit(t.type) + '#'
    elif type(t) == c_ast.Union:
        raise ValueError('unions unsupported')
    else:
        t.show()
        raise ValueError('unknown type')


def myrtype(name, defname=None):
    if name is None:
        return None
    if myrname(name) in myrtypes:
        return myrtypes[myrname(name)]

    t = ctypes[name]
    gen = emit(t, defname)
    have.add(myrname(gen))
    return gen

if __name__ == '__main__':
    structs = StringIO.StringIO()
    defs = StringIO.StringIO()

    prehdr = '''
#define __builtin_va_list int
#define __inline
#define __inline__
#define __restrict
#define __attribute__(_)
#define __asm(x)
#define __asm__(x)
#define __extension__
#define __signed__
#define __extern_inline__
#define volatile(_) /* catches '__asm volatile(asm)' on openbsd */

#include <sys/types.h>
#include <sys/cdefs.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#include <utime.h>
#include <ucontext.h>
#include <linux/futex.h>
#endif
    '''
    includes = []
    # A bunch of these headers are system-only. We wouldn't
    # care, but they also try to prevent people from depending
    # on them accidentally. As a result, let's just try including
    # each one ahead of time, and see if it works.
    #
    # if so, we generate a mega-header with all the types.
    syshdr = []
    if os.path.exists("/usr/include/sys"):
        syshdr = ["sys/" + h for h in os.listdir('/usr/include/sys')]
    if os.path.exists("/usr/include/linux"):
        syshdr = syshdr + ["linux/" + h for h in os.listdir("/usr/include/linux")]
    # it seems that sys headers can also be stashed here. Shit.
    if os.path.exists("/usr/include/x86_64-linux-gnu/sys"):
        syshdr = syshdr + ["linux/" + h for h in os.listdir("/usr/include/linux")]
    for hdr in syshdr:
        print 'trying {}'.format(hdr)
        if not hdr.endswith(".h"):
            continue
        try:
            include = prehdr + '\n#include <{}>'.format(hdr)
            with open('/tmp/myr-includes.h', 'w') as f:
                f.write(include)
            pycparser.parse_file('/tmp/myr-includes.h', use_cpp=True)
            includes.append('#include <{}>'.format(hdr))
        except Exception:
            print '...skip'

    print('importing' + ' '.join(includes))
    with open('/tmp/myr-includes.h', 'w') as f:
        f.write(prehdr + '\n' + '\n'.join(includes))
    with open('want.txt') as f:
        want = f.read().split()
    with open('have.txt') as f:
        have |= set(f.read().split())
        for sz in ['', 8, 16, 32, 64]:
            have.add('int{}'.format(sz))
            have.add('uint{}'.format(sz))
    ast = pycparser.parse_file('/tmp/myr-includes.h', use_cpp=True)
    collect(ast)
    for w in want:
        try:
            myrtype(w)
        except (ValueError,KeyError) as e:
            print('unconvertable type {}: add a manual override: {}'.format(w, e))
    f = 'gentypes+{}-{}.frag'.format(sys.argv[1], sys.argv[2])
    with open(f, 'w') as f:
        f.write(defs.getvalue())
        f.write('\n')
        f.write(structs.getvalue())
