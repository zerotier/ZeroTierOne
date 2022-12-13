// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::error::Error;
use std::fmt::{Debug, Display};

pub struct UnexpectedError;

impl Display for UnexpectedError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("UnexpectedError")
    }
}

impl Debug for UnexpectedError {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        <Self as Display>::fmt(self, f)
    }
}

impl Error for UnexpectedError {}

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

pub struct InvalidParameterError(pub &'static str);

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
