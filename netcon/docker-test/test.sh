#./build.sh

# Relative paths are used in each test script, hence the need to change directories

# httpd
cd httpd
cd httpd-2.4.16-1.fc23.x86_64
./test.sh
cd ..

cd httpd-2.4.17-3.fc23.x86_64
./test.sh
cd ..

cd ..

# nginx
cd nginx
ls -l -G
cd nginx-1.8.0-13.fc23.x86_64
./test.sh
cd ..

cd nginx-1.8.0-14.fc23.x86_64
./test.sh
cd ..

#redis
cd redis-3.0.4-1.fc23.x86_64
./test.sh
cd ..

#node.js
cd nodejs-0.10.36-4.fc23
./test.sh
cd ..

cd ..