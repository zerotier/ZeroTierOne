BUILDDIR := build
TIMESTAMP=$(shell date +"%Y%m%d%H%M")

.PHONY: all

all:	setup
	cd ${BUILDDIR} && $(MAKE) -j$(shell getconf _NPROCESSORS_ONLN)

setup:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Release

setup-debug:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug

debug:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug  && $(MAKE)

central-controller:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_CENTRAL_CONTROLLER=1 && $(MAKE) -j4

central-controller-debug:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_CENTRAL_CONTROLLER=1 && $(MAKE) -j4

central-controller-docker:
	docker build -t registry.zerotier.com/zerotier-central/ztcentral-controller:${TIMESTAMP} -f controller/central-docker/Dockerfile .

clean:
	rm -rf ${BUILDDIR} cmake-build-*

distclean:
	rm -rf ${BUILDDIR} cmake-build-*
