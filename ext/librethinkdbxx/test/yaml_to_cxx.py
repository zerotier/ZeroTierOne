from sys import argv, stderr, float_info
import sys
from upstream.parsePolyglot import parseYAML
from os import walk
from os.path import join
from re import sub, match, split, DOTALL
from collections import namedtuple
import ast

verbosity = 1

try:
    NameConstant = ast.NameConstant
except:
    NameConstant = lambda a: a

class Discard(Exception):
    pass

class Unhandled(Exception):
    pass

failed = False

Ctx = namedtuple('Ctx', ['vars', 'context', 'type'])

def convert(python, prec, file, type):
    try:
        expr = ast.parse(python, filename=file, mode='eval').body
        cxx = to_cxx(expr, prec, Ctx(vars=[], type=type, context=None))
        return sub('" \\+ "', '', cxx)
    except (Unhandled, AssertionError):
        print("While translating: " + python, file=stderr)
        raise
    except SyntaxError as e:
        raise Unhandled("syntax error: " + str(e) + ": " + repr(python))

def py_str(py):
    def maybe_unstr(s):
        if '(' in s:
            return s
        else:
            return repr(s)
    if type(py) is dict:
        return '{' + ', '.join([repr(k) + ': ' + maybe_str(py[k]) for k in py]) + '}'
    if not isinstance(py, "".__class__):
            return repr(py)
    return py

def rename(id):
    return {
        'R::default': 'R::default_',
        'default': 'default_',
        'R::do': 'R::do_',
        'do': 'do_',
        'union': 'union_',
        'False': 'false',
        'True': 'true',
        'xrange': 'R::range',
        'None': 'R::Nil()',
        'null': 'R::Nil()',
        'delete': 'delete_',
        'float': 'double',
        'int_cmp': 'int',
        'float_cmp': 'double',
        'range': 'R::range',
        'list': '',
        'R::union': 'R::union_'
    }.get(id, id)

def to_cxx_str(expr):
    if type(expr) is ast.Str:
        return string(expr.s)
    if type(expr) is ast.Num:
        return string(str(expr.n))
    if 'frozenset' in ast.dump(expr):
        raise Discard("frozenset not supported")
    if type(expr) is ast.Name:
        raise Discard("dict with non-string key")
    raise Unhandled("not string expr: " + ast.dump(expr))

def is_null(expr):
    return (type(expr) is ast.Name and expr.id in ['None', 'null']
            or type(expr) is NameConstant and expr.value == None)

def is_bool(expr):
    return (type(expr) is ast.Name and expr.id in ['true', 'false', 'True', 'False']
            or type(expr) is NameConstant and expr.value in [True, False])

def to_cxx_expr(expr, prec, ctx):
    if ctx.type == 'query':
        if type(expr) in [ast.Str, ast.Num] or is_null(expr) or is_bool(expr):
            return "R::expr(" + to_cxx(expr, 17, ctx) + ")"
    return to_cxx(expr, prec, ctx)

def to_cxx(expr, prec, ctx, parentType=None):
    context = ctx.context
    ctx = Ctx(vars=ctx.vars, type=ctx.type, context=None)
    try:
        t = type(expr)
        if t == ast.Num:
            if abs(expr.n) > 4503599627370496:
                f = repr(expr.n)
                if "e" in f:
                    return f
                else:
                    return f + ".0"
            else:
                return repr(expr.n)
        elif t == ast.Call:
            #assert not expr.kwargs
            #assert not expr.starargs
            return to_cxx(expr.func, 2, ctx_set(ctx, context='function')) + to_args(expr.func, expr.args, expr.keywords, ctx)
        elif t == ast.Attribute:
            if type(expr.value) is ast.Name:
                if expr.value.id == 'r':
                    if expr.attr == 'error' and context != 'function':
                        return "R::error()"
                    if expr.attr == 'binary':
                        if ctx.type == 'query':
                            return 'R::binary'
                        else:
                            return 'R::Binary'
                    return rename("R::" + expr.attr)
                elif expr.value.id == 'datetime':
                    if expr.attr == 'fromtimestamp':
                        return "R::Time"
                    elif expr.attr == 'now':
                        return "R::Time::now"
            if expr.attr == 'RqlTzinfo':
                return 'R::Time::parse_utc_offset'
            if expr.attr in ['encode', 'close']:
                raise Discard(expr.attr + " not supported")
            return to_cxx_expr(expr.value, 2, ctx) + "." + rename(expr.attr)
        elif t == ast.Name:
            if expr.id in ['frozenset']:
                raise Discard("frozenset not supported")
            elif expr.id in ctx.vars:
                if ctx.type == 'query':
                    return parens(prec, 3, "*" + expr.id)
                else:
                    return expr.id
            elif (expr.id == 'range' or expr.id == 'xrange') and ctx.type != 'query':
                return 'array_range'
            elif expr.id == 'nil' and ctx.type == 'query':
                return 'R::expr(nil)'
            return rename(expr.id)
        elif t == NameConstant:
            if expr.value == True:
                return "true"
            elif expr.value == False:
                return "false"
            elif expr.value == None:
                return "R::Nil()"
            else:
                raise Unhandled("constant: " + repr(expr.value))
        elif t == ast.Subscript:
            st = type(expr.slice)
            if st == ast.Index:
                return to_cxx(expr.value, 2, ctx) + "[" + to_cxx(expr.slice.value, 17, ctx) + "]"
            if st == ast.Slice:
                assert not expr.slice.step
                if not expr.slice.upper:
                    return to_cxx(expr.value, 2, ctx) + ".slice(" + to_cxx(expr.slice.lower, 17, ctx) + ")"
                if not expr.slice.lower:
                    return to_cxx(expr.value, 2, ctx) + ".limit(" + to_cxx(expr.slice.upper, 17, ctx) + ")"
                return to_cxx(expr.value, 2, ctx) + ".slice(" + to_cxx(expr.slice.lower, 17, ctx) + ", " + to_cxx(expr.slice.upper, 17, ctx) + ")"
            else:
                raise Unhandled("slice type: " + repr(st))
        elif t == ast.Dict:
            if ctx.type == 'query':
                return "R::object(" + ', '.join([to_cxx(k, 17, ctx) + ", " + to_cxx(v, 17, ctx) for k, v in zip(expr.keys, expr.values)]) + ")"
            else:
                return "R::Object{" + ', '.join(["{" + to_cxx_str(k) + ", " + to_cxx(v, 17, ctx) + "}" for k, v in zip(expr.keys, expr.values)]) + "}"
        elif t == ast.Str:
            return string(expr.s, ctx)
        elif t == ast.List:
            if ctx.type == 'query':
                return "R::array(" + ', '.join([to_cxx(el, 17, ctx) for el in expr.elts]) + ")"
            else:
                if parentType == ast.List:
                    return "{ R::Array{" + ', '.join([to_cxx(el, 17, ctx, t) for el in expr.elts]) + "} }"
                else:
                    return "R::Array{" + ', '.join([to_cxx(el, 17, ctx, t) for el in expr.elts]) + "}"
        elif t == ast.Lambda:
            assert not expr.args.vararg
            assert not expr.args.kwarg
            ctx = ctx_set(ctx, vars = ctx.vars + [arg.arg for arg in expr.args.args])
            return "[=](" + ', '.join(['R::Var ' + arg.arg for arg in expr.args.args]) + "){ return " + to_cxx_expr(expr.body, 17, ctx_set(ctx, type='query')) + "; }"
        elif t == ast.BinOp:
            if type(expr.op) is ast.Mult and type(expr.left) is ast.Str:
                return "repeat(" + to_cxx(expr.left, 17, ctx) + ", " + to_cxx(expr.right, 17, ctx) + ")"
            ll = type(expr.left) is ast.List or type(expr.left) is ast.ListComp
            rl = type(expr.right) is ast.List or type(expr.right) is ast.ListComp
            op, op_prec = convert_op(expr.op)
            if type(expr.op) is ast.Add and ll and rl:
                return "append(" + to_cxx_expr(expr.left, op_prec, ctx) + ", " + to_cxx(expr.right, op_prec, ctx) + ")"
            if op_prec:
                return parens(prec, op_prec, to_cxx_expr(expr.left, op_prec, ctx) + " " + op + " " + to_cxx(expr.right, op_prec, ctx))
            else:
                return op + "(" + to_cxx(expr.left, 17, ctx) + ", " + to_cxx(expr.right, 17, ctx) + ")"
        elif t == ast.ListComp:
            assert len(expr.generators) == 1
            assert type(expr.generators[0]) == ast.comprehension
            assert type(expr.generators[0].target) == ast.Name
            assert expr.generators[0].ifs == []
            seq = to_cxx(expr.generators[0].iter, 2, ctx)
            if ctx.type == 'query':
                var = expr.generators[0].target.id
                body = to_cxx(expr.elt, 17, ctx_set(ctx, vars = ctx.vars + [var]))
                return seq + ".map([=](R::Var " + var + "){ return " + body + "; })"
            else:
                var = expr.generators[0].target.id
                body = to_cxx(expr.elt, 17, ctx_set(ctx, vars = ctx.vars + [var]))
                # assume int
                return "array_map([=](int " + var + "){ return " + body + "; }, " + seq + ")"
        elif t == ast.Compare:
            assert len(expr.ops) == 1
            assert len(expr.comparators) == 1
            op, op_prec = convert_op(expr.ops[0])
            return parens(prec, op_prec, to_cxx_expr(expr.left, op_prec, ctx) + op + to_cxx(expr.comparators[0], op_prec, ctx))
        elif t == ast.UnaryOp:
            op, op_prec = convert_op(expr.op)
            return parens(prec, op_prec, op + to_cxx(expr.operand, op_prec, ctx))
        elif t == ast.Bytes:
            return string(expr.s, ctx)
        elif t == ast.Tuple:
            if ctx.type == 'query':
                return "R::array(" + ', '.join([to_cxx(el, 17, ctx) for el in expr.elts]) + ")"
            else:
                return "R::Array{" + ', '.join([to_cxx(el, 17, ctx) for el in expr.elts]) + "}"
        else:
            raise Unhandled('ast type: ' + repr(t) + ', fields: ' + str(expr._fields))
    except Unhandled:
        print("While translating: " + ast.dump(expr), file=stderr)
        raise

def ctx_set(ctx, context=None, vars=None, type=None):
    if context is None:
        context = ctx.context
    if vars is None:
        vars = ctx.vars
    if type is None:
        type = ctx.type
    return Ctx(vars=vars, type=type, context=context)

def convert_op(op):
    t = type(op)
    if t == ast.Add:
        return '+', 6
    if t == ast.Sub:
        return '-', 6
    if t == ast.Mod:
        return '%', 5
    if t == ast.Mult:
        return '*', 5
    if t == ast.Div:
        return '/', 5
    if t == ast.Pow:
        return 'pow', 0
    if t == ast.Eq:
        return '==', 9
    if t == ast.NotEq:
        return '!=', 9
    if t == ast.Lt:
        return '<', 8
    if t == ast.Gt:
        return '>', 8
    if t == ast.GtE:
        return '>=', 8
    if t == ast.LtE:
        return '<=', 8
    if t == ast.USub:
        return '-', 3
    if t == ast.BitAnd:
        return '&&', 13
    if t == ast.BitOr:
        return '||', 14
    if t == ast.Invert:
        return '!', 3
    else:
        raise Unhandled('op type: ' + repr(t))

def to_args(func, args, optargs, ctx):
    it = func
    while type(it) is ast.Attribute:
        it = it.value
        if type(it) is ast.Call:
            ctx = ctx_set(ctx, type='query')
            break
    if type(it) is ast.Name and it.id == 'r':
        ctx = ctx_set(ctx, type='query')
    ret = "("
    ret = ret + ', '.join([to_cxx(arg, 17, ctx) for arg in args])
    o = list(optargs)
    if o:
        out = []
        for f in o:
            out.append("{" + string(f.arg) + ", R::expr(" + to_cxx(f.value, 17, ctx) + ")}")
        if args:
            ret = ret + ", "
        ret = ret + "R::OptArgs{" + ', '.join(out) + "}"
    return ret + ")"

def string(s, ctx=None):
    was_hex = False
    wrap = ctx and ctx.type == 'string'
    if type(s) is str:
        s = s.encode('utf8')
    if type(s) is bytes:
        def string_escape(c):
            nonlocal wrap
            nonlocal was_hex
            if c == 0:
                wrap = True
            if c < 32 or c > 127 or (was_hex and chr(c) in "0123456789abcdefABCDEF"):
                was_hex = True
                return '\\x' + ('0' + hex(c)[2:])[-2:]
            was_hex = False
            if c == 34:
                return '\\"'
            if c == 92:
                return '\\\\'
            else:
                return chr(c)
    else:
        raise Unhandled("string type: " + repr(type(s)))
    e = '"' + ''.join([string_escape(c) for c in s]) + '"'
    if wrap:
        return "std::string(" + e + ", " + str(len(s)) + ")"
    return e

def parens(prec, in_prec, cxx):
    if in_prec >= prec:
        return "(" + cxx + ")"
    else:
        return cxx

print("// auto-generated by yaml_to_cxx.py from " + argv[1])
print("#include \"testlib.h\"")

indent = 0

def p(s):
    print((indent * "    ") + s);

def enter(s = ""):
    if s:
        p(s)
    global indent
    indent = indent + 1

def exit(s = ""):
    global indent
    indent = indent - 1
    if s:
        p(s)

def get(o, ks, d):
    try:
        for k in ks:
            if k in o:
                return o[k]
    except:
        pass
    return d

def python_tests(tests):
    for test in tests:
        runopts = get(test, ['runopts'], None)
        try:
            ot = py_str(get(test['ot'], ['py', 'cd'], test['ot']))
        except:
            try:
                ot = py_str(test['py']['ot'])
            except:
                ot = None
        if 'def' in test:
            py = get(test['def'], ['py', 'cd'], test['def'])
            if py and type(py) is not dict:
                yield py_str(py), None, 'def', runopts
        py = get(test, ['py', 'cd'], None)
        if py:
            if isinstance(py, "".__class__):
                yield py, ot, 'query', runopts
            elif type(py) is dict and 'cd' in py:
                yield py_str(py['cd']), ot, 'query', runopts
            else:
                for t in py:
                    yield py_str(t), ot, 'query', runopts

def maybe_discard(py, ot):
    if ot is None:
        return
    if match(".*Expected .* argument", ot):
        raise Discard("argument checks not supported")
    if match(".*argument .* must", ot):
        raise Discard("argument checks not supported")
    if match(".*infix bitwise", ot):
        raise Discard("infix bitwise not supported")
    if match(".*Object keys must be strings", ot):
        raise Discard("string object keys tests not supported")
    if match(".*Got .* argument", ot):
        raise Discard("argument checks not supported")
    if match(".*AttributeError.*", ot):
        raise Discard("attribute checks not supported, will cause a compiler error")

data = parseYAML(open(argv[1]).read())

name = sub('/', '_', argv[1].split('.')[0])

enter("void %s() {" % name)

p("enter_section(\"%s: %s\");" % (name, data['desc'].replace('"', '\\"')))

if 'table_variable_name' in data:
    for var in split(" |, ", data['table_variable_name']):
        p("temp_table %s_table;" % var)
        p("R::Term %s = %s_table.table();" % (var, var))

defined = []
for py, ot, tp, runopts in python_tests(data["tests"]):
    try:
        maybe_discard(py, ot)
        assignment = match("\\A(\\w+) *= *([^=].*)\\Z", py, DOTALL)
        if runopts:
            args = ", R::optargs(" + ', '.join(['"' + k + '", ' + convert(py_str(runopts[k]), 17, name, 'value') for k in runopts]) + ")"
        else:
            args = ''
        if assignment:
            var = assignment.group(1)
            if var == 'float_max':
                p('auto float_max = ' + repr(float_info.max) + ";")
            elif var == 'float_min':
                p('auto float_min = ' + repr(float_info.min) + ";")
            else:
                if tp == 'def' and var not in ['bad_insert', 'trows']:
                    val = convert(assignment.group(2), 15, name, 'string')
                    post = ""
                else:
                    val = convert(assignment.group(2), 15, name, 'query')
                    post = ".run(*conn" + args + ")"
                if var in defined:
                    dvar = var
                else:
                    defined.append(var);
                    dvar = "auto " + var
                p("TEST_DO(" + dvar + " = (" + val + post + "));")
        elif ot:
            p("TEST_EQ(maybe_run(%s, *conn%s), (%s));" % (convert(py, 2, name, 'query'), args, convert(ot, 17, name, 'datum')))
        else:
            p("TEST_DO(maybe_run(%s, *conn%s));" % (convert(py, 2, name, 'query'), args))
    except Discard as exc:
        if verbosity >= 1:
            print("Discarding %s (%s): %s" % (repr(py), repr(ot), str(exc)), file=stderr)
        pass
    except Unhandled as e:
        failed = True
        print(argv[1] + ": could not translate: " + str(e), file=stderr)

p("section_cleanup();")
p("exit_section();")

exit("}")

if failed:
    sys.exit(1)
