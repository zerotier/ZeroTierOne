all:
	rm -f zerotier-root
	c++ -std=c++11 -O3 -maes -mpclmul -msse -msse2 -msse3 -msse4.1 -pthread -o zerotier-root root.cpp ../node/*.cpp ../osdep/OSUtils.cpp -lresolv
