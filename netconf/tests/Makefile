# Builds tests.cpp for Redis-based netconf master tests on Linux or OSX

all:	FORCE
	$(CXX) -g -o tests tests.cpp ../../node/Utils.o ../../node/Identity.o ../../node/C25519.o ../../node/SHA512.o ../../node/Salsa20.o ../../node/CertificateOfMembership.o ../../node/Dictionary.o

clean:
	rm -rf tests *.o *.dSYM

FORCE:
