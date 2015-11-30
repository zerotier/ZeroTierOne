# $Id: Makefile,v 1.23 2013/11/26 16:38:15 nanard Exp $
# This Makefile is designed for use with GNU make
# libnatpmp
# (c) 2007-2013 Thomas Bernard
# http://miniupnp.free.fr/libnatpmp.html

OS = $(shell uname -s)
CC = gcc
INSTALL = install -p
ARCH = $(shell uname -m | sed -e s/i.86/i686/)
VERSION = $(shell cat VERSION)

ifeq ($(OS), Darwin)
JARSUFFIX=mac
endif
ifeq ($(OS), Linux)
JARSUFFIX=linux
endif
ifneq (,$(findstring WIN,$(OS)))
JARSUFFIX=win32
endif

# APIVERSION is used in soname
APIVERSION = 1
#LDFLAGS = -Wl,--no-undefined
CFLAGS ?= -Os
#CFLAGS = -g -O0
CFLAGS += -fPIC
CFLAGS += -Wall
#CFLAGS += -Wextra
CFLAGS += -DENABLE_STRNATPMPERR
#CFLAGS += -Wstrict-aliasing

LIBOBJS = natpmp.o getgateway.o

OBJS = $(LIBOBJS) testgetgateway.o natpmpc.o natpmp-jni.o

STATICLIB = libnatpmp.a
ifeq ($(OS), Darwin)
  SHAREDLIB = libnatpmp.dylib
  JNISHAREDLIB = libjninatpmp.dylib
  SONAME = $(basename $(SHAREDLIB)).$(APIVERSION).dylib
  CFLAGS := -DMACOSX -D_DARWIN_C_SOURCE $(CFLAGS)
  SONAMEFLAGS=-Wl,-install_name,$(JNISHAREDLIB) -dynamiclib -framework JavaVM
else
ifneq (,$(findstring WIN,$(OS)))
  SHAREDLIB = natpmp.dll
  JNISHAREDLIB = jninatpmp.dll
  CC = i686-w64-mingw32-gcc
  EXTRA_LD = -lws2_32 -lIphlpapi -Wl,--no-undefined -Wl,--enable-runtime-pseudo-reloc --Wl,kill-at
else
  SHAREDLIB = libnatpmp.so
  JNISHAREDLIB = libjninatpmp.so
  SONAME = $(SHAREDLIB).$(APIVERSION)
  SONAMEFLAGS=-Wl,-soname,$(JNISHAREDLIB)
endif
endif

HEADERS = natpmp.h

EXECUTABLES = testgetgateway natpmpc-shared natpmpc-static

INSTALLPREFIX ?= $(PREFIX)/usr
INSTALLDIRINC = $(INSTALLPREFIX)/include
INSTALLDIRLIB = $(INSTALLPREFIX)/lib
INSTALLDIRBIN = $(INSTALLPREFIX)/bin

JAVA ?= java
JAVAC ?= javac
JAVAH ?= javah
JAVAPACKAGE = fr/free/miniupnp/libnatpmp
JAVACLASSES = $(JAVAPACKAGE)/NatPmp.class $(JAVAPACKAGE)/NatPmpResponse.class $(JAVAPACKAGE)/LibraryExtractor.class $(JAVAPACKAGE)/URLUtils.class
JNIHEADERS = fr_free_miniupnp_libnatpmp_NatPmp.h

.PHONY:	all clean depend install cleaninstall installpythonmodule

all: $(STATICLIB) $(SHAREDLIB) $(EXECUTABLES)

pythonmodule: $(STATICLIB) libnatpmpmodule.c setup.py
	python setup.py build
	touch $@

installpythonmodule: pythonmodule
	python setup.py install

clean:
	$(RM) $(OBJS) $(EXECUTABLES) $(STATICLIB) $(SHAREDLIB) $(JAVACLASSES) $(JNISHAREDLIB)
	$(RM) pythonmodule
	$(RM) -r build/ dist/ libraries/

depend:
	makedepend -f$(MAKEFILE_LIST) -Y $(OBJS:.o=.c) 2>/dev/null

install:	$(HEADERS) $(STATICLIB) $(SHAREDLIB) natpmpc-shared
	$(INSTALL) -d $(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(INSTALLDIRINC)
	$(INSTALL) -d $(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(STATICLIB) $(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(SHAREDLIB) $(INSTALLDIRLIB)/$(SONAME)
	$(INSTALL) -d $(INSTALLDIRBIN)
	$(INSTALL) -m 755 natpmpc-shared $(INSTALLDIRBIN)/natpmpc
	ln -s -f $(SONAME) $(INSTALLDIRLIB)/$(SHAREDLIB)

$(JNIHEADERS): fr/free/miniupnp/libnatpmp/NatPmp.class
	$(JAVAH) -jni fr.free.miniupnp.libnatpmp.NatPmp

%.class: %.java
	$(JAVAC) -cp . $<

$(JNISHAREDLIB): $(SHAREDLIB) $(JNIHEADERS) $(JAVACLASSES)
ifeq (,$(JAVA_HOME))
	@echo "Check your JAVA_HOME environement variable" && false
endif
ifneq (,$(findstring WIN,$(OS)))
	$(CC) -m32 -D_JNI_Implementation_ -Wl,--kill-at \
	-I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/win32" \
	natpmp-jni.c -shared \
	-o $(JNISHAREDLIB) -L. -lnatpmp -lws2_32 -lIphlpapi
else
	$(CC) $(CFLAGS) -c -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/win32" natpmp-jni.c
	$(CC) $(CFLAGS) -o $(JNISHAREDLIB) -shared $(SONAMEFLAGS) natpmp-jni.o -lc -L. -lnatpmp
endif

jar: $(JNISHAREDLIB)
	find fr -name '*.class' -print > classes.list
	$(eval JNISHAREDLIBPATH := $(shell java fr.free.miniupnp.libnatpmp.LibraryExtractor))
	mkdir -p libraries/$(JNISHAREDLIBPATH)
	mv $(JNISHAREDLIB) libraries/$(JNISHAREDLIBPATH)/$(JNISHAREDLIB)
	jar cf natpmp_$(JARSUFFIX).jar @classes.list libraries/$(JNISHAREDLIBPATH)/$(JNISHAREDLIB)
	$(RM) classes.list

jnitest: $(JNISHAREDLIB) JavaTest.class
	$(RM) libjninatpmp.so
	$(JAVA) -Djna.nosys=true -cp . JavaTest

mvn_install:
	mvn install:install-file -Dfile=java/natpmp_$(JARSUFFIX).jar \
	 -DgroupId=com.github \
	 -DartifactId=natpmp \
	 -Dversion=$(VERSION) \
	 -Dpackaging=jar \
	 -Dclassifier=$(JARSUFFIX) \
	 -DgeneratePom=true \
	 -DcreateChecksum=true

cleaninstall:
	$(RM) $(addprefix $(INSTALLDIRINC), $(HEADERS))
	$(RM) $(INSTALLDIRLIB)/$(SONAME)
	$(RM) $(INSTALLDIRLIB)/$(SHAREDLIB)
	$(RM) $(INSTALLDIRLIB)/$(STATICLIB)

testgetgateway:	testgetgateway.o getgateway.o
	$(CC) $(LDFLAGS) -o $@ $^ $(EXTRA_LD)

natpmpc-static:	natpmpc.o $(STATICLIB)
	$(CC) $(LDFLAGS) -o $@ $^ $(EXTRA_LD)

natpmpc-shared:	natpmpc.o $(SHAREDLIB)
	$(CC) $(LDFLAGS) -o $@ $^ $(EXTRA_LD)

$(STATICLIB):	$(LIBOBJS)
	$(AR) crs $@ $?

$(SHAREDLIB):	$(LIBOBJS)
ifeq ($(OS), Darwin)
	$(CC) -dynamiclib -Wl,-install_name,$(SONAME) -o $@ $^
else
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@ $^ $(EXTRA_LD)
endif


# DO NOT DELETE

natpmp.o: natpmp.h getgateway.h declspec.h
getgateway.o: getgateway.h declspec.h
testgetgateway.o: getgateway.h declspec.h
natpmpc.o: natpmp.h
