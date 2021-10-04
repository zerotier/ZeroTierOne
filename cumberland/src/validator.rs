pub trait Validator {
    /// Check an entry and return true if it should be stored, returned, or replicated.
    fn validate(&self, key: &[u8; 48], value: &[u8]) -> bool;
}

/// A validator that approves everything, mostly for testing.
pub struct NilValidator;

impl Validator for NilValidator {
    fn validate(&self, _: &[u8; 48], _: &[u8]) -> bool { true }
}
