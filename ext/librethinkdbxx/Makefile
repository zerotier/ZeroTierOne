# Customisable build settings

CXX ?= clang++
CXXFLAGS ?=
INCLUDE_PYTHON_DOCS ?= no
DEBUG ?= no
PYTHON ?= python3

# Required build settings

ifneq (no,$(DEBUG))
  CXXFLAGS += -ggdb
else
  CXXFLAGS += -O3 # -flto
endif

CXXFLAGS += -std=c++11 -I'build/gen' -Wall -pthread -fPIC

prefix ?= /usr
DESTDIR ?=

.DELETE_ON_ERROR:
SHELL := /bin/bash

modules := connection datum json term cursor types utils
headers := utils error exceptions types datum connection cursor term

o_files := $(patsubst %, build/obj/%.o, $(modules))
d_files := $(patsubst %, build/dep/%.d, $(modules))

skip_tests := regression/1133 regression/767 regression/1005 # python-only
skip_tests += arity # arity errors are compile-time
skip_tests += geo # geo types not implemented yet
skip_tests += limits # possibly broken tests: https://github.com/rethinkdb/rethinkdb/issues/5940

upstream_tests := \
  $(filter-out %.rb.%, \
    $(filter-out $(patsubst %,test/upstream/%%, $(skip_tests)), \
      $(filter test/upstream/$(test_filter)%, \
        $(shell find test/upstream -name '*.yaml' | egrep -v '.(rb|js).yaml$$'))))
upstream_tests_cc := $(patsubst %.yaml, build/tests/%.cc, $(upstream_tests))
upstream_tests_o := $(patsubst %.cc, %.o, $(upstream_tests_cc))

.PRECIOUS: $(upstream_tests_cc) $(upstream_tests_o)

default: build/librethinkdb++.a build/include/rethinkdb.h build/librethinkdb++.so

all: default build/test

build/librethinkdb++.a: $(o_files)
	ar rcs $@ $^

build/librethinkdb++.so: $(o_files)
	$(CXX) -o $@ $(CXXFLAGS) -shared $^

build/obj/%.o: src/%.cc build/gen/protocol_defs.h
	@mkdir -p $(dir $@)
	@mkdir -p $(dir build/dep/$*.d)
	$(CXX) -o $@ $(CXXFLAGS) -c $< -MP -MQ $@ -MD -MF build/dep/$*.d

build/gen/protocol_defs.h: reql/ql2.proto reql/gen.py | build/gen/.
	$(PYTHON) reql/gen.py $< > $@

clean:
	rm -rf build

ifneq (no,$(INCLUDE_PYTHON_DOCS))
build/include/rethinkdb.h: build/rethinkdb.nodocs.h reql/add_docs.py reql/python_docs.txt | build/include/.
	$(PYTHON) reql/add_docs.py reql/python_docs.txt < $< > $@
else
build/include/rethinkdb.h: build/rethinkdb.nodocs.h | build/include/.
	cp $< $@
endif

build/rethinkdb.nodocs.h: build/gen/protocol_defs.h $(patsubst %, src/%.h, $(headers))
	( echo "// Auto-generated file, built from $^"; \
	  echo '#pragma once'; \
	  cat $^ | \
	    grep -v '^#pragma once' | \
	    grep -v '^#include "'; \
	) > $@

build/tests/%.cc: %.yaml test/yaml_to_cxx.py
	@mkdir -p $(dir $@)
	$(PYTHON) test/yaml_to_cxx.py $< > $@

build/tests/upstream_tests.cc: $(upstream_tests) test/gen_index_cxx.py FORCE | build/tests/.
	@echo '$(PYTHON) test/gen_index_cxx.py $(wordlist 1,5,$(upstream_tests)) ... > $@'
	@$(PYTHON) test/gen_index_cxx.py $(upstream_tests) > $@

build/tests/%.o: build/tests/%.cc build/include/rethinkdb.h test/testlib.h | build/tests/.
	$(CXX) -o $@ $(CXXFLAGS) -isystem build/include -I test -c $< -Wno-unused-variable

build/tests/%.o: test/%.cc test/testlib.h build/include/rethinkdb.h | build/tests/.
	$(CXX) -o $@ $(CXXFLAGS) -isystem build/include -I test -c $<

build/test: build/tests/testlib.o build/tests/test.o build/tests/upstream_tests.o $(upstream_tests_o) build/librethinkdb++.a
	@echo $(CXX) -o $@ $(CXXFLAGS) $(wordlist 1,5,$^) ...
	@$(CXX) -o $@ $(CXXFLAGS) build/librethinkdb++.a $^

.PHONY: test
test: build/test
	build/test

build/bench: build/tests/bench.o build/librethinkdb++.a
	@$(CXX) -o $@ $(CXXFLAGS) -isystem build/include build/librethinkdb++.a $^

.PHONY: bench
bench: build/bench
	build/bench

.PHONY: install
install: build/librethinkdb++.a build/include/rethinkdb.h build/librethinkdb++.so
	install -m755 -d $(DESTDIR)$(prefix)/lib
	install -m755 -d $(DESTDIR)$(prefix)/include
	install -m644 build/librethinkdb++.a $(DESTDIR)$(prefix)/lib/librethinkdb++.a
	install -m644 build/librethinkdb++.so $(DESTDIR)$(prefix)/lib/librethinkdb++.so
	install -m644 build/include/rethinkdb.h $(DESTDIR)$(prefix)/include/rethinkdb.h

%/.:
	mkdir -p $*

.PHONY: FORCE
FORCE:

-include $(d_files)
