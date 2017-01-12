CC=clang
CXX=clang++
INCLUDES=
DEFS=
LIBS=
ARCH_FLAGS=
CODESIGN=echo
PRODUCTSIGN=echo
CODESIGN_APP_CERT=
CODESIGN_INSTALLER_CERT=

# 3 == MacOS, 2 == X64 (the only arch for MacOS right now)
DEFS+=-DZT_BUILD_PLATFORM=3 -DZT_BUILD_ARCHITECTURE=2

include objects.mk
OBJS+=osdep/OSXEthernetTap.o ext/lz4/lz4.o ext/http-parser/http_parser.o

# Official releases are signed with our Apple cert and apply software updates by default
ifeq ($(ZT_OFFICIAL_RELEASE),1)
	DEFS+=-DZT_SOFTWARE_UPDATE_DEFAULT="\"apply\""
	ZT_USE_MINIUPNPC=1
	CODESIGN=codesign
	PRODUCTSIGN=productsign
	CODESIGN_APP_CERT="Developer ID Application: ZeroTier, Inc (8ZD9JUCZ4V)"
	CODESIGN_INSTALLER_CERT="Developer ID Installer: ZeroTier, Inc (8ZD9JUCZ4V)"
else
	DEFS+=-DZT_SOFTWARE_UPDATE_DEFAULT="\"download\""
endif

ifeq ($(ZT_ENABLE_CLUSTER),1)
	DEFS+=-DZT_ENABLE_CLUSTER
endif

# Build miniupnpc and nat-pmp as included libraries -- extra defs are required for these sources
DEFS+=-DMACOSX -DZT_USE_MINIUPNPC -DMINIUPNP_STATICLIB -D_DARWIN_C_SOURCE -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_BSD_SOURCE -D_DEFAULT_SOURCE -DOS_STRING=\"Darwin/15.0.0\" -DMINIUPNPC_VERSION_STRING=\"2.0\" -DUPNP_VERSION_STRING=\"UPnP/1.1\" -DENABLE_STRNATPMPERR
OBJS+=ext/libnatpmp/natpmp.o ext/libnatpmp/getgateway.o ext/miniupnpc/connecthostport.o ext/miniupnpc/igd_desc_parse.o ext/miniupnpc/minisoap.o ext/miniupnpc/minissdpc.o ext/miniupnpc/miniupnpc.o ext/miniupnpc/miniwget.o ext/miniupnpc/minixml.o ext/miniupnpc/portlistingparse.o ext/miniupnpc/receivedata.o ext/miniupnpc/upnpcommands.o ext/miniupnpc/upnpdev.o ext/miniupnpc/upnperrors.o ext/miniupnpc/upnpreplyparse.o osdep/PortMapper.o

# Debug mode -- dump trace output, build binary with -g
ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_TRACE
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	STRIP=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-Ofast -fstack-protector-strong
	CFLAGS+=$(ARCH_FLAGS) -Wall -flto -fPIE -pthread -mmacosx-version-min=10.7 -DNDEBUG -Wno-unused-private-field $(INCLUDES) $(DEFS)
	STRIP=strip
endif

CXXFLAGS=$(CFLAGS) -std=c++11 -stdlib=libc++ 

all: one macui

one:	$(OBJS) service/OneService.o one.o
	$(CXX) $(CXXFLAGS) -o zerotier-one $(OBJS) service/OneService.o one.o $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-idtool
	ln -sf zerotier-one zerotier-cli
	$(CODESIGN) -f -s $(CODESIGN_APP_CERT) zerotier-one
	#$(CODESIGN) -vvv zerotier-one

macui:	FORCE
	cd macui && xcodebuild -target "ZeroTier One" -configuration Release
	$(CODESIGN) -f -s $(CODESIGN_APP_CERT) "macui/build/Release/ZeroTier One.app"
	#$(CODESIGN) -vvv "macui/build/Release/ZeroTier One.app"

cli:	FORCE
	$(CXX) $(CXXFLAGS) -o zerotier cli/zerotier.cpp osdep/OSUtils.cpp node/InetAddress.cpp node/Utils.cpp node/Salsa20.cpp node/Identity.cpp node/SHA512.cpp node/C25519.cpp -lcurl
	$(STRIP) zerotier

selftest: $(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

# Requires Packages: http://s.sudre.free.fr/Software/Packages/about.html
mac-dist-pkg: FORCE
	packagesbuild "ext/installfiles/mac/ZeroTier One.pkgproj"
	rm -f "ZeroTier One Signed.pkg"
	$(PRODUCTSIGN) --sign $(CODESIGN_INSTALLER_CERT) "ZeroTier One.pkg" "ZeroTier One Signed.pkg"
	if [ -f "ZeroTier One Signed.pkg" ]; then mv -f "ZeroTier One Signed.pkg" "ZeroTier One.pkg"; fi

# For ZeroTier, Inc. to build official signed packages
official: FORCE
	make clean
	make ZT_OFFICIAL_RELEASE=1 -j 4 one macui
	make ZT_OFFICIAL_RELEASE=1 mac-dist-pkg

clean:
	rm -rf *.dSYM build-* *.pkg *.dmg *.o node/*.o controller/*.o service/*.o osdep/*.o ext/http-parser/*.o ext/lz4/*.o ext/json-parser/*.o $(OBJS) zerotier-one zerotier-idtool zerotier-selftest zerotier-cli zerotier ZeroTierOneInstaller-* mkworld doc/node_modules macui/build

distclean:	clean

# For those building from source -- installs signed binary tap driver in system ZT home
install-mac-tap: FORCE
	mkdir -p /Library/Application\ Support/ZeroTier/One
	rm -rf /Library/Application\ Support/ZeroTier/One/tap.kext
	cp -R ext/bin/tap-mac/tap.kext /Library/Application\ Support/ZeroTier/One
	chown -R root:wheel /Library/Application\ Support/ZeroTier/One/tap.kext

FORCE:
