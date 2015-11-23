# $Id: Makefile,v 1.132 2015/10/26 16:59:54 nanard Exp $
# MiniUPnP Project
# http://miniupnp.free.fr/
# http://miniupnp.tuxfamily.org/
# https://github.com/miniupnp/miniupnp
# (c) 2005-2015 Thomas Bernard
# to install use :
# $ make DESTDIR=/tmp/dummylocation install
# or
# $ INSTALLPREFIX=/usr/local make install
# or
# $ make install (default INSTALLPREFIX is /usr)
OS = $(shell uname -s)
VERSION = $(shell cat VERSION)

ifeq ($(OS), Darwin)
JARSUFFIX=mac
LIBTOOL ?= $(shell which libtool)
endif
ifeq ($(OS), Linux)
JARSUFFIX=linux
endif
ifneq (,$(findstring NT-5.1,$(OS)))
JARSUFFIX=win32
endif

HAVE_IPV6 ?= yes
export HAVE_IPV6

CC ?= gcc
#AR = gar
#CFLAGS = -O -g -DDEBUG
CFLAGS ?= -O
CFLAGS += -Wall
CFLAGS += -W -Wstrict-prototypes
CFLAGS += -fno-common
CFLAGS += -DMINIUPNPC_SET_SOCKET_TIMEOUT
CFLAGS += -DMINIUPNPC_GET_SRC_ADDR
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
ifneq ($(OS), FreeBSD)
ifneq ($(OS), Darwin)
#CFLAGS += -D_POSIX_C_SOURCE=200112L
CFLAGS += -D_XOPEN_SOURCE=600
endif
endif
#CFLAGS += -ansi
# -DNO_GETADDRINFO
INSTALL = install
SH = /bin/sh
JAVA = java
# see http://code.google.com/p/jnaerator/
#JNAERATOR = jnaerator-0.9.7.jar
#JNAERATOR = jnaerator-0.9.8-shaded.jar
#JNAERATORARGS = -library miniupnpc
#JNAERATOR = jnaerator-0.10-shaded.jar
#JNAERATOR = jnaerator-0.11-shaded.jar
# https://repo1.maven.org/maven2/com/nativelibs4java/jnaerator/0.12/jnaerator-0.12-shaded.jar
JNAERATOR = jnaerator-0.12-shaded.jar
JNAERATORARGS = -mode StandaloneJar -runtime JNAerator -library miniupnpc
#JNAERATORBASEURL = http://jnaerator.googlecode.com/files/
JNAERATORBASEURL = https://repo1.maven.org/maven2/com/nativelibs4java/jnaerator/0.12

ifeq (SunOS, $(OS))
  LDFLAGS=-lsocket -lnsl -lresolv
endif

# APIVERSION is used to build SONAME
APIVERSION = 15

SRCS = igd_desc_parse.c miniupnpc.c minixml.c minisoap.c miniwget.c \
       upnpc.c upnpcommands.c upnpreplyparse.c testminixml.c \
       minixmlvalid.c testupnpreplyparse.c minissdpc.c \
       upnperrors.c testigddescparse.c testminiwget.c \
       connecthostport.c portlistingparse.c receivedata.c \
       upnpdev.c testportlistingparse.c miniupnpcmodule.c \
       minihttptestserver.c \
       listdevices.c

LIBOBJS = miniwget.o minixml.o igd_desc_parse.o minisoap.o \
          miniupnpc.o upnpreplyparse.o upnpcommands.o upnperrors.o \
          connecthostport.o portlistingparse.o receivedata.o upnpdev.o

ifneq ($(OS), AmigaOS)
CFLAGS := -fPIC $(CFLAGS)
LIBOBJS := $(LIBOBJS) minissdpc.o
endif

OBJS = $(patsubst %.c,%.o,$(SRCS))

# HEADERS to install
HEADERS = miniupnpc.h miniwget.h upnpcommands.h igd_desc_parse.h \
          upnpreplyparse.h upnperrors.h miniupnpctypes.h \
          portlistingparse.h \
          upnpdev.h \
          miniupnpc_declspec.h

# library names
LIBRARY = libminiupnpc.a
ifeq ($(OS), Darwin)
  SHAREDLIBRARY = libminiupnpc.dylib
  SONAME = $(basename $(SHAREDLIBRARY)).$(APIVERSION).dylib
  CFLAGS := -D_DARWIN_C_SOURCE $(CFLAGS)
else
ifeq ($(JARSUFFIX), win32)
  SHAREDLIBRARY = miniupnpc.dll
else
  # Linux/BSD/etc.
  SHAREDLIBRARY = libminiupnpc.so
  SONAME = $(SHAREDLIBRARY).$(APIVERSION)
endif
endif

EXECUTABLES = upnpc-static listdevices
EXECUTABLES_ADDTESTS = testminixml minixmlvalid testupnpreplyparse \
			  testigddescparse testminiwget testportlistingparse

TESTMINIXMLOBJS = minixml.o igd_desc_parse.o testminixml.o

TESTMINIWGETOBJS = miniwget.o testminiwget.o connecthostport.o receivedata.o

TESTUPNPREPLYPARSE = testupnpreplyparse.o minixml.o upnpreplyparse.o

TESTPORTLISTINGPARSE = testportlistingparse.o minixml.o portlistingparse.o

TESTIGDDESCPARSE = testigddescparse.o igd_desc_parse.o minixml.o \
                   miniupnpc.o miniwget.o upnpcommands.o upnpreplyparse.o \
                   minisoap.o connecthostport.o receivedata.o \
                   portlistingparse.o

ifneq ($(OS), AmigaOS)
EXECUTABLES := $(EXECUTABLES) upnpc-shared
TESTMINIWGETOBJS := $(TESTMINIWGETOBJS) minissdpc.o
TESTIGDDESCPARSE := $(TESTIGDDESCPARSE) minissdpc.o
endif

LIBDIR ?= lib
# install directories
INSTALLPREFIX ?= $(PREFIX)/usr
INSTALLDIRINC = $(INSTALLPREFIX)/include/miniupnpc
INSTALLDIRLIB = $(INSTALLPREFIX)/$(LIBDIR)
INSTALLDIRBIN = $(INSTALLPREFIX)/bin
INSTALLDIRMAN = $(INSTALLPREFIX)/share/man

FILESTOINSTALL = $(LIBRARY) $(EXECUTABLES)
ifneq ($(OS), AmigaOS)
FILESTOINSTALL := $(FILESTOINSTALL) $(SHAREDLIBRARY)
endif


.PHONY:	install clean depend all check test everything \
	installpythonmodule updateversion
#	validateminixml validateminiwget

all:	$(LIBRARY) $(EXECUTABLES)

test:	check

check:	validateminixml validateminiwget validateupnpreplyparse \
	validateportlistingparse validateigddescparse

everything:	all $(EXECUTABLES_ADDTESTS)

pythonmodule:	$(LIBRARY) miniupnpcmodule.c setup.py
	python setup.py build
	touch $@

installpythonmodule:	pythonmodule
	python setup.py install

pythonmodule3:	$(LIBRARY) miniupnpcmodule.c setup.py
	python3 setup.py build
	touch $@

installpythonmodule3:	pythonmodule3
	python3 setup.py install

validateminixml:	minixmlvalid
	@echo "minixml validation test"
	./minixmlvalid
	touch $@

validateminiwget:	testminiwget minihttptestserver testminiwget.sh
	@echo "miniwget validation test"
	./testminiwget.sh
	touch $@

validateupnpreplyparse:	testupnpreplyparse testupnpreplyparse.sh
	@echo "upnpreplyparse validation test"
	./testupnpreplyparse.sh
	touch $@

validateportlistingparse:	testportlistingparse
	@echo "portlistingparse validation test"
	./testportlistingparse
	touch $@

validateigddescparse:	testigddescparse
	@echo "igd desc parse validation test"
	./testigddescparse testdesc/new_LiveBox_desc.xml testdesc/new_LiveBox_desc.values
	./testigddescparse testdesc/linksys_WAG200G_desc.xml testdesc/linksys_WAG200G_desc.values
	touch $@

clean:
	$(RM) $(LIBRARY) $(SHAREDLIBRARY) $(EXECUTABLES) $(OBJS) miniupnpcstrings.h
	$(RM) $(EXECUTABLES_ADDTESTS)
	# clean python stuff
	$(RM) pythonmodule pythonmodule3
	$(RM) validateminixml validateminiwget validateupnpreplyparse
	$(RM) validateigddescparse
	$(RM) minihttptestserver
	$(RM) -r build/ dist/
	#python setup.py clean
	# clean jnaerator stuff
	$(RM) _jnaerator.* java/miniupnpc_$(OS).jar

distclean: clean
	$(RM) $(JNAERATOR) java/*.jar java/*.class out.errors.txt

updateversion:	miniupnpc.h
	cp miniupnpc.h miniupnpc.h.bak
	sed 's/\(.*MINIUPNPC_API_VERSION\s\+\)[0-9]\+/\1$(APIVERSION)/' < miniupnpc.h.bak > miniupnpc.h

install:	updateversion $(FILESTOINSTALL)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(DESTDIR)$(INSTALLDIRLIB)
ifneq ($(OS), AmigaOS)
	$(INSTALL) -m 644 $(SHAREDLIBRARY) $(DESTDIR)$(INSTALLDIRLIB)/$(SONAME)
	ln -fs $(SONAME) $(DESTDIR)$(INSTALLDIRLIB)/$(SHAREDLIBRARY)
endif
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRBIN)
ifeq ($(OS), AmigaOS)
	$(INSTALL) -m 755 upnpc-static $(DESTDIR)$(INSTALLDIRBIN)/upnpc
else
	$(INSTALL) -m 755 upnpc-shared $(DESTDIR)$(INSTALLDIRBIN)/upnpc
endif
	$(INSTALL) -m 755 external-ip.sh $(DESTDIR)$(INSTALLDIRBIN)/external-ip
ifneq ($(OS), AmigaOS)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRMAN)/man3
	$(INSTALL) -m 644 man3/miniupnpc.3 $(DESTDIR)$(INSTALLDIRMAN)/man3/miniupnpc.3
ifeq ($(OS), Linux)
	gzip -f $(DESTDIR)$(INSTALLDIRMAN)/man3/miniupnpc.3
endif
endif

install-static:	updateversion $(FILESTOINSTALL)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRBIN)
	$(INSTALL) -m 755 external-ip.sh $(DESTDIR)$(INSTALLDIRBIN)/external-ip

cleaninstall:
	$(RM) -r $(DESTDIR)$(INSTALLDIRINC)
	$(RM) $(DESTDIR)$(INSTALLDIRLIB)/$(LIBRARY)
	$(RM) $(DESTDIR)$(INSTALLDIRLIB)/$(SHAREDLIBRARY)

depend:
	makedepend -Y -- $(CFLAGS) -- $(SRCS) 2>/dev/null

$(LIBRARY):	$(LIBOBJS)
ifeq ($(OS), Darwin)
	$(LIBTOOL) -static -o $@ $?
else
	$(AR) crs $@ $?
endif

$(SHAREDLIBRARY):	$(LIBOBJS)
ifeq ($(OS), Darwin)
#	$(CC) -dynamiclib $(LDFLAGS) -Wl,-install_name,$(SONAME) -o $@ $^
	$(CC) -dynamiclib $(LDFLAGS) -Wl,-install_name,$(INSTALLDIRLIB)/$(SONAME) -o $@ $^
else
	$(CC) -shared $(LDFLAGS) -Wl,-soname,$(SONAME) -o $@ $^
endif

upnpc-static:	upnpc.o $(LIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

upnpc-shared:	upnpc.o $(SHAREDLIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

listdevices:	listdevices.o $(LIBRARY)

testminixml:	$(TESTMINIXMLOBJS)

testminiwget:	$(TESTMINIWGETOBJS)

minixmlvalid:	minixml.o minixmlvalid.o

testupnpreplyparse:	$(TESTUPNPREPLYPARSE)

testigddescparse:	$(TESTIGDDESCPARSE)

testportlistingparse:	$(TESTPORTLISTINGPARSE)

miniupnpcstrings.h:	miniupnpcstrings.h.in updateminiupnpcstrings.sh VERSION
	$(SH) updateminiupnpcstrings.sh

# ftp tool supplied with OpenBSD can download files from http.
jnaerator-%.jar:
	wget $(JNAERATORBASEURL)/$@ || \
	curl -o $@ $(JNAERATORBASEURL)/$@ || \
	ftp $(JNAERATORBASEURL)/$@

jar: $(SHAREDLIBRARY)  $(JNAERATOR)
	$(JAVA) -jar $(JNAERATOR) $(JNAERATORARGS) \
	miniupnpc.h miniupnpc_declspec.h upnpcommands.h upnpreplyparse.h \
	igd_desc_parse.h miniwget.h upnperrors.h $(SHAREDLIBRARY) \
	-package fr.free.miniupnp -o . -jar java/miniupnpc_$(JARSUFFIX).jar -v

mvn_install:
	mvn install:install-file -Dfile=java/miniupnpc_$(JARSUFFIX).jar \
	 -DgroupId=com.github \
	 -DartifactId=miniupnp \
	 -Dversion=$(VERSION) \
	 -Dpackaging=jar \
	 -Dclassifier=$(JARSUFFIX) \
	 -DgeneratePom=true \
	 -DcreateChecksum=true

# make .deb packages
deb: /usr/share/pyshared/stdeb all
	(python setup.py --command-packages=stdeb.command bdist_deb)

# install .deb packages
ideb:
	(sudo dpkg -i deb_dist/*.deb)

/usr/share/pyshared/stdeb: /usr/share/doc/python-all-dev
	(sudo apt-get install python-stdeb)

/usr/share/doc/python-all-dev:
	(sudo apt-get install python-all-dev)

minihttptestserver:	minihttptestserver.o

# DO NOT DELETE THIS LINE -- make depend depends on it.

igd_desc_parse.o: igd_desc_parse.h
miniupnpc.o: miniupnpc.h miniupnpc_declspec.h igd_desc_parse.h upnpdev.h
miniupnpc.o: minissdpc.h miniwget.h minisoap.h minixml.h upnpcommands.h
miniupnpc.o: upnpreplyparse.h portlistingparse.h miniupnpctypes.h
miniupnpc.o: connecthostport.h
minixml.o: minixml.h
minisoap.o: minisoap.h miniupnpcstrings.h
miniwget.o: miniupnpcstrings.h miniwget.h miniupnpc_declspec.h
miniwget.o: connecthostport.h receivedata.h
upnpc.o: miniwget.h miniupnpc_declspec.h miniupnpc.h igd_desc_parse.h
upnpc.o: upnpdev.h upnpcommands.h upnpreplyparse.h portlistingparse.h
upnpc.o: miniupnpctypes.h upnperrors.h miniupnpcstrings.h
upnpcommands.o: upnpcommands.h upnpreplyparse.h portlistingparse.h
upnpcommands.o: miniupnpc_declspec.h miniupnpctypes.h miniupnpc.h
upnpcommands.o: igd_desc_parse.h upnpdev.h
upnpreplyparse.o: upnpreplyparse.h minixml.h
testminixml.o: minixml.h igd_desc_parse.h
minixmlvalid.o: minixml.h
testupnpreplyparse.o: upnpreplyparse.h
minissdpc.o: minissdpc.h miniupnpc_declspec.h upnpdev.h miniupnpc.h
minissdpc.o: igd_desc_parse.h receivedata.h codelength.h
upnperrors.o: upnperrors.h miniupnpc_declspec.h upnpcommands.h
upnperrors.o: upnpreplyparse.h portlistingparse.h miniupnpctypes.h
upnperrors.o: miniupnpc.h igd_desc_parse.h upnpdev.h
testigddescparse.o: igd_desc_parse.h minixml.h miniupnpc.h
testigddescparse.o: miniupnpc_declspec.h upnpdev.h
testminiwget.o: miniwget.h miniupnpc_declspec.h
connecthostport.o: connecthostport.h
portlistingparse.o: portlistingparse.h miniupnpc_declspec.h miniupnpctypes.h
portlistingparse.o: minixml.h
receivedata.o: receivedata.h
upnpdev.o: upnpdev.h miniupnpc_declspec.h
testportlistingparse.o: portlistingparse.h miniupnpc_declspec.h
testportlistingparse.o: miniupnpctypes.h
miniupnpcmodule.o: miniupnpc.h miniupnpc_declspec.h igd_desc_parse.h
miniupnpcmodule.o: upnpdev.h upnpcommands.h upnpreplyparse.h
miniupnpcmodule.o: portlistingparse.h miniupnpctypes.h upnperrors.h
listdevices.o: miniupnpc.h miniupnpc_declspec.h igd_desc_parse.h upnpdev.h
