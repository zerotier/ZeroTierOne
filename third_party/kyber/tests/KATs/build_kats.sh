#!/bin/env bash
set -e

current_dir="${PWD##*/}"
if [ $current_dir != "KATs" ];
then
  echo "Script needs to be run from inside the KATs folder";
  echo "Current working directory: "$current_dir
  exit;
fi;

git clone https://github.com/pq-crystals/kyber.git;
cd kyber/ref;
make all;

# Create vectors for each security level and mode
for tvec in test_vectors*[^.c];
  do
  sub_str=${tvec/est_/};
  ./$tvec > ${sub_str/tor/};
done;

echo 'Moving Files...'

# Move test vectors and sha256sums into the PQC-Kyber KATs folder
mv tvecs* ../..
cd ../..

echo 'Calculating SHA256 digests...'

# SHA256SUMS
for tvec in tvecs{5,7,1}*;
do
  shasum -a 256 $tvec >> SHA256SUMS;
done;

# Confirm SHA256SUMS match rust repo KAT's
# Please submit a github issue if upstream tests vectors have changed
echo "Checking SHA256SUMS of test vectors built match those used in the test suite..."
diff -s SHA256SUMS_ORIG SHA256SUMS


