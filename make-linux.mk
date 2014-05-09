# Pick clang or gcc, with preference for clang
CC=$(shell which clang gcc cc 2>/dev/null | head -n 1)
CXX=$(shell which clang++ g++ c++ 2>/dev/null | head -n 1)

INCLUDES=
DEFS=
LIBS=

ifeq ($(ZT_OFFICIAL_RELEASE),1)
	ZT_AUTO_UPDATE=1
	DEFS+=-DZT_OFFICIAL_RELEASE 
endif

ifeq ($(ZT_AUTO_UPDATE),1)
	DEFS+=-DZT_AUTO_UPDATE 
endif

ifeq ($(ZT_USE_TESTNET),1)
	DEFS+=-DZT_USE_TESTNET
endif

# Uncomment to dump trace and log to stdout
#DEFS+=-DZT_TRACE -DZT_LOG_STDOUT 

# Uncomment for a release optimized build
CFLAGS=-Wall -O3 -fPIE -fvisibility=hidden -fstack-protector -pthread $(INCLUDES) -DNDEBUG $(DEFS)
LDFLAGS=-pie -Wl,-z,relro,-z,now
STRIP=strip --strip-all

# Uncomment for a debug build
#CFLAGS=-Wall -g -pthread $(INCLUDES) -DZT_TRACE -DZT_LOG_STDOUT $(DEFS)
#LDFLAGS=
#STRIP=echo

# Uncomment for gprof profile build
#CFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#LDFLAGS=
#STRIP=echo

CXXFLAGS=$(CFLAGS) -fno-rtti

include objects.mk

all:	one

one:	$(OBJS) main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-one main.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-cli
	ln -sf zerotier-one zerotier-idtool

selftest:	$(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

installer: one FORCE
	./buildinstaller.sh

clean:
	rm -rf $(OBJS) *.o node/*.o zerotier-* build-* ZeroTierOneInstaller-*

official: FORCE
	make -j 4 ZT_OFFICIAL_RELEASE=1
	./buildinstaller.sh

FORCE:
