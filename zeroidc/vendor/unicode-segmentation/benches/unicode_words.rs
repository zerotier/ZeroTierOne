use criterion::{black_box, criterion_group, criterion_main, Criterion};

use std::fs;
use unicode_segmentation::UnicodeSegmentation;

fn unicode_words(c: &mut Criterion, lang: &str, path: &str) {
    let text = fs::read_to_string(path).unwrap();
    c.bench_function(&format!("unicode_words_{}", lang), |bench| {
        bench.iter(|| {
            for w in text.unicode_words() {
                black_box(w);
            }
        })
    });
}

fn unicode_words_arabic(c: &mut Criterion) {
    unicode_words(c, "arabic", "benches/texts/arabic.txt");
}

fn unicode_words_english(c: &mut Criterion) {
    unicode_words(c, "english", "benches/texts/english.txt");
}

fn unicode_words_hindi(c: &mut Criterion) {
    unicode_words(c, "hindi", "benches/texts/hindi.txt");
}

fn unicode_words_japanese(c: &mut Criterion) {
    unicode_words(c, "japanese", "benches/texts/japanese.txt");
}

fn unicode_words_korean(c: &mut Criterion) {
    unicode_words(c, "korean", "benches/texts/korean.txt");
}

fn unicode_words_mandarin(c: &mut Criterion) {
    unicode_words(c, "mandarin", "benches/texts/mandarin.txt");
}

fn unicode_words_russian(c: &mut Criterion) {
    unicode_words(c, "russian", "benches/texts/russian.txt");
}

fn unicode_words_source_code(c: &mut Criterion) {
    unicode_words(c, "source_code", "benches/texts/source_code.txt");
}

criterion_group!(
    benches,
    unicode_words_arabic,
    unicode_words_english,
    unicode_words_hindi,
    unicode_words_japanese,
    unicode_words_korean,
    unicode_words_mandarin,
    unicode_words_russian,
    unicode_words_source_code,
);

criterion_main!(benches);
