# Lets have a version, at last!
TUNTAP_VERSION = 20111101

# BASE install directory
BASE=

all: tap.kext tun.kext

pkg: all
	/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker \
		-d pkg/tuntap.pmdoc -o tuntap_$(TUNTAP_VERSION).pkg -v
	tar czf tuntap_$(TUNTAP_VERSION).tar.gz \
		README.installer README tuntap_$(TUNTAP_VERSION).pkg 

# Install targets
# They are provided for the gentoo ebuild, but should work just fine for other people as well.
install_kext: tap.kext tun.kext
	mkdir -p ${BASE}/Library/Extensions
	cp -pR tap.kext ${BASE}/Library/Extensions/
	chown -R root:wheel ${BASE}/Library/Extensions/tap.kext
	cp -pR tun.kext ${BASE}/Library/Extensions/ 
	chown -R root:wheel ${BASE}/Library/Extensions/tun.kext

install_scripts:
	mkdir -p ${BASE}/Library/StartupItems
	cp -pR startup_item/tap ${BASE}/Library/StartupItems/
	chown -R root:wheel ${BASE}/Library/StartupItems/tap
	cp -pR startup_item/tun ${BASE}/Library/StartupItems/ 
	chown -R root:wheel ${BASE}/Library/StartupItems/tun

install: install_kext install_scripts

tarball: clean
	touch tuntap_$(TUNTAP_VERSION)_src.tar.gz
	tar czf tuntap_$(TUNTAP_VERSION)_src.tar.gz \
		-C .. \
		--exclude "tuntap/tuntap_$(TUNTAP_VERSION)_src.tar.gz" \
		--exclude "tuntap/tuntap_$(TUNTAP_VERSION).tar.gz" \
		--exclude "tuntap/tuntap_$(TUNTAP_VERSION).pkg" \
		--exclude "*/.*" \
		tuntap

clean:
	cd src/tap && make -f Makefile clean
	cd src/tun && make -f Makefile clean
	-rm -rf tuntap_$(TUNTAP_VERSION).pkg
	-rm -f tuntap_$(TUNTAP_VERSION).tar.gz
	-rm -f tuntap_$(TUNTAP_VERSION)_src.tar.gz

tap.kext:
	cd src/tap && make TUNTAP_VERSION=$(TUNTAP_VERSION) -f Makefile all

tun.kext:
	cd src/tun && make TUNTAP_VERSION=$(TUNTAP_VERSION) -f Makefile all

test:
	# configd messes with interface flags, issuing SIOCSIFFLAGS ioctls upon receiving kernel
	# events indicating protocols have been attached and detached. Unfortunately, configd does
	# this asynchronously, making the SIOCSIFFLAGS changes totally unpredictable when we bring
	# our interfaces up and down in rapid succession during our tests. I haven't found a good
	# way to suppress or handle this mess other than disabling configd temporarily.
	killall -STOP configd
	-PYTHONPATH=test python test/tuntap/tuntap_tests.py --tests='$(TESTS)'
	killall -CONT configd

.PHONY: test
