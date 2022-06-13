use criterion::{criterion_group, criterion_main, BenchmarkId,Criterion, Throughput};
use http::header::HeaderName;

// This is a list of some of the standard headers ordered by increasing size.
// It has exactly one standard header per size (some sizes don't have a standard
// header).
const STANDARD_HEADERS_BY_SIZE: &[&str] = &[
    "te",
    "age",
    "date",
    "allow",
    "accept",
    "alt-svc",
    "if-match",
    "forwarded",
    "connection",
    "retry-after",
    "content-type",
    "accept-ranges",
    "accept-charset",
    "accept-encoding",
    "content-encoding",
    "if-modified-since",
    "proxy-authenticate",
    "content-disposition",
    "sec-websocket-accept",
    "sec-websocket-version",
    "access-control-max-age",
    "content-security-policy",
    "sec-websocket-extensions",
    "strict-transport-security",
    "access-control-allow-origin",
    "access-control-allow-headers",
    "access-control-expose-headers",
    "access-control-request-headers",
    "access-control-allow-credentials",
    "content-security-policy-report-only",
];

fn header_name_by_size(c: &mut Criterion) {
    let mut group = c.benchmark_group("std_hdr");
    for name in STANDARD_HEADERS_BY_SIZE {
        group.throughput(Throughput::Bytes(name.len() as u64));
        group.bench_with_input(BenchmarkId::from_parameter(name), name, |b, name| {
            b.iter(|| HeaderName::from_static(name) );
        });
    }
    group.finish();
}

criterion_group!(benches, header_name_by_size);
criterion_main!(benches);
