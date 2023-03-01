use criterion::{black_box, criterion_group, criterion_main, Criterion};

use std::fs;
use unicode_segmentation::UnicodeSegmentation;

fn word_bounds(c: &mut Criterion, lang: &str, path: &str) {
    let text = fs::read_to_string(path).unwrap();
    c.bench_function(&format!("word_bounds_{}", lang), |bench| {
        bench.iter(|| {
            for w in text.split_word_bounds() {
                black_box(w);
            }
        });
    });
}

fn word_bounds_arabic(c: &mut Criterion) {
    word_bounds(c, "arabic", "benches/texts/arabic.txt");
}

fn word_bounds_english(c: &mut Criterion) {
    word_bounds(c, "english", "benches/texts/english.txt");
}

fn word_bounds_hindi(c: &mut Criterion) {
    word_bounds(c, "hindi", "benches/texts/hindi.txt");
}

fn word_bounds_japanese(c: &mut Criterion) {
    word_bounds(c, "japanese", "benches/texts/japanese.txt");
}

fn word_bounds_korean(c: &mut Criterion) {
    word_bounds(c, "korean", "benches/texts/korean.txt");
}

fn word_bounds_mandarin(c: &mut Criterion) {
    word_bounds(c, "mandarin", "benches/texts/mandarin.txt");
}

fn word_bounds_russian(c: &mut Criterion) {
    word_bounds(c, "russian", "benches/texts/russian.txt");
}

fn word_bounds_source_code(c: &mut Criterion) {
    word_bounds(c, "source_code", "benches/texts/source_code.txt");
}

criterion_group!(
    benches,
    word_bounds_arabic,
    word_bounds_english,
    word_bounds_hindi,
    word_bounds_japanese,
    word_bounds_korean,
    word_bounds_mandarin,
    word_bounds_russian,
    word_bounds_source_code,
);

criterion_main!(benches);
