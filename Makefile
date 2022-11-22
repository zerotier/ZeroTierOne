export CARGO_NET_GIT_FETCH_WITH_CLI := true
export RUST_BACKTRACE := full

all: test build

test:
	@echo "running cargo test"
	${HOME}/.cargo/bin/cargo test -v

build:
	@echo "running cargo build"
	${HOME}/.cargo/bin/cargo build -v

drone:
	@echo "rendering .drone.yaml from .drone.jsonnet"
	drone jsonnet --format --stream

munge_rpm:
	@:$(call check_defined, VERSION)
	@echo "Updating rpm spec to $(VERSION)"
	ci/scripts/munge_rpm_spec.sh zerotier-one.spec $(VERSION) "Adam Ierymenko <adam.ierymenko@zerotier.com>" "see https://github.com/zerotier/ZeroTierOne for release notes"

munge_deb:
	@:$(call check_defined, VERSION)
	@echo "Updating debian/changelog to $(VERSION)"
	ci/scripts/munge_debian_changelog.sh debian/changelog $(VERSION) "Adam Ierymenko <adam.ierymenko@zerotier.com>" "see https://github.com/zerotier/ZeroTierOne for release notes"

debian:	FORCE
	debuild --no-lintian -I -i -us -uc -nc -b

debian-clean: FORCE
	rm -rf debian/files debian/zerotier-one*.debhelper debian/zerotier-one.substvars debian/*.log debian/zerotier-one debian/.debhelper debian/debhelper-build-stamp

redhat:	FORCE
	rpmbuild --target `rpm -q bash --qf "%{arch}"` -ba zerotier-one.spec

FORCE:
