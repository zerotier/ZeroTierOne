CC=clang
CXX=clang++
TOPDIR=$(shell PWD)

INCLUDES=-I$(shell PWD)/rustybits/target -isystem $(TOPDIR)/ext  -I$(TOPDIR)/ext/prometheus-cpp-lite-1.0/core/include -I$(TOPDIR)/ext-prometheus-cpp-lite-1.0/3rdparty/http-client-lite/include -I$(TOPDIR)/ext/prometheus-cpp-lite-1.0/simpleapi/include
DEFS=
LIBS=
ARCH_FLAGS=-arch x86_64 -arch arm64 

CODESIGN=echo
PRODUCTSIGN=echo
CODESIGN_APP_CERT=
CODESIGN_INSTALLER_CERT=
NOTARIZE=echo
NOTARIZE_APPLE_ID=null
NOTARIZE_TEAM_ID=null

ZT_BUILD_PLATFORM=3
ZT_BUILD_ARCHITECTURE=2
ZT_VERSION_MAJOR=$(shell cat version.h | grep -F VERSION_MAJOR | cut -d ' ' -f 3)
ZT_VERSION_MINOR=$(shell cat version.h | grep -F VERSION_MINOR | cut -d ' ' -f 3)
ZT_VERSION_REV=$(shell cat version.h | grep -F VERSION_REVISION | cut -d ' ' -f 3)
ZT_VERSION_BUILD=$(shell cat version.h | grep -F VERSION_BUILD | cut -d ' ' -f 3)

# for central controller builds
TIMESTAMP=$(shell date +"%Y%m%d%H%M")

DEFS+=-DZT_BUILD_PLATFORM=$(ZT_BUILD_PLATFORM) -DZT_BUILD_ARCHITECTURE=$(ZT_BUILD_ARCHITECTURE)

include objects.mk
ONE_OBJS+=osdep/MacEthernetTap.o osdep/MacKextEthernetTap.o osdep/MacDNSHelper.o ext/http-parser/http_parser.o
LIBS+=-framework CoreServices -framework SystemConfiguration -framework CoreFoundation -framework Security

# Official releases are signed with our Apple cert and apply software updates by default
ifeq ($(ZT_OFFICIAL_RELEASE),1)
	DEFS+=-DZT_SOFTWARE_UPDATE_DEFAULT="\"apply\""
	ZT_USE_MINIUPNPC=1
	CODESIGN=codesign
	PRODUCTSIGN=productsign
	CODESIGN_APP_CERT="Developer ID Application: ZeroTier, Inc (8ZD9JUCZ4V)"
	CODESIGN_INSTALLER_CERT="Developer ID Installer: ZeroTier, Inc (8ZD9JUCZ4V)"
	NOTARIZE=xcrun notarytool
	NOTARIZE_APPLE_ID="adam.ierymenko@gmail.com"
	NOTARIZE_TEAM_ID="8ZD9JUCZ4V"
else
	DEFS+=-DZT_SOFTWARE_UPDATE_DEFAULT="\"download\""
endif

# Use fast ASM Salsa20/12 for x64 processors
DEFS+=-DZT_USE_X64_ASM_SALSA2012
CORE_OBJS+=ext/x64-salsa2012-asm/salsa2012.o
CXXFLAGS=$(CFLAGS) -std=c++17 -stdlib=libc++

# Build miniupnpc and nat-pmp as included libraries -- extra defs are required for these sources
DEFS+=-DMACOSX -DZT_SSO_SUPPORTED -DZT_USE_MINIUPNPC -DMINIUPNP_STATICLIB -D_DARWIN_C_SOURCE -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_BSD_SOURCE -D_DEFAULT_SOURCE -DOS_STRING=\"Darwin/15.0.0\" -DMINIUPNPC_VERSION_STRING=\"2.0\" -DUPNP_VERSION_STRING=\"UPnP/1.1\" -DENABLE_STRNATPMPERR
ONE_OBJS+=ext/libnatpmp/natpmp.o ext/libnatpmp/getgateway.o ext/miniupnpc/connecthostport.o ext/miniupnpc/igd_desc_parse.o ext/miniupnpc/minisoap.o ext/miniupnpc/minissdpc.o ext/miniupnpc/miniupnpc.o ext/miniupnpc/miniwget.o ext/miniupnpc/minixml.o ext/miniupnpc/portlistingparse.o ext/miniupnpc/receivedata.o ext/miniupnpc/upnpcommands.o ext/miniupnpc/upnpdev.o ext/miniupnpc/upnperrors.o ext/miniupnpc/upnpreplyparse.o osdep/PortMapper.o
ifeq ($(ZT_CONTROLLER),1)
	MACOS_VERSION_MIN=10.15
	override CXXFLAGS=$(CFLAGS) -std=c++17 -stdlib=libc++
	LIBS+=-L/usr/local/opt/libpqxx/lib -L/usr/local/opt/libpq/lib -L/usr/local/opt/openssl/lib/ -lpqxx -lpq -lssl -lcrypto -lgssapi_krb5 ext/redis-plus-plus-1.1.1/install/macos/lib/libredis++.a ext/hiredis-0.14.1/lib/macos/libhiredis.a
	DEFS+=-DZT_CONTROLLER_USE_LIBPQ -DZT_CONTROLLER_USE_REDIS -DZT_CONTROLLER 
	INCLUDES+=-I/usr/local/opt/libpq/include -I/usr/local/opt/libpqxx/include -Iext/hiredis-0.14.1/include/ -Iext/redis-plus-plus-1.1.1/install/macos/include/sw/
else
	MACOS_VERSION_MIN=10.13
endif

# Build with address sanitization library for advanced debugging (clang)
ifeq ($(ZT_SANITIZE),1)
	DEFS+=-fsanitize=address -DASAN_OPTIONS=symbolize=1
endif
ifeq ($(ZT_DEBUG_TRACE),1)
	DEFS+=-DZT_DEBUG_TRACE
endif
# Debug mode -- dump trace output, build binary with -g
ifeq ($(ZT_DEBUG),1)
	ZT_TRACE=1
	ARCH_FLAGS=
	CFLAGS+=-Wall -g $(INCLUDES) $(DEFS) $(ARCH_FLAGS)
	STRIP=echo
	EXTRA_CARGO_FLAGS=
	RUST_VARIANT=debug
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g $(INCLUDES) $(DEFS)
else
	CFLAGS?=-Ofast -fstack-protector-strong
	CFLAGS+=$(ARCH_FLAGS) -Wall -flto -fPIE -mmacosx-version-min=$(MACOS_VERSION_MIN) -DNDEBUG -Wno-unused-private-field $(INCLUDES) $(DEFS)
	STRIP=strip
	EXTRA_CARGO_FLAGS=--release
	RUST_VARIANT=release
endif

ifeq ($(ZT_TRACE),1)
	DEFS+=-DZT_TRACE
endif

ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_DEBUG
endif

ifeq ($(ZT_VAULT_SUPPORT),1)
	DEFS+=-DZT_VAULT_SUPPORT=1
	LIBS+=-lcurl
endif

all: one

ext/x64-salsa2012-asm/salsa2012.o:
	as -arch x86_64 -mmacosx-version-min=$(MACOS_VERSION_MIN) -o ext/x64-salsa2012-asm/salsa2012.o ext/x64-salsa2012-asm/salsa2012.s

mac-agent: FORCE
	$(CC) -Ofast $(ARCH_FLAGS) -mmacosx-version-min=$(MACOS_VERSION_MIN) -o MacEthernetTapAgent osdep/MacEthernetTapAgent.c
	$(CODESIGN) -f --options=runtime -s $(CODESIGN_APP_CERT) MacEthernetTapAgent

osdep/MacDNSHelper.o: osdep/MacDNSHelper.mm
	$(CXX) $(CXXFLAGS) -c osdep/MacDNSHelper.mm -o osdep/MacDNSHelper.o 

one:	zeroidc $(CORE_OBJS) $(ONE_OBJS) one.o mac-agent 
	$(CXX) $(CXXFLAGS) -o zerotier-one $(CORE_OBJS) $(ONE_OBJS) one.o $(LIBS) rustybits/target/libzeroidc.a
	# $(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-idtool
	ln -sf zerotier-one zerotier-cli
	$(CODESIGN) -f --options=runtime -s $(CODESIGN_APP_CERT) zerotier-one

zerotier-one: one

zeroidc: rustybits/target/libzeroidc.a

rustybits/target/libzeroidc.a:	FORCE
	cd rustybits && MACOSX_DEPLOYMENT_TARGET=$(MACOS_VERSION_MIN) cargo build -p zeroidc --target=x86_64-apple-darwin $(EXTRA_CARGO_FLAGS)
	cd rustybits && MACOSX_DEPLOYMENT_TARGET=$(MACOS_VERSION_MIN) cargo build -p zeroidc --target=aarch64-apple-darwin $(EXTRA_CARGO_FLAGS)
	cd rustybits && lipo -create target/x86_64-apple-darwin/$(RUST_VARIANT)/libzeroidc.a target/aarch64-apple-darwin/$(RUST_VARIANT)/libzeroidc.a -output target/libzeroidc.a

