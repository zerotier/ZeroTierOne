use criterion::{black_box, criterion_group, criterion_main, Criterion};
use tracing::Level;

struct FakeEmptySpan {
    inner: Option<(usize, std::sync::Arc<()>)>,
    meta: Option<&'static ()>,
}

impl FakeEmptySpan {
    fn new() -> Self {
        Self {
            inner: None,
            meta: None,
        }
    }
}

impl Drop for FakeEmptySpan {
    fn drop(&mut self) {
        black_box(&self.inner);
        black_box(&self.meta);
    }
}

fn bench_no_subscriber(c: &mut Criterion) {
    use std::sync::atomic::{AtomicUsize, Ordering};

    let mut group = c.benchmark_group("no_subscriber");

    group.bench_function("span", |b| {
        b.iter(|| {
            let span = tracing::span!(Level::TRACE, "span");
            black_box(&span);
        })
    });
    group.bench_function("span_enter", |b| {
        b.iter(|| {
            let span = tracing::span!(Level::TRACE, "span");
            let _e = span.enter();
        })
    });
    group.bench_function("empty_span", |b| {
        b.iter(|| {
            let span = tracing::span::Span::none();
            black_box(&span);
        });
    });
    group.bench_function("empty_struct", |b| {
        b.iter(|| {
            let span = FakeEmptySpan::new();
            black_box(&span);
        })
    });
    group.bench_function("event", |b| {
        b.iter(|| {
            tracing::event!(Level::TRACE, "hello");
        })
    });
    group.bench_function("relaxed_load", |b| {
        let foo = AtomicUsize::new(1);
        b.iter(|| black_box(foo.load(Ordering::Relaxed)));
    });
    group.bench_function("acquire_load", |b| {
        let foo = AtomicUsize::new(1);
        b.iter(|| black_box(foo.load(Ordering::Acquire)))
    });
    group.bench_function("log", |b| {
        b.iter(|| {
            log::log!(log::Level::Info, "log");
        })
    });
    group.finish();
}

fn bench_fields(c: &mut Criterion) {
    let mut group = c.benchmark_group("no_subscriber_field");
    group.bench_function("span", |b| {
        b.iter(|| {
            black_box(tracing::span!(
                Level::TRACE,
                "span",
                foo = tracing::field::display(format!("bar {:?}", 2))
            ));
        })
    });
    group.bench_function("event", |b| {
        b.iter(|| {
            tracing::event!(
                Level::TRACE,
                foo = tracing::field::display(format!("bar {:?}", 2))
            );
        })
    });
    group.bench_function("log", |b| {
        b.iter(|| log::log!(log::Level::Trace, "{}", format!("bar {:?}", 2)))
    });
    group.finish();
}

criterion_group!(benches, bench_no_subscriber, bench_fields);
criterion_main!(benches);
