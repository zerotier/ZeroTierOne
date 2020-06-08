BUILDDIR := build

.PHONY: all

all:	setup
	cd ${BUILDDIR} && $(MAKE) -j$(shell getconf _NPROCESSORS_ONLN)

setup:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Release ${CMAKE_ARGS}

setup-debug:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug ${CMAKE_ARGS}

debug:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. -DCMAKE_BUILD_TYPE=Debug ${CMAKE_ARGS} && $(MAKE)

clean:
	rm -rf ${BUILDDIR} cmake-build-*

distclean:
	rm -rf ${BUILDDIR} cmake-build-*
