SHCC=gcc

# intercept-specific
intercept_CFLAGS = -c -fPIC -g -O2 -Wall -std=c99 -D_GNU_SOURCE -DNETCON_INTERCEPT
LIB_NAME = intercept
SHLIB_EXT=dylib
SHLIB_MAJOR = 1
SHLIB_MINOR = 8
COMMON = common
OBJS= intercept.o
SHLIB = ${LIB_NAME}.${SHLIB_EXT}.${SHLIB_MAJOR}.${SHLIB_MINOR}
SHLDFLAGS = -g -O2 -Wall -I. -nostdlib -shared
LIBS = -ldl -lc -lrt -lpthread

lib:
	${SHCC} $(intercept_CFLAGS) -I. intercept.c -o intercept.o
	${SHCC} $(intercept_CFLAGS) -I. common.c -o common.o
	${SHCC} $(intercept_CFLAGS) -I. sendfd.c -o sendfd.o
	${SHCC} $(SHLDFLAGS) intercept.o common.o sendfd.o -o libintercept.so.1.0 $(LIBS)

install:
	cp libintercept.so.1.0 /lib/libintercept.so.1.0
	ln -sf /lib/libintercept.so.1.0 /lib/libintercept
	/usr/bin/install -c netcon/intercept /usr/bin

uninstall:
	rm -r /lib/libintercept.so.1.0
	rm -r /lib/libintercept
	rm -r /usr/bin/intercept
