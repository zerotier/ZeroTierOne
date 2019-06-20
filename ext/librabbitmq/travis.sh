#!/usr/bin/env bash

build_cmake() {
  mkdir $PWD/_build && cd $PWD/_build
  cmake .. -DCMAKE_INSTALL_PREFIX=$PWD/../_install -DCMAKE_C_FLAGS="-Werror" \
    ${_CMAKE_OPENSSL_FLAG}
  cmake --build . --target install
  ctest -V .
}

build_format() {
  ./travis/run-clang-format/run-clang-format.py \
    --clang-format-executable="${PWD}/travis/clang-format.sh" \
    --recursive examples librabbitmq tests tools
}

build_coverage() {
  mkdir $PWD/_build && cd $PWD/_build
  cmake .. -DCMAKE_BUILD_TYPE=Coverage -DCMAKE_INSTALL_PREFIX=$PWD/../_install \
    -DCMAKE_C_FLAGS="-Werror -fprofile-arcs -ftest-coverage" \
    ${_CMAKE_OPENSSL_FLAG}
  cmake --build . --target install
  ctest -V .
  
  pip install --user cpp-coveralls
  coveralls --exclude tests --build-root . --root .. --gcov-options '\-lp'
}

build_asan() {
  mkdir $PWD/_build && cd $PWD/_build
  cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$PWD/../_install \
    -DCMAKE_C_FLAGS="-Werror -fsanitize=address,undefined -O1"
  cmake --build . --target install
  ctest -V .
}

build_tsan() {
  mkdir $PWD/_build && cd $PWD/_build
  cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$PWD/../_install \
    -DCMAKE_C_FLAGS="-Werror -fsanitize=thread,undefined -O1"
  cmake --build . --target install
  ctest -V .
}

build_scan-build() {
  mkdir $PWD/_build && cd $PWD/_build
  scan-build-3.9 cmake .. -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$PWD/../_install \
    -DCMAKE_C_FLAGS="-Werror"
  scan-build-3.9 make install
}

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 {cmake|asan|tsan|scan-build}"
  exit 1
fi

set -e  # exit on error.
set -x  # echo commands.

case $TRAVIS_OS_NAME in
osx)
  # This prints out a long list of updated packages, which isn't useful.
  brew update > /dev/null
  brew install popt
  brew outdated openssl || brew install openssl
  export _CMAKE_OPENSSL_FLAG="-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl"
  ;;
esac

eval "build_$1"
