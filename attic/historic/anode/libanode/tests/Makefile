all: force clean anode-utils-test anode-zone-test aes-test ec-test

aes-test:
	gcc -Wall -O6 -ftree-vectorize -std=c99 -o aes-test aes-test.c ../aes_digest.c -lcrypto

http_client-test:
	gcc -O0 -g -std=c99 -o http_client-test http_client-test.c ../anode-utils.c ../misc.c ../http_client.c ../dictionary.c ../iptransport.c ../anode-transport.c -lcrypto

anode-utils-test:
	gcc -O0 -g -std=c99 -o anode-utils-test anode-utils-test.c ../anode-utils.c ../misc.c

ec-test:
	gcc -O0 -g -std=c99 -o ec-test ec-test.c ../impl/ec.c ../impl/misc.c -lcrypto

anode-zone-test:
	gcc -O0 -g -std=c99 -o anode-zone-test anode-zone-test.c ../anode-zone.c ../http_client.c ../dictionary.c ../misc.c ../anode-transport.c ../iptransport.c ../environment.c

system_transport-test:
	gcc -O0 -g -std=c99 -o system_transport-test system_transport-test.c ../system_transport.c ../network_address.c ../address.c ../aes_digest.c ../impl/misc.c ../impl/thread.c ../impl/dns_txt.c ../impl/aes.c -lresolv -lcrypto

clean:	force
	rm -rf *.dSYM
	rm -f http_client-test anode-utils-test anode-zone-test ec-test aes-test system_transport-test

force: ;
