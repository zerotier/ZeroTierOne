/// The poly1305 message authentication function.
pub struct Poly1305(gcrypt::mac::Mac);

pub const POLY1305_ONE_TIME_KEY_SIZE: usize = 32;
pub const POLY1305_MAC_SIZE: usize = 16;

impl Poly1305 {
    #[inline(always)]
    pub fn new(key: &[u8]) -> Option<Poly1305> {
        if key.len() == 32 {
            gcrypt::mac::Mac::new(gcrypt::mac::Algorithm::Poly1305).map_or(None, |mut poly| {
                let _ = poly.set_key(key);
                Some(Poly1305(poly))
            })
        } else {
            None
        }
    }

    #[inline(always)]
    pub fn update(&mut self, data: &[u8]) {
        let _ = self.0.update(data);
    }

    #[inline(always)]
    pub fn finish(&mut self) -> [u8; POLY1305_MAC_SIZE] {
        let mut mac = [0_u8; POLY1305_MAC_SIZE];
        let _ = self.0.get_mac(&mut mac);
        mac
    }
}
