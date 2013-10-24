all:
	gcc -O6 -c ../ext/lz4/lz4hc.c ../ext/lz4/lz4.c
	g++ -DZT_OSNAME="linux" -DZT_ARCH="x86_64" -I/usr/include/mysql -I../ext/bin/libcrypto/include -O -pthread -o netconf.service netconf.cpp ../node/Utils.cpp ../node/Identity.cpp ../node/Salsa20.cpp ../node/C25519.cpp ../node/SHA512.cpp ../node/CertificateOfMembership.cpp lz4.o lz4hc.o -lmysqlpp
#	g++ -DZT_OSNAME="linux" -DZT_ARCH="x86_64" -I/usr/include/mysql -I../ext/bin/libcrypto/include -O -pthread -o netconf-test netconf-test.cpp ../node/Utils.cpp ../node/Identity.cpp ../node/Salsa20.cpp ../node/Logger.cpp ../node/Service.cpp ../node/C25519.cpp ../node/SHA512.cpp lz4.o lz4hc.o

clean:
	rm -f *.o netconf.service netconf-test
