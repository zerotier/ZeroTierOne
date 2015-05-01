all:	FORCE
	g++ -o mktopology mktopology.cpp ../osdep/OSUtils.cpp ../node/Utils.cpp ../node/InetAddress.cpp ../node/Identity.cpp ../node/C25519.cpp ../node/Salsa20.cpp ../node/Dictionary.cpp ../node/SHA512.cpp
	gcc -o bin2c bin2c.c

official: FORCE
	rm -f ZT_DEFAULT_ROOT_TOPOLOGY.dict
	./mktopology >ZT_DEFAULT_ROOT_TOPOLOGY.dict
	./bin2c ZT_DEFAULT_ROOT_TOPOLOGY < ZT_DEFAULT_ROOT_TOPOLOGY.dict > ZT_DEFAULT_ROOT_TOPOLOGY.c
	ls -l ZT_DEFAULT_ROOT_TOPOLOGY.c

clean:
	rm -f *.o mktopology bin2c

realclean: clean
	rm -f ZT_DEFAULT_ROOT_TOPOLOGY.c ZT_DEFAULT_ROOT_TOPOLOGY.dict

FORCE:
