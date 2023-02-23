use criterion::{criterion_group, criterion_main, Criterion};
use std::time::Duration;

use zerotier_crypto::p384::*;
use zerotier_crypto::x25519::*;

pub fn criterion_benchmark(c: &mut Criterion) {
    let p384_a = P384KeyPair::generate();
    let p384_b = P384KeyPair::generate();

    let x25519_a = X25519KeyPair::generate();
    let x25519_b = X25519KeyPair::generate();
    let x25519_b_pub = x25519_b.public_bytes();

    let mut group = c.benchmark_group("cryptography");
    group.measurement_time(Duration::new(10, 0));

    group.bench_function("ecdhp384", |b| {
        b.iter(|| p384_a.agree(p384_b.public_key()).expect("ecdhp384 failed"))
    });
    group.bench_function("ecdhx25519", |b| b.iter(|| x25519_a.agree(&x25519_b_pub)));

    group.finish();
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
