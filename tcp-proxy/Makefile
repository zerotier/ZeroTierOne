CXX=$(shell which clang++ g++ c++ 2>/dev/null | head -n 1)

INCLUDES?=-I../ext/prometheus-cpp-lite-1.0/core/include  -I../ext/prometheus-cpp-lite-1.0/simpleapi/include

all:
	$(CXX) -O3 -fno-rtti $(INCLUDES) -std=c++11 -pthread -frtti  -o tcp-proxy tcp-proxy.cpp ../node/Metrics.cpp

clean:
	rm -f *.o tcp-proxy *.dSYM
