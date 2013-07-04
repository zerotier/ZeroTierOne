make clean
./Configure no-sock no-ssl2 no-ssl3 no-err no-krb5 no-engine no-hw no-tlsext no-jpake no-capieng no-idea no-camellia no-seed no-bf no-cast no-des no-rc2 no-rc4 no-rc5 no-md2 no-md4 no-ripemd no-mdc2 no-rsa no-dsa no-dh no-shared no-zlib no-dso no-cms no-ocsp no-txt_db no-pem no-mdc2 no-ui no-ts no-aes no-whrlpool no-srp darwin64-x86_64-cc
make depend
make build_crypto
mv libcrypto.a /tmp/libcrypto-x86_64.a
make clean
./Configure no-sock no-ssl2 no-ssl3 no-err no-krb5 no-engine no-hw no-tlsext no-jpake no-capieng no-idea no-camellia no-seed no-bf no-cast no-des no-rc2 no-rc4 no-rc5 no-md2 no-md4 no-ripemd no-mdc2 no-rsa no-dsa no-dh no-shared no-zlib no-dso no-cms no-ocsp no-txt_db no-pem no-mdc2 no-ui no-ts no-aes no-whrlpool no-srp darwin-i386-cc
make depend
make build_crypto
mv libcrypto.a /tmp/libcrypto-i386.a
make clean

echo use lipo to combine, libs are in /tmp
