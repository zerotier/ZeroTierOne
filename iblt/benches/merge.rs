use criterion::{criterion_group, criterion_main, Criterion};
use iblt::IBLT;

const CAPACITY: usize = 4096;
type IBLT32 = IBLT<[u8; 32], CAPACITY, 3>;
type IBLT16 = IBLT<[u8; 16], CAPACITY, 3>;
type IBLT8 = IBLT<[u8; 8], CAPACITY, 3>;

pub fn criterion_benchmark(c: &mut Criterion) {
    let mut orig = IBLT32::new();
    let mut new = IBLT32::new();
    for _ in 0..CAPACITY {
        let mut v = [0u8; 32];
        v.fill_with(rand::random);
        orig.insert(&v);
    }

    for _ in 0..CAPACITY {
        let mut v = [0u8; 32];
        v.fill_with(rand::random);
        new.insert(&v);
    }

    c.bench_function("merge 32", |b| {
        b.iter(|| {
            let mut new2 = new.clone();
            orig.subtract(&new);
            new2.subtract(&orig);
        })
    });

    let mut orig = IBLT16::new();
    let mut new = IBLT16::new();
    for _ in 0..CAPACITY {
        let mut v = [0u8; 16];
        v.fill_with(rand::random);
        orig.insert(&v);
    }

    for _ in 0..CAPACITY {
        let mut v = [0u8; 16];
        v.fill_with(rand::random);
        new.insert(&v);
    }

    c.bench_function("merge 16", |b| {
        b.iter(|| {
            let mut new2 = new.clone();
            orig.subtract(&new);
            new2.subtract(&orig);
        })
    });

    let mut orig = IBLT8::new();
    let mut new = IBLT8::new();
    for _ in 0..CAPACITY {
        let mut v = [0u8; 8];
        v.fill_with(rand::random);
        orig.insert(&v);
    }

    for _ in 0..CAPACITY {
        let mut v = [0u8; 8];
        v.fill_with(rand::random);
        new.insert(&v);
    }

    c.bench_function("merge 8", |b| {
        b.iter(|| {
            let mut new2 = new.clone();
            orig.subtract(&new);
            new2.subtract(&orig);
        })
    });
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
