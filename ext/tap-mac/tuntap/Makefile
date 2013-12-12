TUNTAP_VERSION = 20131028
BASE=

all: tap.kext

clean:
	cd src/tap && make -f Makefile clean

tap.kext:
	cd src/tap && make TUNTAP_VERSION=$(TUNTAP_VERSION) -f Makefile all

.PHONY: test
