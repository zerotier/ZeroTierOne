all:
	gcc -O6 -c ../ext/lz4/lz4hc.c ../ext/lz4/lz4.c
	g++ -DZT_OSNAME="linux" -DZT_ARCH="x86_64" -I/usr/include/mysql -I../ext/bin/libcrypto/include -O -o netconf.service netconf.cpp ../node/Utils.cpp ../node/Identity.cpp ../node/EllipticCurveKeyPair.cpp ../node/Salsa20.cpp ../node/HMAC.cpp lz4.o lz4hc.o ../ext/bin/libcrypto/linux-x86_64/libcrypto.a -lmysqlpp

clean:
	rm -f *.o netconf.service
