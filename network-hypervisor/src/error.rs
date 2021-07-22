use std::error::Error;
use std::fmt::{Display, Debug};

pub struct InvalidFormatError;

impl Display for InvalidFormatError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("InvalidFormatError")
    }
}

impl Debug for InvalidFormatError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("InvalidFormatError")
    }
}

impl Error for InvalidFormatError {}
