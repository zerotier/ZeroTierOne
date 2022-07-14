# Randbuf Generation

This program generates the deterministic rng output used in the intermediate stages of keypair generation and encoding from KAT seed values. 

`rng.c` and `rng.h` are directly from the NIST submission, `generate_bufs.c` is a stripped down version of `PQCgenKAT_kem.c` to print out the seeded values from `randombytes()` into their respective files. 

These values are then used in place of regular rng output when running the KATs.

### Usage

To build and use: 

```shell
cd tests/rand_bufs
make
./generate
mkdir outputs
mv crypto_kem_keypair indcpa_keypair encode outputs/
```

### Original Files

* [rng.c](https://github.com/pq-crystals/kyber/blob/master/ref/rng.c)
* [rng.h](https://github.com/pq-crystals/kyber/blob/master/ref/rng.h)
* [PQCgenKAT_kem.c](https://github.com/pq-crystals/kyber/blob/master/ref/PQCgenKAT_kem.c)


