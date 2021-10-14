use std::error::Error;
use std::fmt::{Display, Debug};

pub struct InvalidFormatError;

impl Display for InvalidFormatError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("InvalidFormatError")
    }
}

impl Debug for InvalidFormatError {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        <Self as Display>::fmt(self, f)
    }
}

impl Error for InvalidFormatError {}

pub struct InvalidParameterError(pub(crate) &'static str);

impl Display for InvalidParameterError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "InvalidParameterError: {}", self.0)
    }
}

impl Debug for InvalidParameterError {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        <Self as Display>::fmt(self, f)
    }
}

impl Error for InvalidParameterError {}
