# This requires GNU make, which is typically "gmake" on BSD systems

INCLUDES=-isystem ext
DEFS=
LIBS=

include objects.mk
ONE_OBJS+=osdep/BSDEthernetTap.o ext/http-parser/http_parser.o

ifeq ($(OSTYPE),FreeBSD)
	# Auto-detect miniupnpc and nat-pmp as well and use ports libs if present,
	# otherwise build into binary as done on Mac and Windows.
	INCLUDES+=-I/usr/local/include
	LIBS+=-L/usr/local/lib
	ONE_OBJS+=osdep/PortMapper.o
	override DEFS+=-DZT_USE_MINIUPNPC
	MINIUPNPC_IS_NEW_ENOUGH=$(shell grep -sqr '.*define.*MINIUPNPC_VERSION.*"2..*"' /usr/local/include/miniupnpc/miniupnpc.h && echo 1)
	ifeq ($(MINIUPNPC_IS_NEW_ENOUGH),1)
		LIBS+=-lminiupnpc
		override DEFS+=-DZT_USE_SYSTEM_MINIUPNPC
	else
		override DEFS+=-DMINIUPNP_STATICLIB -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_BSD_SOURCE -D_DEFAULT_SOURCE -DOS_STRING=\"FreeBSD/$(shell uname -r)\" -DMINIUPNPC_VERSION_STRING=\"2.0\" -DUPNP_VERSION_STRING=\"UPnP/1.1\" -DENABLE_STRNATPMPERR
		ONE_OBJS+=ext/miniupnpc/connecthostport.o ext/miniupnpc/igd_desc_parse.o ext/miniupnpc/minisoap.o ext/miniupnpc/minissdpc.o ext/miniupnpc/miniupnpc.o ext/miniupnpc/miniwget.o ext/miniupnpc/minixml.o ext/miniupnpc/portlistingparse.o ext/miniupnpc/receivedata.o ext/miniupnpc/upnpcommands.o ext/miniupnpc/upnpdev.o ext/miniupnpc/upnperrors.o ext/miniupnpc/upnpreplyparse.o
	endif
	ifeq ($(wildcard /usr/local/include/natpmp.h),)
		ONE_OBJS+=ext/libnatpmp/natpmp.o ext/libnatpmp/getgateway.o
	else
		LIBS+=-lnatpmp
		override DEFS+=-DZT_USE_SYSTEM_NATPMP
	endif
endif

# Build with address sanitization library for advanced debugging (clang)
ifeq ($(ZT_SANITIZE),1)
	SANFLAGS+=-fsanitize=address -DASAN_OPTIONS=symbolize=1
endif
# "make debug" is a shortcut for this
ifeq ($(ZT_DEBUG),1)
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	LDFLAGS+=
	STRIP=echo
	ZT_TRACE=1
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-O3 -fstack-protector
	CFLAGS+=-Wall -fPIE -fvisibility=hidden -fstack-protector -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	LDFLAGS+=-pie -Wl,-z,relro,-z,now
	STRIP=strip --strip-all
endif

ifeq ($(ZT_TRACE),1)
	DEFS+=-DZT_TRACE
endif

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
	override DEFS+=-DZT_NO_TYPE_PUNNING -DZT_NO_UNALIGNED_ACCESS
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
	override DEFS+=-DZT_NO_TYPE_PUNNING -DZT_AES_NO_ACCEL -DZT_NO_UNALIGNED_ACCESS
	ZT_USE_ARM32_NEON_ASM_SALSA2012=1
endif
ifeq ($(CC_MACH),arm64)
	ZT_ARCHITECTURE=4
	override DEFS+=-DZT_NO_TYPE_PUNNING -march=armv8-a+crypto
endif
ifeq ($(CC_MACH),aarch64)
	ZT_ARCHITECTURE=4
	override DEFS+=-DZT_NO_TYPE_PUNNING -march=armv8-a+crypto
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

# Build faster crypto on some targets
ifeq ($(ZT_USE_X64_ASM_SALSA2012),1)
	override DEFS+=-DZT_USE_X64_ASM_SALSA2012
	override CORE_OBJS+=ext/x64-salsa2012-asm/salsa2012.o
endif
ifeq ($(ZT_USE_ARM32_NEON_ASM_SALSA2012),1)
	override DEFS+=-DZT_USE_ARM32_NEON_ASM_SALSA2012
	override CORE_OBJS+=ext/arm32-neon-salsa2012-asm/salsa2012.o
	override ASFLAGS+=-meabi=5
	override LDFLAGS+=-Wl,-z,notext
endif

override DEFS+=-DZT_BUILD_PLATFORM=$(ZT_BUILD_PLATFORM) -DZT_BUILD_ARCHITECTURE=$(ZT_ARCHITECTURE) -DZT_SOFTWARE_UPDATE_DEFAULT="\"disable\""

CXXFLAGS+=$(CFLAGS) -std=c++17 #-D_GLIBCXX_USE_C99 -D_GLIBCXX_USE_C99_MATH -D_GLIBCXX_USE_C99_MATH_TR1

all:	one

one:	$(CORE_OBJS) $(ONE_OBJS) one.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-one $(CORE_OBJS) $(ONE_OBJS) one.o $(LIBS)
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
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-selftest selftest.o $(CORE_OBJS) $(ONE_OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

zerotier-selftest: selftest

clean:
	rm -rf *.a *.o node/*.o controller/*.o osdep/*.o service/*.o ext/http-parser/*.o build-* zerotier-one zerotier-idtool zerotier-selftest zerotier-cli $(ONE_OBJS) $(CORE_OBJS)

debug:	FORCE
	$(MAKE) -j ZT_DEBUG=1

install:	one
	rm -f /usr/local/sbin/zerotier-one
	cp zerotier-one /usr/local/sbin
	ln -sf /usr/local/sbin/zerotier-one /usr/local/sbin/zerotier-cli
	ln -sf /usr/local/sbin/zerotier-one /usr/local/bin/zerotier-idtool

uninstall:	FORCE
	rm -rf /usr/local/sbin/zerotier-one /usr/local/sbin/zerotier-cli /usr/local/bin/zerotier-idtool /var/db/zerotier-one/zerotier-one.port /var/db/zerotier-one/zerotier-one.pid /var/db/zerotier-one/iddb.d

FORCE:
