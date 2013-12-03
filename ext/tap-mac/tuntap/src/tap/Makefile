#
# ethertap driver for MacOSX
#
# Makefile
#
# (c) 2004, 2005, 2006, 2007, 2008 Mattias Nissler
#

OBJS = ../tuntap.o ../tuntap_mgr.o ../lock.o ../mem.o kmod.o tap.o
KMOD_BIN = tap
BUNDLE_DIR = ../..
BUNDLE_NAME = tap.kext

TAP_KEXT_VERSION = $(TUNTAP_VERSION)

BUNDLE_REGION = English
BUNDLE_IDENTIFIER = com.zerotier.tap
BUNDLE_SIGNATURE = ????
BUNDLE_PACKAGETYPE = KEXT
BUNDLE_VERSION = $(TAP_KEXT_VERSION)

INCLUDE = -I.. -I/System/Library/Frameworks/Kernel.framework/Headers -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk/System/Library/Frameworks/Kernel.framework/Headers
CFLAGS = -Wall -mkernel -force_cpusubtype_ALL \
	-fno-builtin -fno-stack-protector -arch i386 -arch x86_64 \
	-DKERNEL -D__APPLE__ -DKERNEL_PRIVATE -DTUNTAP_VERSION=\"$(TUNTAP_VERSION)\" \
	-DTAP_KEXT_VERSION=\"$(TAP_KEXT_VERSION)\"
CCFLAGS = $(CFLAGS)
LDFLAGS = -Wall -mkernel -nostdlib -r -lcc_kext -arch i386 -arch x86_64 -Xlinker -kext

#CCP = g++
#CC = gcc
CCP = ../../../../llvm-g++-Xcode4.6.2/bin/llvm-g++
CC = ../../../../llvm-g++-Xcode4.6.2/bin/llvm-gcc

all: $(KMOD_BIN) bundle

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@
.cc.o:
	$(CCP) $(CCFLAGS) $(INCLUDE) -c $< -o $@

$(KMOD_BIN):	$(OBJS)
	$(CCP) $(LDFLAGS) -o $(KMOD_BIN) $(OBJS)

bundle:	$(KMOD_BIN)
	rm -rf $(BUNDLE_DIR)/$(BUNDLE_NAME)
	mkdir -p $(BUNDLE_DIR)/$(BUNDLE_NAME)/Contents/MacOS
	cp $(KMOD_BIN) $(BUNDLE_DIR)/$(BUNDLE_NAME)/Contents/MacOS
	sed -e "s/@@CFBUNDLEEXECUTABLE@@/$(KMOD_BIN)/" \
	 -e "s/@@CFBUNDLEDEVELOPMENTREGION@@/$(BUNDLE_REGION)/" \
	 -e "s/@@CFBUNDLEIDENTIFIER@@/$(BUNDLE_IDENTIFIER)/" \
	 -e "s/@@CFBUNDLESIGNATURE@@/$(BUNDLE_SIGNATURE)/" \
	 -e "s/@@CFBUNDLEPACKAGETYPE@@/$(BUNDLE_PACKAGETYPE)/" \
	 -e "s/@@CFBUNDLEVERSION@@/$(BUNDLE_VERSION)/" \
	Info.plist > $(BUNDLE_DIR)/$(BUNDLE_NAME)/Contents/Info.plist

clean:
	-rm -f $(OBJS) $(KMOD_BIN)
	-rm -rf $(BUNDLE_DIR)/$(BUNDLE_NAME)

