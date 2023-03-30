use criterion::{criterion_group, criterion_main, Criterion};
use std::time::Duration;
use zerotier_network_hypervisor::vl1::identity::Identity;

pub fn criterion_benchmark(c: &mut Criterion) {
    let mut group = c.benchmark_group("basic");
    group.measurement_time(Duration::new(30, 0));
    group.bench_function("identity generation", |b| b.iter(|| Identity::generate(false)));
    group.finish();
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
