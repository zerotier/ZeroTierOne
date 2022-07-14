CC=/usr/bin/gcc
LDFLAGS=-lcrypto

SOURCES= rng.c generate_bufs.c
HEADERS= rng.h

generate: $(HEADERS) $(SOURCES)
	$(CC) -g -o $@ $(SOURCES) $(LDFLAGS)

.PHONY: clean

clean:
	-rm generate

