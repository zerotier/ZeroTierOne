# Common makefile -- loads make rules for each platform

BUILDDIR := build

.PHONY: all

all:	setup
	cd ${BUILDDIR} && $(MAKE) -j$(shell getconf _NPROCESSORS_ONLN)

setup:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Release

setup-debug:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug

debug:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug && $(MAKE)

clean:
	rm -rf ${BUILDDIR}

distclean:
	rm -rf ${BUILDDIR}
