CC=clang
CXX=clang++

INCLUDES=
DEFS=
LIBS=

# Disable codesign since open source users will not have ZeroTier's certs
CODESIGN=echo
CODESIGN_CERT=

ifeq ($(ZT_OFFICIAL_RELEASE),1)
	ZT_AUTO_UPDATE=1
	DEFS+=-DZT_OFFICIAL_RELEASE 
	CODESIGN=codesign
	CODESIGN_CERT="Developer ID Application: ZeroTier Networks LLC (8ZD9JUCZ4V)"
endif
ifeq ($(ZT_AUTO_UPDATE),1)
	DEFS+=-DZT_AUTO_UPDATE 
endif

# Enable SSE-optimized Salsa20
DEFS+=-DZT_SALSA20_SSE

# "make debug" is a shortcut for this
ifeq ($(ZT_DEBUG),1)
#	DEFS+=-DZT_TRACE -DZT_LOG_STDOUT 
	CFLAGS=-Wall -g -pthread $(INCLUDES) $(DEFS)
	STRIP=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -ftree-vectorize -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS=-arch i386 -arch x86_64 -Wall -O3 -flto -fPIE -fvectorize -fstack-protector -pthread -mmacosx-version-min=10.6 -DNDEBUG -Wno-unused-private-field $(INCLUDES) $(DEFS)
	STRIP=strip
endif

CXXFLAGS=$(CFLAGS) -fno-rtti

include objects.mk
OBJS+=osnet/BSDRoutingTable.o osnet/OSXEthernetTap.o osnet/OSXEthernetTapFactory.o

all: one

one:	$(OBJS) main.o
	$(CXX) $(CXXFLAGS) -o zerotier-one main.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-cli
	ln -sf zerotier-one zerotier-idtool

selftest: $(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

# Requires that ../Qt be symlinked to the Qt root to use for UI build
mac-ui: FORCE
	mkdir -p build-ZeroTierUI-release
	cd build-ZeroTierUI-release ; ../../Qt/bin/qmake ../ZeroTierUI/ZeroTierUI.pro ; make -j 4
	strip "build-ZeroTierUI-release/ZeroTier One.app/Contents/MacOS/ZeroTier One"
	find "build-ZeroTierUI-release/ZeroTier One.app" -type f -name '.DS_Store' -print0 | xargs -0 rm -f
	$(CODESIGN) -f -s $(CODESIGN_CERT) "build-ZeroTierUI-release/ZeroTier One.app"
	$(CODESIGN) -vvv "build-ZeroTierUI-release/ZeroTier One.app"

clean:
	rm -rf *.dSYM main.o selftest.o build-* $(OBJS) zerotier-* ZeroTierOneInstaller-* "ZeroTier One.zip" "ZeroTier One.dmg"

debug:	FORCE
	make -j 4 ZT_DEBUG=1

# For our use -- builds official signed binary, packages in installer and download DMG
official: FORCE
	make -j 4 ZT_OFFICIAL_RELEASE=1
	make mac-ui ZT_OFFICIAL_RELEASE=1
	./buildinstaller.sh
	make mac-dmg ZT_OFFICIAL_RELEASE=1

mac-dmg:	FORCE
	mkdir -p build-ZeroTierOne-dmg
	cd build-ZeroTierOne-dmg ; ln -sf /Applications Applications
	cp -a "build-ZeroTierUI-release/ZeroTier One.app" build-ZeroTierOne-dmg/
	rm -f /tmp/tmp.dmg
	hdiutil create /tmp/tmp.dmg -ov -volname "ZeroTier One" -fs HFS+ -srcfolder ./build-ZeroTierOne-dmg
	hdiutil convert /tmp/tmp.dmg -format UDZO -o "ZeroTier One.dmg"
	$(CODESIGN) -f -s $(CODESIGN_CERT) "ZeroTier One.dmg"
	rm -f /tmp/tmp.dmg

# For those building from source -- installs signed binary tap driver in system ZT home
install-mac-tap: FORCE
	mkdir -p /Library/Application\ Support/ZeroTier/One
	rm -rf /Library/Application\ Support/ZeroTier/One/tap.kext
	cp -R ext/bin/tap-mac/tap.kext /Library/Application\ Support/ZeroTier/One
	chown -R root:wheel /Library/Application\ Support/ZeroTier/One/tap.kext

FORCE:
