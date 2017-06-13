# Automagically pick clang or gcc, with preference for clang
# This is only done if we have not overridden these with an environment or CLI variable
ifeq ($(origin CC),default)
	CC=$(shell if [ -e /usr/bin/clang ]; then echo clang; else echo gcc; fi)
endif
ifeq ($(origin CXX),default)
	CXX=$(shell if [ -e /usr/bin/clang++ ]; then echo clang++; else echo g++; fi)
endif

INCLUDES?=
DEFS?=
LDLIBS?=
DESTDIR?=

include objects.mk
ONE_OBJS+=osdep/LinuxEthernetTap.o

# Auto-detect miniupnpc and nat-pmp as well and use system libs if present,
# otherwise build into binary as done on Mac and Windows.
ONE_OBJS+=osdep/PortMapper.o
DEFS+=-DZT_USE_MINIUPNPC
MINIUPNPC_IS_NEW_ENOUGH=$(shell grep -sqr '.*define.*MINIUPNPC_VERSION.*"2.."' /usr/include/miniupnpc/miniupnpc.h && echo 1)
ifeq ($(MINIUPNPC_IS_NEW_ENOUGH),1)
	DEFS+=-DZT_USE_SYSTEM_MINIUPNPC
	LDLIBS+=-lminiupnpc
else
	DEFS+=-DMINIUPNP_STATICLIB -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -DOS_STRING=\"Linux\" -DMINIUPNPC_VERSION_STRING=\"2.0\" -DUPNP_VERSION_STRING=\"UPnP/1.1\" -DENABLE_STRNATPMPERR
	ONE_OBJS+=ext/miniupnpc/connecthostport.o ext/miniupnpc/igd_desc_parse.o ext/miniupnpc/minisoap.o ext/miniupnpc/minissdpc.o ext/miniupnpc/miniupnpc.o ext/miniupnpc/miniwget.o ext/miniupnpc/minixml.o ext/miniupnpc/portlistingparse.o ext/miniupnpc/receivedata.o ext/miniupnpc/upnpcommands.o ext/miniupnpc/upnpdev.o ext/miniupnpc/upnperrors.o ext/miniupnpc/upnpreplyparse.o
endif
ifeq ($(wildcard /usr/include/natpmp.h),)
	ONE_OBJS+=ext/libnatpmp/natpmp.o ext/libnatpmp/getgateway.o
else
	LDLIBS+=-lnatpmp
	DEFS+=-DZT_USE_SYSTEM_NATPMP
endif

# Use bundled http-parser since distribution versions are NOT API-stable or compatible!
# Trying to use dynamically linked libhttp-parser causes tons of compatibility problems.
ONE_OBJS+=ext/http-parser/http_parser.o

ifeq ($(ZT_ENABLE_CLUSTER),1)
	DEFS+=-DZT_ENABLE_CLUSTER
endif

ifeq ($(ZT_SYNOLOGY), 1)
	DEFS+=-D__SYNOLOGY__
endif

ifeq ($(ZT_TRACE),1)
	DEFS+=-DZT_TRACE
endif

ifeq ($(ZT_RULES_ENGINE_DEBUGGING),1)
	DEFS+=-DZT_RULES_ENGINE_DEBUGGING
endif

ifeq ($(ZT_DEBUG),1)
	override CFLAGS+=-Wall -Wno-deprecated -Werror -g -pthread $(INCLUDES) $(DEFS)
	override CXXFLAGS+=-Wall -Wno-deprecated -Werror -g -std=c++11 -pthread $(INCLUDES) $(DEFS)
	override LDFLAGS+=
	ZT_TRACE=1
	STRIP?=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in -O0 even on a 3ghz box!
node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CXXFLAGS=-Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	override DEFS+=-D_FORTIFY_SOURCE=2
	CFLAGS?=-O3 -fstack-protector
	override CFLAGS+=-Wall -Wno-deprecated -fPIE -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	CXXFLAGS?=-O3 -fstack-protector
	override CXXFLAGS+=-Wall -Wno-deprecated -Wno-unused-result -Wreorder -fPIE -std=c++11 -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	override LDFLAGS+=-pie -Wl,-z,relro,-z,now
	STRIP?=strip
	STRIP+=--strip-all
endif

ifeq ($(ZT_TRACE),1)
	override DEFS+=-DZT_TRACE
endif

ifeq ($(ZT_USE_TEST_TAP),1)
	override DEFS+=-DZT_USE_TEST_TAP
endif

# Uncomment for gprof profile build
#CFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#CXXFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#LDFLAGS=
#STRIP=echo

# Determine system build architecture from compiler target
CC_MACH=$(shell $(CC) -dumpmachine | cut -d '-' -f 1)
ZT_ARCHITECTURE=999
ifeq ($(CC_MACH),x86_64)
	ZT_ARCHITECTURE=2
	ZT_USE_X64_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),amd64)
	ZT_ARCHITECTURE=2
	ZT_USE_X64_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),i386)
	ZT_ARCHITECTURE=1
endif
ifeq ($(CC_MACH),i486)
	ZT_ARCHITECTURE=1
endif
ifeq ($(CC_MACH),i586)
	ZT_ARCHITECTURE=1
endif
ifeq ($(CC_MACH),i686)
	ZT_ARCHITECTURE=1
endif
ifeq ($(CC_MACH),arm)
	ZT_ARCHITECTURE=3
	override DEFS+=-DZT_NO_TYPE_PUNNING
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),armel)
	ZT_ARCHITECTURE=3
	override DEFS+=-DZT_NO_TYPE_PUNNING
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),armhf)
	ZT_ARCHITECTURE=3
	override DEFS+=-DZT_NO_TYPE_PUNNING
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),armv6)
	ZT_ARCHITECTURE=3
	override DEFS+=-DZT_NO_TYPE_PUNNING
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),armv6zk)
	ZT_ARCHITECTURE=3
	override DEFS+=-DZT_NO_TYPE_PUNNING
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),armv6kz)
	ZT_ARCHITECTURE=3
	override DEFS+=-DZT_NO_TYPE_PUNNING
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),armv7)
	ZT_ARCHITECTURE=3
	override DEFS+=-DZT_NO_TYPE_PUNNING
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),arm64)
	ZT_ARCHITECTURE=4
	override DEFS+=-DZT_NO_TYPE_PUNNING
endif
ifeq ($(CC_MACH),aarch64)
	ZT_ARCHITECTURE=4
	override DEFS+=-DZT_NO_TYPE_PUNNING
endif
ifeq ($(CC_MACH),mipsel)
	ZT_ARCHITECTURE=5
	override DEFS+=-DZT_NO_TYPE_PUNNING
endif
ifeq ($(CC_MACH),mips)
	ZT_ARCHITECTURE=5
	override DEFS+=-DZT_NO_TYPE_PUNNING
endif
ifeq ($(CC_MACH),mips64)
	ZT_ARCHITECTURE=6
	override DEFS+=-DZT_NO_TYPE_PUNNING
endif
ifeq ($(CC_MACH),mips64el)
	ZT_ARCHITECTURE=6
	override DEFS+=-DZT_NO_TYPE_PUNNING
endif

# Fail if system architecture could not be determined
ifeq ($(ZT_ARCHITECTURE),999)
ERR=$(error FATAL: architecture could not be determined from $(CC) -dumpmachine: $CC_MACH)
.PHONY: err
err: ; $(ERR)
endif

# Disable software updates by default on Linux since that is normally done with package management
override DEFS+=-DZT_BUILD_PLATFORM=1 -DZT_BUILD_ARCHITECTURE=$(ZT_ARCHITECTURE) -DZT_SOFTWARE_UPDATE_DEFAULT="\"disable\""

# Static builds, which are currently done for a number of Linux targets
ifeq ($(ZT_STATIC),1)
	override LDFLAGS+=-static
endif

# ARM32 hell -- use conservative CFLAGS
ifeq ($(ZT_ARCHITECTURE),3)
	ifeq ($(shell if [ -e /usr/bin/dpkg ]; then dpkg --print-architecture; fi),armel)
		override CFLAGS+=-march=armv5 -mfloat-abi=soft -msoft-float -mno-unaligned-access -marm
		override CXXFLAGS+=-march=armv5 -mfloat-abi=soft -msoft-float -mno-unaligned-access -marm
		ZT_USE_ARM32_NEON_ASM_SALSA2012=0
	else
		override CFLAGS+=-march=armv5 -mno-unaligned-access -marm
		override CXXFLAGS+=-march=armv5 -mno-unaligned-access -marm
	endif
endif

# Build faster crypto on some targets
ifeq ($(ZT_USE_X64_ASM_SALSA2012),1)
	override DEFS+=-DZT_USE_X64_ASM_SALSA2012
	override CORE_OBJS+=ext/x64-salsa2012-asm/salsa2012.o
endif
ifeq ($(ZT_USE_ARM32_NEON_ASM_SALSA2012),1)
	override DEFS+=-DZT_USE_ARM32_NEON_ASM_SALSA2012
	override CORE_OBJS+=ext/arm32-neon-salsa2012-asm/salsa2012.o
endif

all:	one

#ext/x64-salsa2012-asm/salsa2012.o:
#	$(CC) -c ext/x64-salsa2012-asm/salsa2012.s -o ext/x64-salsa2012-asm/salsa2012.o

