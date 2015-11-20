#./build.sh

# httpd
cd httpd
cd httpd-2.4.16-1.fc23.x86_64
pwd
./test.sh
cd ..

cd httpd-2.4.17-3.fc23.x86_64
pwd
./test.sh
cd ..

cd ..

# nginx
cd nginx
pwd
ls -l -G
cd nginx-1.8.0-13.fc23.x86_64
./test.sh
cd ..

cd nginx-1.8.0-14.fc23.x86_64
pwd
./test.sh
cd ..

cd ..