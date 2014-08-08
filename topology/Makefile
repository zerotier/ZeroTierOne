all:	FORCE
	g++ -o mktopology mktopology.cpp ../node/Utils.cpp ../node/Identity.cpp ../node/C25519.cpp ../node/Salsa20.cpp ../node/Dictionary.cpp ../node/SHA512.cpp
	gcc -o bin2c bin2c.c

official: all
	rm -f root_topology.out
	./mktopology >root_topology.out
	./bin2c ZT_DEFAULT_ROOT_TOPOLOGY < root_topology.out > ZT_DEFAULT_ROOT_TOPOLOGY.c
	ls -l ZT_DEFAULT_ROOT_TOPOLOGY.c

clean:
	rm -f *.o mktopology bin2c root_topology.out

FORCE:
