# Known Answer Tests

The test vectors need to be generated locally. Running [build_kats.sh](./build_kats.sh) will automate the process, otherwise follow the instructions below to clone the C reference repo, compile the test binaries, then generate and rename the files.

This results in 6 files, each containing 10000 KATs, total size is ~600MB:

* tvecs512
* tvecs512-90s
* tvecs768
* tvecs768-90s
* tvecs1024
* tvecs1024-90s

These need to be then moved into the `tests/KATs` folder. The `SHA256SUMS_ORIG` file contains 
the digests this library was tested against.


C Reference Repo: https://github.com/pq-crystals/kyber


## Generating test vectors

```bash
git clone https://github.com/pq-crystals/kyber.git;
cd kyber/ref;
make all;

# Write binary output to file for each security level and mode
# Rename to match C repo sums
for tvec in test_vectors*[^.c];
  do
  sub_str=${tvec/est_/};
  ./$tvec > ${sub_str/tor/};
done;

# Move test vectors and sha256sums into the PQC-Kyber KATs folder
mv tvecs* <Project Root>/tests/KATs

# SHA256SUMS
for tvec in tvecs{5,7,1}*;
do
  sha256sum $tvec >> SHA256SUMS;
done;

# Confirm SHA256SUMS match rust repo KAT's
# Please submit a github issue if upstream test vectors have changed
diff SHA256SUMS_ORIG SHA256SUMS
```