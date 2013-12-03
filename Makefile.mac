CC=clang
CXX=clang++

INCLUDES=
DEFS=
LIBS=-lm

# Uncomment for a release optimized universal binary build
CFLAGS=-arch i386 -arch x86_64 -Wall -O4 -pthread -mmacosx-version-min=10.6 -DNDEBUG -Wno-unused-private-field $(INCLUDES) $(DEFS)
STRIP=strip

# Uncomment for a debug build
#CFLAGS=-Wall -g -pthread -DZT_TRACE -DZT_LOG_STDOUT $(INCLUDES) $(DEFS)
#STRIP=echo

CXXFLAGS=$(CFLAGS) -fno-rtti

include objects.mk

all: one

one:	$(OBJS)
	$(CXX) $(CXXFLAGS) -o zerotier-one main.cpp $(OBJS) $(LIBS)
	$(STRIP) zerotier-one
	ln -sf zerotier-one zerotier-cli

selftest: $(OBJS)
	$(CXX) $(CXXFLAGS) -o zerotier-selftest selftest.cpp $(OBJS) $(LIBS)
	$(STRIP) zerotier-selftest

idtool: $(OBJS)
	$(CXX) $(CXXFLAGS) -o zerotier-idtool idtool.cpp $(OBJS) $(LIBS)
	$(STRIP) zerotier-idtool

install-mac-tap: FORCE
	mkdir -p /Library/Application\ Support/ZeroTier/One
	rm -rf /Library/Application\ Support/ZeroTier/One/tap.kext
	cp -R ext/bin/tap-mac//tap.kext /Library/Application\ Support/ZeroTier/One
	chown -R root:wheel /Library/Application\ Support/ZeroTier/One/tap.kext

file2lz4c: ext/lz4/lz4hc.o FORCE
	$(CXX) $(CXXFLAGS) -o file2lz4c file2lz4c.cpp node/Utils.cpp node/Salsa20.cpp ext/lz4/lz4hc.o

clean:
	rm -rf *.dSYM build-ZeroTierUI-* $(OBJS) file2lz4c zerotier-*

FORCE:
