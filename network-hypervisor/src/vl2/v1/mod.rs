mod certificateofmembership;
mod certificateofownership;
mod revocation;
mod tag;

#[repr(u8)]
pub enum CredentialType {
    Null = 0u8,
    CertificateOfMembership = 1,
    Capability = 2,
    Tag = 3,
    CertificateOfOwnership = 4,
    Revocation = 5,
}

pub use certificateofmembership::CertificateOfMembership;
pub use certificateofownership::{CertificateOfOwnership, Thing};
pub use revocation::Revocation;
pub use tag::Tag;