central-controller:
	make ARCH_FLAGS="-arch x86_64" ZT_CONTROLLER=1 one

zerotier-idtool: one

zerotier-cli: one

$(ONE_OBJS): zeroidc

libzerotiercore.a:	$(CORE_OBJS)
	ar rcs libzerotiercore.a $(CORE_OBJS)
	ranlib libzerotiercore.a

core: libzerotiercore.a

#cli:	FORCE
#	$(CXX) $(CXXFLAGS) -o zerotier cli/zerotier.cpp osdep/OSUtils.cpp node/InetAddress.cpp node/Utils.cpp node/Salsa20.cpp node/Identity.cpp node/SHA512.cpp node/C25519.cpp -lcurl
#	$(STRIP) zerotier

selftest: $(CORE_OBJS) $(ONE_OBJS) selftest.o
	$(CXX) $(CXXFLAGS) -o zerotier-selftest selftest.o $(CORE_OBJS) $(ONE_OBJS) $(LIBS) rustybits/target/libzeroidc.a
	$(STRIP) zerotier-selftest

zerotier-selftest: selftest

# Make compile_commands.json for clangd editor extensions. Probably works on Linux too.
compile_commands: FORCE
	compiledb make ZT_DEBUG=1

# Requires Packages: http://s.sudre.free.fr/Software/Packages/about.html
mac-dist-pkg: FORCE
	packagesbuild "ext/installfiles/mac/ZeroTier One.pkgproj"
	rm -f "ZeroTier One Signed.pkg"
	$(PRODUCTSIGN) --sign $(CODESIGN_INSTALLER_CERT) "ZeroTier One.pkg" "ZeroTier One Signed.pkg"
	if [ -f "ZeroTier One Signed.pkg" ]; then mv -f "ZeroTier One Signed.pkg" "ZeroTier One.pkg"; fi
	rm -f zt1_update_$(ZT_BUILD_PLATFORM)_$(ZT_BUILD_ARCHITECTURE)_*
	cat ext/installfiles/mac-update/updater.tmpl.sh "ZeroTier One.pkg" >zt1_update_$(ZT_BUILD_PLATFORM)_$(ZT_BUILD_ARCHITECTURE)_$(ZT_VERSION_MAJOR).$(ZT_VERSION_MINOR).$(ZT_VERSION_REV)_$(ZT_VERSION_BUILD).exe
	$(NOTARIZE) submit --apple-id "adam.ierymenko@gmail.com" --team-id "8ZD9JUCZ4V" --wait "ZeroTier One.pkg"
	echo '*** When Apple notifies that the app is notarized, run: xcrun stapler staple "ZeroTier One.pkg"'

# For ZeroTier, Inc. to build official signed packages
official: FORCE
	cd ../DesktopUI ; make ZT_OFFICIAL_RELEASE=1
	make clean
	make ZT_OFFICIAL_RELEASE=1 -j 8 one
	make ZT_OFFICIAL_RELEASE=1 mac-dist-pkg

_buildx:
	@echo "docker buildx create"
	# docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
	docker run --privileged --rm tonistiigi/binfmt --install all
	@echo docker buildx create --name multiarch --driver docker-container --use
	@echo docker buildx inspect --bootstrap

controller-builder: _buildx FORCE
	docker buildx build --platform linux/arm64,linux/amd64 --no-cache -t registry.zerotier.com/zerotier/ctlbuild:latest -f ext/central-controller-docker/Dockerfile.builder . --push

controller-run: _buildx FORCE
	docker buildx build --platform linux/arm64,linux/amd64 --no-cache -t registry.zerotier.com/zerotier-central/ctlrun:latest -f ext/central-controller-docker/Dockerfile.run_base . --push

central-controller-docker: _buildx FORCE
	docker buildx build --platform linux/arm64,linux/amd64 --no-cache -t registry.zerotier.com/zerotier-central/ztcentral-controller:${TIMESTAMP} -f ext/central-controller-docker/Dockerfile --build-arg git_branch=$(shell git name-rev --name-only HEAD) . --push
	@echo Image: registry.zerotier.com/zerotier-central/ztcentral-controller:${TIMESTAMP}
	
clean:
	rm -rf MacEthernetTapAgent *.dSYM build-* *.a *.pkg *.dmg *.o node/*.o controller/*.o service/*.o osdep/*.o ext/http-parser/*.o $(CORE_OBJS) $(ONE_OBJS) zerotier-one zerotier-idtool zerotier-selftest zerotier-cli zerotier doc/node_modules zt1_update_$(ZT_BUILD_PLATFORM)_$(ZT_BUILD_ARCHITECTURE)_* rustybits/target/

distclean:	clean

realclean:	clean

FORCE:
