use criterion::{criterion_group, criterion_main, Criterion};
use iblt::IBLT;

const CAPACITY: usize = 4096;
type OurIBLT = IBLT<[u8; 32], CAPACITY, 3>;

pub fn criterion_benchmark(c: &mut Criterion) {
    let mut iblt = OurIBLT::new();
    for _ in 0..CAPACITY {
        let mut v = [0u8; 32];
        v.fill_with(rand::random);
        iblt.insert(&v);
    }

    c.bench_function("clone", |b| b.iter(|| iblt.clone()));
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
