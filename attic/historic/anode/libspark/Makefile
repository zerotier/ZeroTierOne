SYSNAME:=${shell uname}
SYSNAME!=uname
include ../config.mk.${SYSNAME}

LIBSPARK_OBJS=

all: libspark

libspark: $(LIBSPARK_OBJS)
	ar rcs libspark.a $(LIBSPARK_OBJS)
	ranlib libspark.a

clean: force
	rm -f *.a *.so *.dylib *.dll *.lib *.exe *.o

force: ;
