#![feature(test)]
extern crate test;

use std::{
    alloc::{alloc_zeroed, dealloc, Layout},
    ptr::NonNull,
};

// AlignedBuffer is like a Box<[u8; N]> except that it is always N-byte aligned
struct AlignedBuffer<const N: usize>(NonNull<[u8; N]>);

impl<const N: usize> AlignedBuffer<N> {
    fn layout() -> Layout {
        Layout::from_size_align(N, N).unwrap()
    }

    fn new() -> Self {
        let p = unsafe { alloc_zeroed(Self::layout()) } as *mut [u8; N];
        Self(NonNull::new(p).unwrap())
    }

    fn buf(&mut self) -> &mut [u8; N] {
        unsafe { self.0.as_mut() }
    }
}

impl<const N: usize> Drop for AlignedBuffer<N> {
    fn drop(&mut self) {
        unsafe { dealloc(self.0.as_ptr() as *mut u8, Self::layout()) }
    }
}

// Used to benchmark the throughput of getrandom in an optimal scenario.
// The buffer is hot, and does not require initialization.
#[inline(always)]
fn bench<const N: usize>(b: &mut test::Bencher) {
    let mut ab = AlignedBuffer::<N>::new();
    let buf = ab.buf();
    b.iter(|| {
        getrandom::getrandom(&mut buf[..]).unwrap();
        test::black_box(&buf);
    });
    b.bytes = N as u64;
}

// Used to benchmark the throughput of getrandom is a slightly less optimal
// scenario. The buffer is still hot, but requires initialization.
#[inline(always)]
fn bench_with_init<const N: usize>(b: &mut test::Bencher) {
    let mut ab = AlignedBuffer::<N>::new();
    let buf = ab.buf();
    b.iter(|| {
        for byte in buf.iter_mut() {
            *byte = 0;
        }
        getrandom::getrandom(&mut buf[..]).unwrap();
        test::black_box(&buf);
    });
    b.bytes = N as u64;
}

// 32 bytes (256-bit) is the seed sized used for rand::thread_rng
const SEED: usize = 32;
// Common size of a page, 4 KiB
const PAGE: usize = 4096;
// Large buffer to get asymptotic performance, 2 MiB
const LARGE: usize = 1 << 21;

#[bench]
fn bench_seed(b: &mut test::Bencher) {
    bench::<SEED>(b);
}
#[bench]
fn bench_seed_init(b: &mut test::Bencher) {
    bench_with_init::<SEED>(b);
}

#[bench]
fn bench_page(b: &mut test::Bencher) {
    bench::<PAGE>(b);
}
#[bench]
fn bench_page_init(b: &mut test::Bencher) {
    bench_with_init::<PAGE>(b);
}

#[bench]
fn bench_large(b: &mut test::Bencher) {
    bench::<LARGE>(b);
}
#[bench]
fn bench_large_init(b: &mut test::Bencher) {
    bench_with_init::<LARGE>(b);
}
