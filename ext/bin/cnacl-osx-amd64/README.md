# cNaCl

    If you would like to be confusing, you could pronounce it sea-salt

This is a fork NaCl by Daniel J. Bernstein and Tanja Lange.
The build has been ported to cmake so it can be cross compiled and build output is reliable.
Since it uses cmake, it could theoretically be built on windows but this has not been tested.
It does compile using mingw32.

## How do I make this thing work?

    mkdir cbuild
    cd cbuild
    cmake ..
    make

## Ok now how about cross compiling?

    mkdir cbuildw32
    cd cbuildw32
    cmake -DCMAKE_TOOLCHAIN_FILE=../CMakeWindows.txt ..
    make

## Why fork?

NaCl builds using a shell script called `./do`. This script does compiling, testing, measuring
and selection of the best implementation of each algorithm for the given machine. It also generates
the header files which will be used.

The problems with `./do` are it's slow, it tries compiling with multiple different compiler
profiles, it's very platform independent but it doesn't run on Windows and most importantly, with
compiling, testing and measuring so tightly bound, it is impossible to cross compile for a
different operating system.


## How it works

The first time you build for a new ABI, it will trigger the traditional nacl `./do` script.
What cNaCl does is parse the resulting headers from the `./do` build and create a plan so that it
can repeat roughly the same build.

If there is already a plan for the given ABI, the build uses this plan and the build is very fast.

Plans are stored in `./cmake/plans/` and I will be adding plans as I find new ones.


## What else is new?

There is a problem with the `./do` build which prevents it from running on some ARM based machines,
this was fixed by adding a more lax method for measuring CPU speed as a fall back.


`#EOF#`
