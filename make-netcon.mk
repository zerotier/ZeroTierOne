SHCC=gcc

# intercept-specific
intercept_CFLAGS = -c -fPIC -g -O2 -Wall -std=c99 -D_GNU_SOURCE -DNETCON_INTERCEPT
LIB_NAME = intercept
SHLIB_EXT=dylib
SHLIB_MAJOR = 1
SHLIB_MINOR = 8
COMMON = common
OBJS= netcon/intercept.o
SHLIB = ${LIB_NAME}.${SHLIB_EXT}.${SHLIB_MAJOR}.${SHLIB_MINOR}
SHLDFLAGS = -g -O2 -Wall -I. -nostdlib -shared
LIBS = -ldl -lc -lrt -lpthread

lib:
	${SHCC} $(intercept_CFLAGS) -I. netcon/intercept.c -o netcon/intercept.o
	${SHCC} $(intercept_CFLAGS) -I. netcon/common.c -o netcon/common.o
	${SHCC} $(intercept_CFLAGS) -I. netcon/sendfd.c -o netcon/sendfd.o
	${SHCC} $(SHLDFLAGS) netcon/intercept.o netcon/common.o netcon/sendfd.o -o netcon/libintercept.so.1.0 $(LIBS)

install:
	cp netcon/libintercept.so.1.0 /lib/libintercept.so.1.0
	ln -sf /lib/libintercept.so.1.0 /lib/libintercept
	/usr/bin/install -c netcon/intercept /usr/bin

uninstall:
	rm -r /lib/libintercept.so.1.0
	rm -r /lib/libintercept
	rm -r /usr/bin/intercept
