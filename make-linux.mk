#
# Makefile for ZeroTier One on Linux
#
# This is confirmed to work on distributions newer than CentOS 6 (the
# one used for reference builds) and on 32 and 64 bit x86 and ARM
# machines. It should also work on other 'normal' machines and recent
# distributions. Editing might be required for tiny devices or weird
# distros.
#
# Targets
#   one: zerotier-one and symlinks (cli and idtool)
#   all: builds 'one'
#   selftest: zerotier-selftest
#   debug: builds 'one' and 'selftest' with tracing and debug flags
#   installer: ZeroTierOneInstaller-... and packages (if possible)
#   official: builds 'one' and 'installer'
#   clean: removes all built files, objects, other trash
#

# Automagically pick clang or gcc, with preference for clang
# This is only done if we have not overridden these with an environment or CLI variable
ifeq ($(origin CC),default)
	CC=$(shell if [ -e /usr/bin/clang ]; then echo clang; else echo gcc; fi)
endif
ifeq ($(origin CXX),default)
	CXX=$(shell if [ -e /usr/bin/clang++ ]; then echo clang++; else echo g++; fi)
endif

#UNAME_M=$(shell $(CC) -dumpmachine | cut -d '-' -f 1)

INCLUDES=-Iext/lwip/src/include -Iext/lwip/src/include/ipv4 -Iext/lwip/src/include/ipv6
DEFS=-DZT_ENABLE_NETCON
#CXXFLAGS+=-Wc++11-compat-deprecated-writable-strings -Wformat
LDLIBS?=

include objects.mk
OBJS+=osdep/LinuxEthernetTap.o netcon/NetconEthernetTap.o

# "make official" is a shortcut for this
ifeq ($(ZT_OFFICIAL_RELEASE),1)
	DEFS+=-DZT_OFFICIAL_RELEASE
	ZT_USE_MINIUPNPC=1
endif

ifeq ($(ZT_USE_MINIUPNPC),1)
	DEFS+=-DZT_USE_MINIUPNPC
	LDLIBS+=ext/miniupnpc/libminiupnpc.a
	OBJS+=osdep/UPNPClient.o
endif

# Build with ZT_ENABLE_NETWORK_CONTROLLER=1 to build with the Sqlite network controller
ifeq ($(ZT_ENABLE_NETWORK_CONTROLLER),1)
	DEFS+=-DZT_ENABLE_NETWORK_CONTROLLER
	LDLIBS+=-L/usr/local/lib -lsqlite3
	OBJS+=controller/SqliteNetworkController.o
endif

# Build with ZT_ENABLE_CLUSTER=1 to build with cluster support
ifeq ($(ZT_ENABLE_CLUSTER),1)
	DEFS+=-DZT_ENABLE_CLUSTER
endif

# "make debug" is a shortcut for this
ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_TRACE
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	CXXFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	LDFLAGS=-ldl
	STRIP=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in -O0 even on a 3ghz box!
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-O3 -fstack-protector
	CFLAGS+=-Wall -fPIE -fvisibility=hidden -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	CXXFLAGS?=-O3 -fstack-protector
	CXXFLAGS+=-Wall -Wreorder -fPIE -fvisibility=hidden -fno-rtti -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	LDFLAGS=-ldl -pie -Wl,-z,relro,-z,now
	STRIP=strip --strip-all
endif

ifeq ($(ZT_TRACE),1)
	DEFS+=-DZT_TRACE
endif

# Uncomment for gprof profile build
#CFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#CXXFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#LDFLAGS=
#STRIP=echo

all:	one

one:	$(OBJS) one.o
ifeq ($(ZT_USE_MINIUPNPC),1)
	cd ext/miniupnpc ; make clean ; make 'CFLAGS=-O2 -fstack-protector -fPIE -fno-common -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600' -j 2 libminiupnpc.a
endif
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-one $(OBJS) one.o $(LDLIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-idtool
	ln -sf zerotier-one zerotier-cli

selftest:	$(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LDLIBS)
	$(STRIP) zerotier-selftest

installer: one FORCE
	./ext/installfiles/linux/buildinstaller.sh

clean:
	find ./ -type f -name '*.o' -delete
	find netcon/ -type f -name '*.so' -delete
	find netcon/ -type f -name '*.1.0' -delete
	find netcon/ -type f -name 'zerotier-one' -delete
	find netcon/ -type f -name 'zerotier-cli' -delete
	find netcon/docker-test -name "zerotier-intercept" -type f -delete
	rm -rf zerotier-one zerotier-idtool zerotier-cli zerotier-selftest zerotier-netcon build-* ZeroTierOneInstaller-* *.deb *.rpm *.pkg *.tgz
	cd ext/miniupnpc ; make clean

debug:	FORCE
	make ZT_DEBUG=1 one
	make ZT_DEBUG=1 selftest

official: FORCE
	make -j 4 ZT_OFFICIAL_RELEASE=1
	make ZT_OFFICIAL_RELEASE=1 installer

FORCE:
