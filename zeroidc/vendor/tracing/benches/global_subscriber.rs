use std::fmt::Write;

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use tracing::Level;

use tracing::{span, Event, Id, Metadata};

/// A subscriber that is enabled but otherwise does nothing.
struct EnabledSubscriber;

impl tracing::Subscriber for EnabledSubscriber {
    fn new_span(&self, span: &span::Attributes<'_>) -> Id {
        let _ = span;
        Id::from_u64(0xDEAD_FACE)
    }

    fn event(&self, event: &Event<'_>) {
        let _ = event;
    }

    fn record(&self, span: &Id, values: &span::Record<'_>) {
        let _ = (span, values);
    }

    fn record_follows_from(&self, span: &Id, follows: &Id) {
        let _ = (span, follows);
    }

    fn enabled(&self, metadata: &Metadata<'_>) -> bool {
        let _ = metadata;
        true
    }

    fn enter(&self, span: &Id) {
        let _ = span;
    }

    fn exit(&self, span: &Id) {
        let _ = span;
    }
}

const NOP_LOGGER: NopLogger = NopLogger;

struct NopLogger;

impl log::Log for NopLogger {
    fn enabled(&self, _metadata: &log::Metadata) -> bool {
        true
    }

    fn log(&self, record: &log::Record) {
        if self.enabled(record.metadata()) {
            let mut this = self;
            let _ = write!(this, "{}", record.args());
        }
    }

    fn flush(&self) {}
}

impl Write for &NopLogger {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        black_box(s);
        Ok(())
    }
}

const N_SPANS: usize = 100;

fn criterion_benchmark(c: &mut Criterion) {
    let mut c = c.benchmark_group("global/subscriber");
    let _ = tracing::subscriber::set_global_default(EnabledSubscriber);
    let _ = log::set_logger(&NOP_LOGGER);
    log::set_max_level(log::LevelFilter::Trace);
    c.bench_function("span_no_fields", |b| b.iter(|| span!(Level::TRACE, "span")));

    c.bench_function("event", |b| {
        b.iter(|| {
            tracing::event!(Level::TRACE, "hello");
        })
    });

    c.bench_function("enter_span", |b| {
        let span = span!(Level::TRACE, "span");
        #[allow(clippy::unit_arg)]
        b.iter(|| black_box(span.in_scope(|| {})))
    });

    c.bench_function("span_repeatedly", |b| {
        #[inline]
        fn mk_span(i: u64) -> tracing::Span {
            span!(Level::TRACE, "span", i = i)
        }

        let n = black_box(N_SPANS);
        b.iter(|| (0..n).fold(mk_span(0), |_, i| mk_span(i as u64)))
    });

    c.bench_function("span_with_fields", |b| {
        b.iter(|| {
            span!(
                Level::TRACE,
                "span",
                foo = "foo",
                bar = "bar",
                baz = 3,
                quuux = tracing::field::debug(0.99)
            )
        });
    });
}

fn bench_dispatch(c: &mut Criterion) {
    let mut group = c.benchmark_group("global/dispatch");
    let _ = tracing::subscriber::set_global_default(EnabledSubscriber);
    let _ = log::set_logger(&NOP_LOGGER);
    log::set_max_level(log::LevelFilter::Trace);
    group.bench_function("get_ref", |b| {
        b.iter(|| {
            tracing::dispatcher::get_default(|current| {
                black_box(&current);
            })
        })
    });
    group.bench_function("get_clone", |b| {
        b.iter(|| {
            let current = tracing::dispatcher::get_default(|current| current.clone());
            black_box(current);
        })
    });
    group.finish();
}

criterion_group!(benches, criterion_benchmark, bench_dispatch);
criterion_main!(benches);
