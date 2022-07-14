# Fuzzing

This library uses Google's honggfuzz, for more information see the [official page](https://honggfuzz.dev/) or the [rust docs](https://docs.rs/honggfuzz/0.5.54/honggfuzz/)

### Dependencies

* C compiler: cc
* GNU Make: make
* GNU Binutils development files for the BFD library: libbfd.h
* libunwind development files: libunwind.h
* liblzma development files

To install on Debian:

```bash
sudo apt install build-essential binutils-dev libunwind-dev
cargo install honggfuzz
```

The best place to start probing is the unsafe code in the avx2 optimized version. 

So to run on x86_64 platforms:

```bash
export RUSTFLAGS="-Z sanitizer=address -C target-cpu=native -C target-feature=+aes,+avx2,+sse2,+sse4.1,+bmi2,+popcnt"
cargo hfuzz run <TARGET>
```
Run different security levels and modes:

```bash
cargo hfuzz run <TARGET> --features "kyber512 90s"
```

Current targets are: 

* keypair
* encap
* decap