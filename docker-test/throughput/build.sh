cd ../../
make clean
make
cd netcon
make -f make-intercept.mk lib
rm *.o
rm liblwip.so
make -f make-liblwip.mk
cd ../docker-test/throughput-test

cp ../../zerotier-one zerotier-one
cp ../../zerotier-cli zerotier-cli

cp ../../netcon/liblwip.so liblwip.so
cp ../../netcon/libintercept.so.1.0 libintercept.so.1.0

