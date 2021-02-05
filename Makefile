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
	rm -rf ${BUILDDIR} rust-zerotier-core/target rust-zerotier-service/target

distclean: FORCE
	rm -rf ${BUILDDIR}

rust-bindgen: FORCE
	cargo install bindgen
	rm -f rust-zerotier-core/src/capi.rs rust-zerotier-service/src/osdep.rs
	bindgen --no-doc-comments --no-layout-tests --no-derive-debug core/zerotier.h >rust-zerotier-core/src/capi.rs
	bindgen --no-doc-comments --no-layout-tests --no-derive-debug osdep/rust-osdep.h >rust-zerotier-service/src/osdep.rs

FORCE:
