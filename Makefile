BUILDDIR := build
TIMESTAMP=$(shell date +"%Y%m%d%H%M")

.PHONY: all

all:	setup
	cd ${BUILDDIR} && $(MAKE) -j4 VERBOSE=1

setup: FORCE
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Release ${CMAKE_ARGS}

setup-debug: FORCE
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug ${CMAKE_ARGS}

debug: FORCE
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug ${CMAKE_ARGS} && $(MAKE)

central-controller: FORCE
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_CENTRAL_CONTROLLER=1 ${CMAKE_ARGS} && $(MAKE) -j4

central-controller-debug: FORCE
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_CENTRAL_CONTROLLER=1 ${CMAKE_ARGS}  && $(MAKE) -j4

central-controller-docker: FORCE
	docker build -t registry.zerotier.com/zerotier-central/ztcentral-controller:${TIMESTAMP} -f controller/central-docker/Dockerfile .

clean: FORCE
	rm -rf ${BUILDDIR}

distclean: FORCE
	rm -rf ${BUILDDIR}

rust-zerotier-core-bindgen: FORCE
	cargo install bindgen
	rm -f rust-zerotier-core/src/bindings/capi.rs
	bindgen --no-doc-comments core/zerotier.h >rust-zerotier-core/src/bindings/capi.rs

FORCE:
