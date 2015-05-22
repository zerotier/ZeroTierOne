CC=clang
CXX=clang++

INCLUDES=-I/usr/local/include
DEFS=
LIBS=
ARCH_FLAGS=-arch x86_64

include objects.mk
OBJS+=osdep/OSXEthernetTap.o 

# Disable codesign since open source users will not have ZeroTier's certs
CODESIGN=echo
CODESIGN_CERT=

# For internal use only -- signs everything with ZeroTier's developer cert
ifeq ($(ZT_OFFICIAL_RELEASE),1)
	DEFS+=-DZT_OFFICIAL_RELEASE -DZT_AUTO_UPDATE 
	CODESIGN=codesign
	CODESIGN_CERT="Developer ID Application: ZeroTier Networks LLC (8ZD9JUCZ4V)"
endif

ifeq ($(ZT_AUTO_UPDATE),1)
	DEFS+=-DZT_AUTO_UPDATE 
endif

# Build with ZT_ENABLE_NETWORK_CONTROLLER=1 to build with the Sqlite network controller
ifeq ($(ZT_ENABLE_NETWORK_CONTROLLER),1)
	DEFS+=-DZT_ENABLE_NETWORK_CONTROLLER 
	LIBS+=-L/usr/local/lib -lsqlite3
	OBJS+=controller/SqliteNetworkController.o 
endif

# Enable SSE-optimized Salsa20 -- all Intel macs support SSE2
DEFS+=-DZT_SALSA20_SSE

# Debug mode -- dump trace output, build binary with -g
ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_TRACE 
	CFLAGS=-Wall -g -pthread $(INCLUDES) $(DEFS)
	STRIP=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS=$(ARCH_FLAGS) -Wall -O3 -flto -fPIE -fvectorize -fstack-protector -pthread -mmacosx-version-min=10.7 -DNDEBUG -Wno-unused-private-field $(INCLUDES) $(DEFS)
	STRIP=strip
endif

CXXFLAGS=$(CFLAGS) -fno-rtti

all: one

one:	$(OBJS) one.o
	$(CXX) $(CXXFLAGS) -o zerotier-one $(OBJS) one.o $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-idtool
	ln -sf zerotier-one zerotier-cli
	$(CODESIGN) -f -s $(CODESIGN_CERT) zerotier-one
	$(CODESIGN) -vvv zerotier-one

selftest: $(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

# Requires Packages: http://s.sudre.free.fr/Software/Packages/about.html
mac-dist-pkg: FORCE
	cd ext/installfiles/mac ; packagesbuild "ZeroTier One.pkgproj"
	$(CODESIGN) -f -s $(CODESIGN_CERT) "ZeroTier One.pkg"
	$(CODESIGN) -vvv "ZeroTier One.pkg"

# For internal use only
official: FORCE
	make clean
	make -j 4 ZT_OFFICIAL_RELEASE=1
	make ZT_OFFICIAL_RELEASE=1 mac-dist-pkg

clean:
	rm -rf *.dSYM build-* *.pkg *.dmg *.o node/*.o controller/*.o service/*.o osdep/*.o ext/http-parser/*.o ext/lz4/*.o ext/json-parser/*.o zerotier-one zerotier-idtool zerotier-selftest zerotier-cli ZeroTierOneInstaller-*

# For those building from source -- installs signed binary tap driver in system ZT home
install-mac-tap: FORCE
	mkdir -p /Library/Application\ Support/ZeroTier/One
	rm -rf /Library/Application\ Support/ZeroTier/One/tap.kext
	cp -R ext/bin/tap-mac/tap.kext /Library/Application\ Support/ZeroTier/One
	chown -R root:wheel /Library/Application\ Support/ZeroTier/One/tap.kext

FORCE:
