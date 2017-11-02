from sys import stdin, stderr, stdout, argv
from re import match, sub

docs = {}

for line in open(argv[1]):
    res = match('^\t\(([^,]*), (.*)\),$', line)
    if res:
        fullname = res.group(1)
        docs[fullname.split('.')[-1]] = eval(res.group(2)).decode('utf-8')

translate_name = {
    'name': None,
    'delete_': 'delete',
    'union_': 'union',
    'operator[]': '__getitem__',
    'operator+': '__add__',
    'operator-': '__sub__',
    'operator*': '__mul__',
    'operator/': '__div__',
    'operator%': '__mod__',
    'operator&&': 'and_',
    'operator||': 'or_',
    'operator==': '__eq__',
    'operator!=': '__ne__',
    'operator>': '__gt__',
    'operator>=': '__ge__',
    'operator<': '__lt__',
    'operator<=': '__le__',
    'operator!': 'not_',
    'default_': 'default',
    'array': None,
    'desc': None,
    'asc': None,
    'maxval': None,
    'minval': None,
    'january': None,
    'february': None,
    'march': None,
    'april': None,
    'may': None,
    'june': None,
    'july': None,
    'august': None,
    'september': None,
    'october': None,
    'november': None,
    'december': None,
    'monday': None,
    'tuesday': None,
    'wednesday': None,
    'thursday': None,
    'friday': None,
    'saturday': None,
    'sunday': None,
}

def print_docs(name, line):
    py_name = translate_name.get(name, name)
    if py_name in docs:
        indent = match("^( *)", line).group(1)
        stdout.write('\n')
        # TODO: convert the examples to C++
        for line in docs[py_name].split('\n'):
            stdout.write(indent + "// " + line + '\n')
    elif py_name:
        stderr.write('Warning: no docs for ' + py_name + ': ' + line)

stdout.write('// Contains documentation copied as-is from the Python driver')

for line in stdin:
    res = match("^ *CO?[0-9_]+\(([^,)]+)|extern Query (\w+)|^ *// *(\$)doc\((\w+)\) *$", line)
    if res:
        name = res.group(1) or res.group(2) or res.group(4)
        print_docs(name, line)
        if not res.group(3):
            stdout.write(line)
    else:
        stdout.write(line)

