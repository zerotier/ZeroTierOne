# Pick clang or gcc, with preference for clang
CC=$(shell which clang gcc cc 2>/dev/null | head -n 1)
CXX=$(shell which clang++ g++ c++ 2>/dev/null | head -n 1)

INCLUDES=
DEFS=
LIBS=

ifeq ($(ZT_OFFICIAL_RELEASE),1)
	ZT_AUTO_UPDATE=1
endif
ifeq ($(ZT_AUTO_UPDATE),1)
	DEFS+=-DZT_AUTO_UPDATE 
endif

# Uncomment to dump trace and log to stdout
#DEFS+=-DZT_TRACE -DZT_LOG_STDOUT 

# Uncomment for a release optimized build
CFLAGS=-Wall -O3 -fvisibility=hidden -fstack-protector -pthread $(INCLUDES) -DNDEBUG $(DEFS)
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

installer: one FORCE
	./buildinstaller.sh

clean:
	rm -rf $(OBJS) zerotier-* build-* ZeroTierOneInstaller-*

official: FORCE
	make -j 4 ZT_OFFICIAL_RELEASE=1
	./buildinstaller.sh

FORCE:
