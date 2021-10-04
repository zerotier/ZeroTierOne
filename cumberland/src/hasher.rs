/// Hasher responsible for hashing keys.
pub trait Hasher {
    const OUTPUT_SIZE: usize;
    fn new() -> Self;
    fn digest(&mut self, b: &[u8]) -> [u8; Self::OUTPUT_SIZE];
}
