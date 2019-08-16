CC=gcc
CXX=g++

INCLUDES=
DEFS=
LIBS=

include objects.mk
OBJS+=osdep/NetBSDEthernetTap.o ext/lz4/lz4.o ext/json-parser/json.o ext/http-parser/http_parser.o

# "make official" is a shortcut for this
ifeq ($(ZT_OFFICIAL_RELEASE),1)
	DEFS+=-DZT_OFFICIAL_RELEASE
endif

# Build with ZT_ENABLE_CLUSTER=1 to build with cluster support
ifeq ($(ZT_ENABLE_CLUSTER),1)
	DEFS+=-DZT_ENABLE_CLUSTER
endif

# "make debug" is a shortcut for this
ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_TRACE
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	LDFLAGS+=
	STRIP=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-O3 -fstack-protector
	CFLAGS+=-fPIE -fvisibility=hidden -fstack-protector -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	LDFLAGS+=-pie -Wl,-z,relro,-z,now
	STRIP=strip --strip-all
endif

CXXFLAGS+=$(CFLAGS) -fno-rtti -fpermissive

all:	one

one:	$(OBJS) service/OneService.o one.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS)  -o zerotier-one $(OBJS) service/OneService.o one.o $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-idtool
	ln -sf zerotier-one zerotier-cli

selftest:	$(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

# No installer on FreeBSD yet
#installer: one FORCE
#	./buildinstaller.sh

clean:
	rm -rf *.o node/*.o controller/*.o osdep/*.o service/*.o ext/http-parser/*.o ext/lz4/*.o ext/json-parser/*.o build-* zerotier-one zerotier-idtool zerotier-selftest zerotier-cli ZeroTierOneInstaller-*

debug:	FORCE
	make -j 4 ZT_DEBUG=1

#official: FORCE
#	make -j 4 ZT_OFFICIAL_RELEASE=1
#	./buildinstaller.sh

FORCE:
