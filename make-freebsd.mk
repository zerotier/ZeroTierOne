CC?=cc
CXX?=c++

INCLUDES=
DEFS=
LIBS=

include objects.mk
OBJS+=osnet/BSDEthernetTapFactory.o osnet/BSDEthernetTap.o osnet/BSDRoutingTable.o
TESTNET_OBJS=testnet/SimNet.o testnet/SimNetSocketManager.o testnet/TestEthernetTap.o testnet/TestEthernetTapFactory.o testnet/TestRoutingTable.o

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
#	DEFS+=-DZT_TRACE -DZT_LOG_STDOUT 
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	LDFLAGS+=
	STRIP=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS?=-O2 CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-O3 -fstack-protector
	CFLAGS+=-Wall -fPIE -fvisibility=hidden -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	LDFLAGS+=-pie -Wl,-z,relro,-z,now
	STRIP=strip --strip-all
endif

CXXFLAGS?=-fno-rtti
CXXFLAGS=$(CFLAGS)

all:	one

one:	$(OBJS) main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-one main.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-cli
	ln -sf zerotier-one zerotier-idtool

selftest:	$(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

testnet: $(TESTNET_OBJS) $(OBJS) testnet.o
	$(CXX) $(CXXFLAGS) -o zerotier-testnet testnet.o $(OBJS) $(TESTNET_OBJS) $(LIBS)
	$(STRIP) zerotier-testnet

# No installer on FreeBSD yet
#installer: one FORCE
#	./buildinstaller.sh

clean:
	rm -rf $(OBJS) $(TESTNET_OBJS) node/*.o osnet/*.o control/*.o testnet/*.o *.o zerotier-* build-* ZeroTierOneInstaller-*

debug:	FORCE
	make -j 4 ZT_DEBUG=1

#official: FORCE
#	make -j 4 ZT_OFFICIAL_RELEASE=1
#	./buildinstaller.sh

FORCE:
