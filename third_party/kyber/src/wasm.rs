use super::*;
use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn keypair() -> Keys {
    let mut pk = [0u8; KYBER_PUBLICKEYBYTES];
    let mut sk = [0u8; KYBER_SECRETKEYBYTES];
    kem::crypto_kem_keypair(&mut pk, &mut sk, None);
    Keys {
        pubkey: Box::new(pk),
        secret: Box::new(sk),
        ..Default::default()
    }
}

#[wasm_bindgen]
pub fn encapsulate(pk: Box<[u8]>) -> Result<Keys, JsValue> {
    if pk.len() != KYBER_PUBLICKEYBYTES {
        return Err(JsValue::null());
    }
    let mut ct = [0u8; KYBER_CIPHERTEXTBYTES];
    let mut ss = [0u8; KYBER_SSBYTES];
    kem::crypto_kem_enc(&mut ct, &mut ss, &pk, None);
    Ok(Keys {
        ciphertext: Box::new(ct),
        shared_secret: Box::new(ss),
        ..Default::default()
    })
}

#[wasm_bindgen]
pub fn decapsulate(ct: Box<[u8]>, sk: Box<[u8]>) -> Result<Keys, JsValue> {
    let mut ss = [0u8; KYBER_SSBYTES];
    match kem::crypto_kem_dec(&mut ss, &ct, &sk) {
        Ok(_) => Ok(Keys { shared_secret: Box::new(ss), ..Default::default() }),
        Err(_) => Err(JsValue::null()),
    }
}

#[wasm_bindgen]
#[derive(Default, Clone, Debug)]
pub struct Keys {
    pubkey: Box<[u8]>,
    secret: Box<[u8]>,
    ciphertext: Box<[u8]>,
    shared_secret: Box<[u8]>,
}

#[wasm_bindgen]
impl Keys {
    #[wasm_bindgen(constructor)]
    pub fn new() -> Self {
        Keys::default()
    }

    #[wasm_bindgen(getter)]
    pub fn pubkey(&self) -> Box<[u8]> {
        self.pubkey.clone()
    }

    #[wasm_bindgen(getter)]
    pub fn secret(&self) -> Box<[u8]> {
        self.secret.clone()
    }

    #[wasm_bindgen(getter)]
    pub fn ciphertext(&self) -> Box<[u8]> {
        self.ciphertext.clone()
    }

    #[wasm_bindgen(getter)]
    pub fn shared_secret(&self) -> Box<[u8]> {
        self.shared_secret.clone()
    }

    #[wasm_bindgen(setter)]
    pub fn set_pubkey(&mut self, pubkey: Box<[u8]>) {
        self.pubkey = pubkey;
    }

    #[wasm_bindgen(setter)]
    pub fn set_secret(&mut self, secret: Box<[u8]>) {
        self.secret = secret;
    }

    #[wasm_bindgen(setter)]
    pub fn set_ciphertext(&mut self, ciphertext: Box<[u8]>) {
        self.ciphertext = ciphertext;
    }

    #[wasm_bindgen(setter)]
    pub fn set_shared_secret(&mut self, shared_secret: Box<[u8]>) {
        self.shared_secret = shared_secret;
    }
}
