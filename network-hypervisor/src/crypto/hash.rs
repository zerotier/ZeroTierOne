use std::mem::MaybeUninit;
use std::convert::TryInto;
use std::io::Write;

pub struct SHA512(gcrypt::digest::MessageDigest);

impl SHA512 {
    #[inline(always)]
    pub fn hash(b: &[u8]) -> [u8; 64] {
        let mut h = unsafe { MaybeUninit::<[u8; 64]>::uninit().assume_init() };
        gcrypt::digest::hash(gcrypt::digest::Algorithm::Sha512, b, &mut h);
        h
    }

    #[inline(always)]
    pub fn new() -> Self {
        SHA512(gcrypt::digest::MessageDigest::new(gcrypt::digest::Algorithm::Sha512).unwrap())
    }

    #[inline(always)]
    pub fn reset(&mut self) {
        self.0.reset();
    }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) {
        self.0.update(b);
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; 64] {
        self.0.finish();
        self.0.get_only_digest().unwrap().try_into().unwrap()
    }
}

impl Write for SHA512 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.0.write(b)
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> {
        self.0.flush()
    }
}

pub struct SHA384(gcrypt::digest::MessageDigest);

impl SHA384 {
    #[inline(always)]
    pub fn hash(b: &[u8]) -> [u8; 48] {
        let mut h = unsafe { MaybeUninit::<[u8; 48]>::uninit().assume_init() };
        gcrypt::digest::hash(gcrypt::digest::Algorithm::Sha512, b, &mut h);
        h
    }

    #[inline(always)]
    pub fn new() -> Self {
        SHA384(gcrypt::digest::MessageDigest::new(gcrypt::digest::Algorithm::Sha384).unwrap())
    }

    #[inline(always)]
    pub fn reset(&mut self) {
        self.0.reset();
    }

    #[inline(always)]
    pub fn update(&mut self, b: &[u8]) {
        self.0.update(b);
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; 48] {
        self.0.finish();
        self.0.get_only_digest().unwrap().try_into().unwrap()
    }
}

impl Write for SHA384 {
    #[inline(always)]
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.0.write(b)
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> {
        self.0.flush()
    }
}
