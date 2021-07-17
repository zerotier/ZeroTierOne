use std::error::Error;
use std::fmt::{Display, Debug};

pub struct InvalidFormatError(pub(crate) &'static str);

impl Display for InvalidFormatError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "UnmarshalError: {}", self.0)
    }
}

impl Debug for InvalidFormatError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "UnmarshalError: {}", self.0)
    }
}

impl Error for InvalidFormatError {}