#ext/arm32-neon-salsa2012-asm/salsa2012.o:
#	$(CC) -c ext/arm32-neon-salsa2012-asm/salsa2012.s -o ext/arm32-neon-salsa2012-asm/salsa2012.o

one:	$(CORE_OBJS) $(ONE_OBJS) one.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-one $(CORE_OBJS) $(ONE_OBJS) one.o $(LDLIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-idtool
	ln -sf zerotier-one zerotier-cli

zerotier-one: one

zerotier-idtool: one

zerotier-cli: one

libzerotiercore.a:	$(CORE_OBJS)
	ar rcs libzerotiercore.a $(CORE_OBJS)
	ranlib libzerotiercore.a

core: libzerotiercore.a

selftest:	$(CORE_OBJS) $(ONE_OBJS) selftest.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-selftest selftest.o $(CORE_OBJS) $(ONE_OBJS $(LDLIBS)
	$(STRIP) zerotier-selftest

zerotier-selftest: selftest

manpages:	FORCE
	cd doc ; ./build.sh

doc:	manpages

clean: FORCE
	rm -rf *.a *.so *.o node/*.o controller/*.o osdep/*.o service/*.o ext/http-parser/*.o ext/miniupnpc/*.o ext/libnatpmp/*.o $(CORE_OBJS) $(ONE_OBJS) zerotier-one zerotier-idtool zerotier-cli zerotier-selftest build-* ZeroTierOneInstaller-* *.deb *.rpm .depend debian/files debian/zerotier-one*.debhelper debian/zerotier-one.substvars debian/*.log debian/zerotier-one doc/node_modules

distclean:	clean

realclean:	distclean

debug:	FORCE
	make ZT_DEBUG=1 one
	make ZT_DEBUG=1 selftest

# Note: keep the symlinks in /var/lib/zerotier-one to the binaries since these
# provide backward compatibility with old releases where the binaries actually
# lived here. Folks got scripts.

install:	FORCE
	mkdir -p $(DESTDIR)/usr/sbin
	rm -f $(DESTDIR)/usr/sbin/zerotier-one
	cp -f zerotier-one $(DESTDIR)/usr/sbin/zerotier-one
	rm -f $(DESTDIR)/usr/sbin/zerotier-cli
	rm -f $(DESTDIR)/usr/sbin/zerotier-idtool
	ln -s zerotier-one $(DESTDIR)/usr/sbin/zerotier-cli
	ln -s zerotier-one $(DESTDIR)/usr/sbin/zerotier-idtool
	mkdir -p $(DESTDIR)/var/lib/zerotier-one
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-one
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-cli
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-idtool
	ln -s ../../../usr/sbin/zerotier-one $(DESTDIR)/var/lib/zerotier-one/zerotier-one
	ln -s ../../../usr/sbin/zerotier-one $(DESTDIR)/var/lib/zerotier-one/zerotier-cli
	ln -s ../../../usr/sbin/zerotier-one $(DESTDIR)/var/lib/zerotier-one/zerotier-idtool
	mkdir -p $(DESTDIR)/usr/share/man/man8
	rm -f $(DESTDIR)/usr/share/man/man8/zerotier-one.8.gz
	cat doc/zerotier-one.8 | gzip -9 >$(DESTDIR)/usr/share/man/man8/zerotier-one.8.gz
	mkdir -p $(DESTDIR)/usr/share/man/man1
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-idtool.1.gz
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-cli.1.gz
	cat doc/zerotier-cli.1 | gzip -9 >$(DESTDIR)/usr/share/man/man1/zerotier-cli.1.gz
	cat doc/zerotier-idtool.1 | gzip -9 >$(DESTDIR)/usr/share/man/man1/zerotier-idtool.1.gz

# Uninstall preserves identity.public and identity.secret since the user might
# want to save these. These are your ZeroTier address.

uninstall:	FORCE
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-one
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-cli
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-idtool
	rm -f $(DESTDIR)/usr/sbin/zerotier-cli
	rm -f $(DESTDIR)/usr/sbin/zerotier-idtool
	rm -f $(DESTDIR)/usr/sbin/zerotier-one
	rm -rf $(DESTDIR)/var/lib/zerotier-one/iddb.d
	rm -rf $(DESTDIR)/var/lib/zerotier-one/updates.d
	rm -rf $(DESTDIR)/var/lib/zerotier-one/networks.d
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-one.port
	rm -f $(DESTDIR)/usr/share/man/man8/zerotier-one.8.gz
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-idtool.1.gz
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-cli.1.gz

# These are just for convenience for building Linux packages

debian:	FORCE
	debuild -I -i -us -uc -nc -b

redhat:	FORCE
	rpmbuild -ba zerotier-one.spec

FORCE:
