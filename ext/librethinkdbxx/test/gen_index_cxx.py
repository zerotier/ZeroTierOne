from sys import argv
from re import sub

print("#include \"testlib.h\"");
print("void run_upstream_tests() {")
for path in argv[1:]:
    name = sub('/', '_', path.split('.')[0])
    print("    extern void %s();" % name)
    print("    clean_slate();")
    print("    %s();" % name)
print("}")
