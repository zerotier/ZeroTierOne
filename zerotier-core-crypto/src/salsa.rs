/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

/// The classic Salsa20 stream cipher supporting 20-round and 12-round variants.
pub struct Salsa(gcrypt::cipher::Cipher);

impl Salsa {
    /// Initialize Salsa cipher.
    /// Key must be 32 bytes and iv must be 8 bytes. If r12 is true the 12-round
    /// variant of Salsa will be used, otherwise 20 rounds are used.
    pub fn new(key: &[u8], iv: &[u8], r12: bool) -> Option<Salsa> {
        if key.len() == 32 && iv.len() == 8 {
            gcrypt::cipher::Cipher::new(if r12 { gcrypt::cipher::Algorithm::Salsa20r12 } else { gcrypt::cipher::Algorithm::Salsa20 }, gcrypt::cipher::Mode::Stream).map_or(None, |mut salsa| {
                let _ = salsa.set_key(key);
                let _ = salsa.set_iv(iv);
                Some(Salsa(salsa))
            })
        } else {
            None
        }
    }

    /// Encrypt or decrypt, which for Salsa is the same operation.
    #[inline(always)]
    pub fn crypt(&mut self, plaintext: &[u8], ciphertext: &mut [u8]) {
        let _ = self.0.encrypt(plaintext, ciphertext);
    }

    #[inline(always)]
    pub fn crypt_in_place(&mut self, plaintext_to_ciphertext: &mut [u8]) {
        let _ = self.0.encrypt_inplace(plaintext_to_ciphertext);
    }
}
