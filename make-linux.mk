# Pick clang or gcc, with preference for clang
CC=$(shell which clang gcc cc 2>/dev/null | head -n 1)
CXX=$(shell which clang++ g++ c++ 2>/dev/null | head -n 1)

INCLUDES=
DEFS=
LIBS=

# Enable SSE-optimized Salsa20 on x86 and x86_64 machines
MACHINE=$(shell uname -m)
ifeq ($(MACHINE),x86_64)
	DEFS+=-DZT_SALSA20_SSE 
endif
ifeq ($(MACHINE),amd64)
	DEFS+=-DZT_SALSA20_SSE 
endif
ifeq ($(MACHINE),i686)
	DEFS+=-DZT_SALSA20_SSE 
endif
ifeq ($(MACHINE),i586)
	DEFS+=-DZT_SALSA20_SSE 
endif
ifeq ($(MACHINE),i386)
	DEFS+=-DZT_SALSA20_SSE 
endif
ifeq ($(MACHINE),x86)
	DEFS+=-DZT_SALSA20_SSE 
endif

# "make official" is a shortcut for this
ifeq ($(ZT_OFFICIAL_RELEASE),1)
	ZT_AUTO_UPDATE=1
	DEFS+=-DZT_OFFICIAL_RELEASE 
endif
ifeq ($(ZT_AUTO_UPDATE),1)
	DEFS+=-DZT_AUTO_UPDATE 
endif

# "make debug" is a shortcut for this
ifeq ($(ZT_DEBUG),1)
	CFLAGS=-Wall -g -pthread $(INCLUDES) -DZT_TRACE -DZT_LOG_STDOUT $(DEFS)
	LDFLAGS=
	STRIP=echo
	DEFS+=-DZT_TRACE -DZT_LOG_STDOUT 
else
	CFLAGS=-Wall -O3 -fPIE -fvisibility=hidden -fstack-protector -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	LDFLAGS=-pie -Wl,-z,relro,-z,now
	STRIP=strip --strip-all
endif

# Uncomment for gprof profile build
#CFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#LDFLAGS=
#STRIP=echo

CXXFLAGS=$(CFLAGS) -fno-rtti

include objects.mk
OBJS+=osnet/LinuxRoutingTable.o osnet/LinuxEthernetTap.o osnet/LinuxEthernetTapFactory.o

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
	rm -rf $(OBJS) node/*.o osnet/*.o *.o zerotier-* build-* ZeroTierOneInstaller-*

debug:	FORCE
	make -j 4 ZT_DEBUG=1

official: FORCE
	make -j 4 ZT_OFFICIAL_RELEASE=1
	./buildinstaller.sh

FORCE:
