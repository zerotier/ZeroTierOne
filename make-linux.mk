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
#   doc: builds manpages, requires rst2man somewhere in PATH
#   all: builds 'one'
#   selftest: zerotier-selftest
#   debug: builds 'one' and 'selftest' with tracing and debug flags
#   installer: builds installers and packages (RPM/DEB/etc.) if possible
#   official: cleans and then builds 'one', 'installer', and 'doc'
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

INCLUDES?=
DEFS?=-D_FORTIFY_SOURCE=2
LDLIBS?=
DESTDIR?=

include objects.mk

# On Linux we auto-detect the presence of some libraries
ifeq ($(wildcard /usr/include/lz4.h),)
	OBJS+=ext/lz4/lz4.o
else
	LDLIBS+=-llz4
	DEFS+=-DZT_USE_SYSTEM_LZ4
endif
ifeq ($(wildcard /usr/include/http_parser.h),)
	OBJS+=ext/http-parser/http_parser.o
else
	LDLIBS+=-lhttp_parser
	DEFS+=-DZT_USE_SYSTEM_HTTP_PARSER
endif
ifeq ($(wildcard /usr/include/json-parser/json.h),)
	OBJS+=ext/json-parser/json.o
else
	LDLIBS+=-ljsonparser
	DEFS+=-DZT_USE_SYSTEM_JSON_PARSER
endif

ifeq ($(ZT_OFFICIAL_RELEASE),1)
	DEFS+=-DZT_OFFICIAL_RELEASE
	ZT_USE_MINIUPNPC=1
endif

ifeq ($(ZT_USE_MINIUPNPC),1)
	DEFS+=-DZT_USE_MINIUPNPC -DMINIUPNP_STATICLIB -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -DOS_STRING=\"Linux\" -DMINIUPNPC_VERSION_STRING=\"2.0\" -DUPNP_VERSION_STRING=\"UPnP/1.1\" -DENABLE_STRNATPMPERR
	OBJS+=osdep/PortMapper.o

	# We always use ext/miniupnpc because versions that ship with various Linux distributions are too old
	#ifeq ($(wildcard /usr/include/miniupnpc/miniupnpc.h),)
		OBJS+=ext/miniupnpc/connecthostport.o ext/miniupnpc/igd_desc_parse.o ext/miniupnpc/minisoap.o ext/miniupnpc/minissdpc.o ext/miniupnpc/miniupnpc.o ext/miniupnpc/miniwget.o ext/miniupnpc/minixml.o ext/miniupnpc/portlistingparse.o ext/miniupnpc/receivedata.o ext/miniupnpc/upnpcommands.o ext/miniupnpc/upnpdev.o ext/miniupnpc/upnperrors.o ext/miniupnpc/upnpreplyparse.o
	#else
	#	LDLIBS+=-lminiupnpc
	#endif

	ifeq ($(wildcard /usr/include/natpmp.h),)
		OBJS+=ext/libnatpmp/natpmp.o ext/libnatpmp/getgateway.o
	else
		LDLIBS+=-lnatpmp
		DEFS+=-DZT_USE_SYSTEM_NATPMP
	endif
endif

ifeq ($(ZT_ENABLE_NETWORK_CONTROLLER),1)
	DEFS+=-DZT_ENABLE_NETWORK_CONTROLLER
	LDLIBS+=-L/usr/local/lib -lsqlite3
	OBJS+=controller/SqliteNetworkController.o
endif

ifeq ($(ZT_ENABLE_CLUSTER),1)
	DEFS+=-DZT_ENABLE_CLUSTER
endif

ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_TRACE
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	CXXFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	LDFLAGS=
	STRIP?=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in -O0 even on a 3ghz box!
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-O3 -fstack-protector-strong
	CFLAGS+=-Wall -fPIE -fvisibility=hidden -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	CXXFLAGS?=-O3 -fstack-protector-strong
	CXXFLAGS+=-Wall -Wreorder -fPIE -fvisibility=hidden -fno-rtti -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	LDFLAGS=-pie -Wl,-z,relro,-z,now
	STRIP?=strip
	STRIP+=--strip-all
endif

ifeq ($(ZT_TRACE),1)
	DEFS+=-DZT_TRACE
endif

# Uncomment for gprof profile build
#CFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#CXXFLAGS=-Wall -g -pg -pthread $(INCLUDES) $(DEFS)
#LDFLAGS=
#STRIP=echo

all:	one manpages

one:	$(OBJS) service/OneService.o one.o osdep/LinuxEthernetTap.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-one $(OBJS) service/OneService.o one.o osdep/LinuxEthernetTap.o $(LDLIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-idtool
	ln -sf zerotier-one zerotier-cli

netcon: $(OBJS)
	rm -f *.o
	# Need to selectively rebuild one.cpp and OneService.cpp with ZT_SERVICE_NETCON and ZT_ONE_NO_ROOT_CHECK defined, and also NetconEthernetTap
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -DZT_SERVICE_NETCON -DZT_ONE_NO_ROOT_CHECK -Iext/lwip/src/include -Iext/lwip/src/include/ipv4 -Iext/lwip/src/include/ipv6 -o zerotier-netcon-service $(OBJS) service/OneService.cpp netcon/NetconEthernetTap.cpp one.cpp -x c netcon/RPC.c $(LDLIBS) -ldl
	# Build netcon/liblwip.so which must be placed in ZT home for zerotier-netcon-service to work
	cd netcon ; make -f make-liblwip.mk
	# Use gcc not clang to build standalone intercept library since gcc is typically used for libc and we want to ensure maximal ABI compatibility
	cd netcon ; gcc -g -O2 -Wall -std=c99 -fPIC -DVERBOSE -D_GNU_SOURCE -DNETCON_INTERCEPT -I. -nostdlib -shared -o libzerotierintercept.so Intercept.c RPC.c -ldl
	cp netcon/libzerotierintercept.so libzerotierintercept.so
	ln -sf zerotier-netcon-service zerotier-cli
	ln -sf zerotier-netcon-service zerotier-idtool

selftest:	$(OBJS) selftest.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o zerotier-selftest selftest.o $(OBJS) $(LDLIBS)
	$(STRIP) zerotier-selftest

manpages:	FORCE
	cd doc ; ./build.sh

clean: FORCE
	rm -rf *.so *.o netcon/*.a node/*.o controller/*.o osdep/*.o service/*.o ext/http-parser/*.o ext/lz4/*.o ext/json-parser/*.o ext/miniupnpc/*.o ext/libnatpmp/*.o $(OBJS) zerotier-one zerotier-idtool zerotier-cli zerotier-selftest zerotier-netcon-service build-* ZeroTierOneInstaller-* *.deb *.rpm .depend netcon/.depend doc/*.1 doc/*.2 doc/*.8 debian/files debian/zerotier-one*.debhelper debian/zerotier-one.substvars debian/*.log debian/zerotier-one
	find netcon -type f \( -name '*.o' -o -name '*.so' -o -name '*.1.0' -o -name 'zerotier-one' -o -name 'zerotier-cli' -o -name 'zerotier-netcon-service' \) -delete
	find netcon/docker-test -name "zerotier-intercept" -type f -delete

distclean:	clean
	rm -rf doc/node_modules

debug:	FORCE
	make ZT_DEBUG=1 one
	make ZT_DEBUG=1 selftest

install:	FORCE
	mkdir -p $(DESTDIR)/usr/sbin
	rm -f $(DESTDIR)/usr/sbin/zerotier-one
	cp -f zerotier-one $(DESTDIR)/usr/sbin/zerotier-one
	mkdir -p $(DESTDIR)/usr/bin
	rm -f $(DESTDIR)/usr/bin/zerotier-cli
	rm -f $(DESTDIR)/usr/bin/zerotier-idtool
	ln -rs $(DESTDIR)/usr/sbin/zerotier-one $(DESTDIR)/usr/bin/zerotier-cli
	ln -rs $(DESTDIR)/usr/sbin/zerotier-one $(DESTDIR)/usr/bin/zerotier-idtool
	mkdir -p $(DESTDIR)/var/lib/zerotier-one
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-one
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-cli
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-idtool
	ln -rs $(DESTDIR)/usr/sbin/zerotier-one $(DESTDIR)/var/lib/zerotier-one/zerotier-one
	ln -rs $(DESTDIR)/usr/sbin/zerotier-one $(DESTDIR)/var/lib/zerotier-one/zerotier-cli
	ln -rs $(DESTDIR)/usr/sbin/zerotier-one $(DESTDIR)/var/lib/zerotier-one/zerotier-idtool
	mkdir -p $(DESTDIR)/usr/share/man/man8
	rm -f $(DESTDIR)/usr/share/man/man8/zerotier-one.8.gz
	cat doc/zerotier-one.8 | gzip -9 >$(DESTDIR)/usr/share/man/man8/zerotier-one.8.gz
	mkdir -p $(DESTDIR)/usr/share/man/man1
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-idtool.1.gz
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-cli.1.gz
	cat doc/zerotier-cli.1 | gzip -9 >$(DESTDIR)/usr/share/man/man1/zerotier-cli.1.gz
	cat doc/zerotier-idtool.1 | gzip -9 >$(DESTDIR)/usr/share/man/man1/zerotier-idtool.1.gz

uninstall:	FORCE
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-one
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-cli
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-idtool
	rm -f $(DESTDIR)/usr/bin/zerotier-cli
	rm -f $(DESTDIR)/usr/bin/zerotier-idtool
	rm -f $(DESTDIR)/usr/sbin/zerotier-one
	rm -rf $(DESTDIR)/var/lib/zerotier-one/iddb.d
	rm -rf $(DESTDIR)/var/lib/zerotier-one/updates.d
	rm -rf $(DESTDIR)/var/lib/zerotier-one/networks.d
	rm -f $(DESTDIR)/var/lib/zerotier-one/zerotier-one.port
	rm -f $(DESTDIR)/usr/share/man/man8/zerotier-one.8.gz
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-idtool.1.gz
	rm -f $(DESTDIR)/usr/share/man/man1/zerotier-cli.1.gz

debian:	distclean
	debuild -I -i -us -uc

FORCE:
