CXX=$(shell which clang++ g++ c++ 2>/dev/null | head -n 1)

all:
	$(CXX) -O3 -fno-rtti -o tcp-proxy tcp-proxy.cpp

clean:
	rm -f *.o tcp-proxy *.dSYM
