
case $ZT_NAME in
    el*|fc*|amzn*)
        export PKGFMT=rpm
        ;;
    *)
        export PKGFMT=deb
esac

case $ZT_ISA in
    386)
        export DOCKER_ARCH=386
        export DEB_ARCH=i386
        export DNF_ARCH=i686
        export RUST_TRIPLET=i686-unknown-linux-gnu
        ;;
    amd64)
        export DOCKER_ARCH=amd64
        export DEB_ARCH=amd64
        export DNF_ARCH=x86_64
        export RUST_TRIPLET=x86_64-unknown-linux-gnu
        ;;
    armv7)
        export DOCKER_ARCH=arm/v7
        export DNF_ARCH=armv7
        export DEB_ARCH=armhf
        export RUST_TRIPLET=armv7-unknown-linux-gnueabihf
        ;;
    arm64)
        export DOCKER_ARCH=arm64/v8
        export DEB_ARCH=arm64
        export DNF_ARCH=linux64
        export RUST_TRIPLET=aarch64-unknown-linux-gnu
        ;;
    riscv64)
        export DOCKER_ARCH=riscv64
        export DEB_ARCH=riscv64
        export DNF_ARCH=riscv64
        export RUST_TRIPLET=riscv64gc-unknown-linux-gnu
        ;;
    ppc64le)
        export DOCKER_ARCH=ppc64le
        export DEB_ARCH=ppc64el
        export DNF_ARCH=ppc64le
        export RUST_TRIPLET=powerpc64le-unknown-linux-gnu
        ;;
    mips64le)
        export DOCKER_ARCH=mips64le
        export DEB_ARCH=mips64le
        export DNF_ARCH=mips64le
        export RUST_TRIPLET=mips64el-unknown-linux-gnuabi64
        ;;
    s390x)
        export DOCKER_ARCH=s390x
        export DEB_ARCH=s390x
        export DNF_ARCH=s390x
        export RUST_TRIPLET=s390x-unknown-linux-gnu
        ;;
    *)
        echo "ERROR: could not determine architecture settings. PLEASE FIX ME"
        exit 1
        ;;
esac
