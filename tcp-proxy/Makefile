CXX=$(shell which clang++ g++ c++ 2>/dev/null | head -n 1)

INCLUDES?=-isystem ../ext/prometheus-cpp-lite-1.0/core/include  -isystem ../ext/prometheus-cpp-lite-1.0/simpleapi/include

OBJS=Metrics.o	\
	../node/Metrics.o 

CXXFLAGS=-O3 -fno-rtti $(INCLUDES) -std=c++17 -pthread -frtti

all: $(OBJS) tcp-proxy.o
	$(CXX) -O3 -fno-rtti $(INCLUDES) -std=c++17 -pthread -frtti  -o tcp-proxy tcp-proxy.o $(OBJS) 

clean:
	rm -f $(OBJS) tcp-proxy.o tcp-proxy *.dSYM
