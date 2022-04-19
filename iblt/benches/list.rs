use criterion::{criterion_group, criterion_main, Criterion};
use iblt::IBLT;

const CAPACITY: usize = 4096;
type IBLT32 = IBLT<[u8; 32], CAPACITY, 3>;
type IBLT16 = IBLT<[u8; 16], CAPACITY, 3>;
type IBLT8 = IBLT<[u8; 8], CAPACITY, 3>;

pub fn criterion_benchmark(c: &mut Criterion) {
    let mut iblt = IBLT32::new();
    for _ in 0..CAPACITY {
        let mut v = [0u8; 32];
        v.fill_with(rand::random);
        iblt.insert(&v);
    }

    c.bench_function("list 32", |b| b.iter(|| iblt.list(|_, _| {})));

    let mut iblt = IBLT16::new();
    for _ in 0..CAPACITY {
        let mut v = [0u8; 16];
        v.fill_with(rand::random);
        iblt.insert(&v);
    }

    c.bench_function("list 16", |b| b.iter(|| iblt.list(|_, _| {})));

    let mut iblt = IBLT8::new();
    for _ in 0..CAPACITY {
        let mut v = [0u8; 8];
        v.fill_with(rand::random);
        iblt.insert(&v);
    }

    c.bench_function("list 8", |b| b.iter(|| iblt.list(|_, _| {})));
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
