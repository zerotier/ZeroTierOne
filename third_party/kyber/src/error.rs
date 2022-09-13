#[derive(Debug, PartialEq)]
/// Error types for the failure modes
pub enum KyberError {
    /// One or more inputs to a function are incorrectly sized. A likely cause of this is two parties using different security
    /// levels while trying to negotiate a key exchange.
    InvalidInput,
    /// The ciphertext was unable to be authenticated.
    /// The shared secret was not decapsulated.
    Decapsulation,
}

impl core::fmt::Display for KyberError {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        match *self {
            KyberError::InvalidInput => write!(f, "Function input is of incorrect length"),
            KyberError::Decapsulation => write!(f, "Decapsulation Failure, unable to obtain shared secret from ciphertext"),
        }
    }
}
