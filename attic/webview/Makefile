WEBVIEW_gtk_FLAGS = -DWEBVIEW_GTK -std=c++14 -Wall -Wextra -pedantic $(shell pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0)
WEBVIEW_cocoa_FLAGS = -DWEBVIEW_COCOA -std=c++14 -Wall -Wextra -pedantic -framework WebKit -mmacosx-version-min=10.11 -DOBJC_OLD_DISPATCH_PROTOTYPES
WEBVIEW_mshtml_FLAGS := -DWEBVIEW_MSHTML -std=c++14 -luser32 -lole32 -loleaut32 -lcomctl32 -luuid -static
WEBVIEW_edge_FLAGS := -DWEBVIEW_EDGE

all:
	@echo "make WEBVIEW=... test - build and run tests"
	@echo "make WEBVIEW=... lint - run clang-tidy checkers"
	@echo "make WEBVIEW=... fmt	- run clang-format for all sources"

fmt: webview.h
	clang-format -i $^

check-env:
ifndef WEBVIEW_$(WEBVIEW)_FLAGS
	$(error "Unknown WEBVIEW value, use WEBVIEW=gtk|cocoa|mshtml|edge")
endif

lint: check-env
	clang-tidy example.cc -- $(WEBVIEW_$(WEBVIEW)_FLAGS)

example: check-env example.cc webview.h
	$(CXX) example.cc $(WEBVIEW_$(WEBVIEW)_FLAGS) -o example

test: check-env
	$(CXX) webview_test.cc $(WEBVIEW_$(WEBVIEW)_FLAGS) -o webview_test
	./webview_test
	rm webview_test
