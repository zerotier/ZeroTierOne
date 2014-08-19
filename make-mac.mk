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
	CFLAGS=-Wall -g -pthread -DZT_TRACE -DZT_LOG_STDOUT $(INCLUDES) $(DEFS)
	STRIP=echo
	DEFS+=-DZT_TRACE -DZT_LOG_STDOUT 
else
	CFLAGS=-arch i386 -arch x86_64 -Wall -O3 -flto -fPIE -fvectorize -fstack-protector -pthread -mmacosx-version-min=10.6 -DNDEBUG -Wno-unused-private-field $(INCLUDES) $(DEFS)
	STRIP=strip
endif

CXXFLAGS=$(CFLAGS) -fno-rtti

include objects.mk
OBJS+=main.o osnet/BSDRoutingTable.o osnet/OSXEthernetTap.o osnet/OSXEthernetTapFactory.o

all: one

one:	$(OBJS)
	$(CXX) $(CXXFLAGS) -o zerotier-one $(OBJS) $(LIBS)
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
	mkdir build-ZeroTierOne-dmg
	cd build-ZeroTierOne-dmg ; ln -sf /Applications Applications
	cp -a "build-ZeroTierUI-release/ZeroTier One.app" build-ZeroTierOne-dmg/
	rm -f /tmp/tmp.dmg
	hdiutil create /tmp/tmp.dmg -ov -volname "ZeroTier One" -fs HFS+ -srcfolder ./build-ZeroTierOne-dmg
	hdiutil convert /tmp/tmp.dmg -format UDZO -o "ZeroTier One.dmg"
	rm -f /tmp/tmp.dmg

# For those building from source -- installs signed binary tap driver in system ZT home
install-mac-tap: FORCE
	mkdir -p /Library/Application\ Support/ZeroTier/One
	rm -rf /Library/Application\ Support/ZeroTier/One/tap.kext
	cp -R ext/bin/tap-mac/tap.kext /Library/Application\ Support/ZeroTier/One
	chown -R root:wheel /Library/Application\ Support/ZeroTier/One/tap.kext

FORCE:
