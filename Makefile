all:

clean:	FORCE
	for i in */Cargo.toml; do cd $$(dirname $$i); cargo clean || exit 1; cd ..; done

test:
	for i in */Cargo.toml; do cd $$(dirname $$i); cargo test || exit 1; cd ..; done

fmt:
	set extglob; for i in */Cargo.toml; do cd $$(dirname $$i); rustfmt **/*.rs || exit 1; cd ..; done


FORCE:

.PHONY: test
