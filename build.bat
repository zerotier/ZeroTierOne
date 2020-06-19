echo off
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
make -j4
cd ..
