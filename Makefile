all:

clean:	FORCE
	rm -rf zerotier-core-crypto/target zerotier-network-hypervisor/target zerotier-system-service/target syncwhole/target aes-gmac-siv/target iblt/target

test:
	for i in */Cargo.toml; do cd $$(dirname $$i); cargo test || exit 1; cd ..; done

FORCE:

.PHONY: test
