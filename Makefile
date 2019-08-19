# Common makefile -- loads make rules for each platform

BUILDDIR := build
CMAKE_OPTS := -DCMAKE_BUILD_TYPE=Release

.PHONY: all

all:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. ${CMAKE_OPTS} && $(MAKE) -j$(shell getconf _NPROCESSORS_ONLN)

clean:
	rm -rf ${BUILDDIR}

distclean:
	rm -rf ${BUILDDIR}
