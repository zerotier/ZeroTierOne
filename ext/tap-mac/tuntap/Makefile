# Lets have a version, at last!
TUNTAP_VERSION = 20150118

# BASE install directory
BASE=

all: tap.kext

keysetup:
	-security delete-keychain net.sf.tuntaposx.tmp
	security create-keychain -p $$(head -c 32 /dev/urandom | hexdump -e '"%02x"') \
		net.sf.tuntaposx.tmp
	security set-keychain-settings -lut 60 net.sf.tuntaposx.tmp
	security import identity.p12 -k net.sf.tuntaposx.tmp -f pkcs12 \
		-P $$(read -sp 'identity passphrase: ' pw && echo "$$pw") -A
	security find-identity -v net.sf.tuntaposx.tmp | \
		awk -F \" '$$2 ~ /^Developer ID Application:/ { print $$2 }' > .signing_identity
	security find-identity -v net.sf.tuntaposx.tmp | \
		awk -F \" '$$2 ~ /^Developer ID Installer:/ { print $$2 }' > .installer_identity

pkgbuild/%.pkg: %.kext
	mkdir -p pkgbuild/$*_root/Library/Extensions
	cp -pR $*.kext pkgbuild/$*_root/Library/Extensions
	mkdir -p pkgbuild/$*_root/Library/LaunchDaemons
	cp pkg/launchd/net.sf.tuntaposx.$*.plist pkgbuild/$*_root/Library/LaunchDaemons
	pkgbuild --root pkgbuild/$*_root \
		--component-plist pkg/components/$*.plist \
		--scripts pkg/scripts/$* pkgbuild/$*.pkg

tuntap_$(TUNTAP_VERSION).pkg: pkgbuild/tap.pkg pkgbuild/tun.pkg
	productbuild --distribution pkg/distribution.xml --package-path pkgbuild \
		--resources pkg/res.dummy \
		tuntap_$(TUNTAP_VERSION).pkg ; \
	pkgutil --expand tuntap_$(TUNTAP_VERSION).pkg pkgbuild/tuntap_pkg.d
	cp -pR pkg/res/ pkgbuild/tuntap_pkg.d/Resources
	pkgutil --flatten pkgbuild/tuntap_pkg.d tuntap_$(TUNTAP_VERSION).pkg
	if test -s ".installer_identity"; then \
		productsign --sign "$$(cat .installer_identity)" --keychain net.sf.tuntaposx.tmp \
			tuntap_$(TUNTAP_VERSION).pkg tuntap_$(TUNTAP_VERSION).pkg.signed ; \
		mv tuntap_$(TUNTAP_VERSION).pkg.signed tuntap_$(TUNTAP_VERSION).pkg ; \
	fi

pkg: tuntap_$(TUNTAP_VERSION).pkg
	tar czf tuntap_$(TUNTAP_VERSION).tar.gz \
		README.installer README tuntap_$(TUNTAP_VERSION).pkg 

# Install targets
# They are provided for the gentoo ebuild, but should work just fine for other people as well.
install_%_kext: %.kext
	mkdir -p $(BASE)/Library/Extensions
	cp -pR $*.kext $(BASE)/Library/Extensions/
	chown -R root:wheel $(BASE)/Library/Extensions/$*.kext
	mkdir -p $(BASE)/Library/LaunchDaemons
	cp pkg/launchd/net.sf.tuntaposx.$*.plist $(BASE)/Library/LaunchDaemons
	chown -R root:wheel $(BASE)/Library/LaunchDaemons/net.sf.tuntaposx.$*.plist

install: install_tap_kext install_tun_kext

tarball: clean
	touch tuntap_$(TUNTAP_VERSION)_src.tar.gz
	tar czf tuntap_$(TUNTAP_VERSION)_src.tar.gz \
		-C .. \
		--exclude "tuntap/identity.p12" \
		--exclude "tuntap/tuntap_$(TUNTAP_VERSION)_src.tar.gz" \
		--exclude "tuntap/tuntap_$(TUNTAP_VERSION).tar.gz" \
		--exclude "tuntap/tuntap_$(TUNTAP_VERSION).pkg" \
		--exclude "*/.*" \
		tuntap

clean:
	cd src/tap && make -f Makefile clean
	cd src/tun && make -f Makefile clean
	-rm -rf pkgbuild
	-rm -rf tuntap_$(TUNTAP_VERSION).pkg
	-rm -f tuntap_$(TUNTAP_VERSION).tar.gz
	-rm -f tuntap_$(TUNTAP_VERSION)_src.tar.gz

%.kext:
	cd src/$* && make TUNTAP_VERSION=$(TUNTAP_VERSION) -f Makefile all
	if test -s ".signing_identity"; then \
		codesign -fv --keychain net.sf.tuntaposx.tmp -s "$$(cat .signing_identity)" \
			$*.kext ; \
	fi

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
