//! A structured representation of a JWT.

pub mod signed;
pub mod verified;

pub struct Unsigned;

pub struct Signed {
    pub token_string: String,
}

pub struct Verified;

pub struct Unverified<'a> {
    pub header_str: &'a str,
    pub claims_str: &'a str,
    pub signature_str: &'a str,
}
