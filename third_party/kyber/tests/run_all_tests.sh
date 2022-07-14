#!/bin/env bash
set -e

# This script runs a matrix of every valid feature combination
# Known Answer Tests are run seperately at the end

# Enable avx2 target features
# Enable LLVM address sanitser checks
export RUSTFLAGS="-Z sanitizer=address -C target-cpu=native -C target-feature=+aes,+avx2,+sse2,+sse4.1,+bmi2,+popcnt"
export RUSTDOCFLAGS="-Z sanitizer=address"

TARGET=$(rustc -vV | sed -n 's|host: ||p')

# Required for address sanitiser checks
rustup default nightly

# Print Headers
announce(){
  title="#    $1    #"
  edge=$(echo "$title" | sed 's/./#/g')
  echo -e "\n\n$edge"; echo "$title"; echo -e "$edge\n\n";
}

##############################################################

# Initial compile
cargo build --tests --features "kyber512" --target $TARGET

announce "Kyber512"
cargo test --features "kyber512" --target $TARGET

announce "Kyber764"
cargo test --target $TARGET

announce "Kyber1024"
cargo test --features "kyber1024" --target $TARGET

announce "Kyber512-90s"
cargo test --features "kyber512 90s" --target $TARGET

announce "Kyber764-90s"
cargo test --features "90s" --target $TARGET

announce "Kyber1024-90s"
cargo test --features "kyber1024 90s" --target $TARGET

announce "Reference Kyber512"
cargo test --features "reference kyber512" --target $TARGET

announce "Reference Kyber764"
cargo test --features "reference" --target $TARGET

announce "Reference Kyber1024"
cargo test --features "reference kyber1024" --target $TARGET

announce "Reference Kyber764-90s"
cargo test --features "reference 90s" --target $TARGET

announce "Reference Kyber512-90s"
cargo test --features "reference kyber512 90s" --target $TARGET

announce "Reference Kyber1024-90s"
cargo test --features "reference kyber1024 90s" --target $TARGET


##############################################################

# Omit santiser for faster test vectors
export RUSTFLAGS="-C target-cpu=native -C target-feature=+aes,+avx2,+sse2,+sse4.1,+bmi2,+popcnt"

cargo build --test kat --features "KATs kyber512"

announce "Kyber512 KATs"
cargo test --test kat --features "KATs kyber512" 

announce "Kyber764 KATs"
cargo test --test kat --features "KATs" 

announce "Kyber1024 KATs"
cargo test --test kat --features "KATs kyber1024" 

announce "Kyber512-90s KATs"
cargo test --test kat --features "KATs kyber512 90s" 

announce "Kyber764-90s KATs"
cargo test --test kat --features "KATs 90s" 

announce "Kyber1024-90s KATs"
cargo test --test kat --features "KATs kyber1024 90s" 

announce "Reference Kyber512 KATs"
cargo test --test kat --features "reference KATs kyber512" 

announce "Reference Kyber764 KATs"
cargo test --test kat --features "reference KATs" 

announce "Reference Kyber1024 KATs"
cargo test --test kat --features "reference KATs kyber1024" 

announce "Reference Kyber764-90s KATs"
cargo test --test kat --features "reference KATs 90s" 

announce "Reference Kyber512-90s KATs"
cargo test --test kat --features "reference KATs kyber512 90s" 

announce "Reference Kyber1024-90s KATs"
cargo test --test kat --features "reference KATs kyber1024 90s" 
