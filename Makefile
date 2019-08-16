# Common makefile -- loads make rules for each platform

BUILDDIR := build

.PHONY: all

all:
	mkdir -p ${BUILDDIR} && cd ${BUILDDIR} && cmake .. && $(MAKE)

clean:
	rm -rf ${BUILDDIR}
