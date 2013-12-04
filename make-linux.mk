CC=gcc
CXX=g++

INCLUDES=
DEFS=
LIBS=

# Uncomment for a release optimized build
CFLAGS=-Wall -O3 -fno-unroll-loops -fvisibility=hidden -fstack-protector -pthread $(INCLUDES) -DNDEBUG $(DEFS)
STRIP=strip --strip-all

# Uncomment for a debug build
#CFLAGS=-Wall -g -pthread $(INCLUDES) -DZT_TRACE $(DEFS)
#STRIP=echo

# Uncomment for gprof profile build
#CFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#STRIP=echo

CXXFLAGS=$(CFLAGS) -fno-rtti

include objects.mk

all:	one

one:	$(OBJS)
	$(CXX) $(CXXFLAGS) -o zerotier-one main.cpp $(OBJS) $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-cli
	ln -sf zerotier-one zerotier-idtool

selftest:	$(OBJS)
	$(CXX) $(CXXFLAGS) -o zerotier-selftest selftest.cpp $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

idtool:	$(OBJS)
	$(CXX) $(CXXFLAGS) -o zerotier-idtool idtool.cpp $(OBJS) $(LIBS)
	$(STRIP) zerotier-idtool

file2lz4c: ext/lz4/lz4hc.o FORCE
	$(CXX) $(CXXFLAGS) -o file2lz4c file2lz4c.cpp node/Utils.cpp node/Salsa20.cpp ext/lz4/lz4hc.o

installer: one FORCE
	./buildinstaller.sh

clean:
	rm -rf $(OBJS) file2lz4c zerotier-* zt1-*-install installer-build build-ZeroTierUI-*

FORCE:
