use core::fmt;
use std::ptr;

use crate::bn::{BigNumContext, BigNumContextRef, BigNumRef};
use crate::error::{cvt, cvt_n, cvt_p, ErrorStack};
use foreign_types::{foreign_type, ForeignType, ForeignTypeRef};

foreign_type! {
    pub unsafe type EcGroup: Send + Sync {
        type CType = ffi::EC_GROUP;
        fn drop = ffi::EC_GROUP_free;
    }
    /// Public and optional private key on the given curve.
    pub unsafe type EcKey {
        type CType = ffi::EC_KEY;
        fn drop = ffi::EC_KEY_free;
    }

    pub unsafe type EcPoint {
        type CType = ffi::EC_POINT;
        fn drop = ffi::EC_POINT_free;
    }
}

impl EcKeyRef {
    /// Returns the public key. The returned EcPoint must be treated as read only.
    pub fn public_key(&self) -> &EcPointRef {
        unsafe {
            let ptr = ffi::EC_KEY_get0_public_key(self.as_ptr());
            &*ptr.cast()
        }
    }

    /// Returns the private key value. The returned BigNum must be treated as read only.
    pub fn private_key(&self) -> &BigNumRef {
        unsafe {
            let ptr = ffi::EC_KEY_get0_private_key(self.as_ptr());
            &*ptr.cast()
        }
    }

    /// Checks the key for validity.
    pub fn check_key(&self) -> Result<(), ErrorStack> {
        unsafe { cvt(ffi::EC_KEY_check_key(self.as_ptr())).map(|_| ()) }
    }
}
impl EcKey {
    /// Generates a new public/private key pair on the specified curve.
    pub fn generate(group: &EcGroupRef) -> Result<EcKey, ErrorStack> {
        unsafe {
            cvt_p(ffi::EC_KEY_new())
                .map(|p| EcKey::from_ptr(p))
                .and_then(|key| cvt(ffi::EC_KEY_set_group(key.as_ptr(), group.as_ptr())).map(|_| key))
                .and_then(|key| cvt(ffi::EC_KEY_generate_key(key.as_ptr())).map(|_| key))
        }
    }
    /// Constructs an `EcKey` from the specified group with the associated [`EcPoint`]: `public_key`.
    ///
    /// This will only have the associated `public_key`.
    pub fn from_public_key(group: &EcGroupRef, public_key: &EcPointRef) -> Result<EcKey, ErrorStack> {
        unsafe {
            cvt_p(ffi::EC_KEY_new())
                .map(|p| EcKey::from_ptr(p))
                .and_then(|key| cvt(ffi::EC_KEY_set_group(key.as_ptr(), group.as_ptr())).map(|_| key))
                .and_then(|key| cvt(ffi::EC_KEY_set_public_key(key.as_ptr(), public_key.as_ptr())).map(|_| key))
        }
    }

    /// Constructs an public/private key pair given a curve, a private key and a public key point.
    pub fn from_private_components(group: &EcGroupRef, private_number: &BigNumRef, public_key: &EcPointRef) -> Result<EcKey, ErrorStack> {
        unsafe {
            cvt_p(ffi::EC_KEY_new())
                .map(|p| EcKey::from_ptr(p))
                .and_then(|key| cvt(ffi::EC_KEY_set_group(key.as_ptr(), group.as_ptr())).map(|_| key))
                .and_then(|key| cvt(ffi::EC_KEY_set_private_key(key.as_ptr(), private_number.as_ptr())).map(|_| key))
                .and_then(|key| cvt(ffi::EC_KEY_set_public_key(key.as_ptr(), public_key.as_ptr())).map(|_| key))
        }
    }
}

impl EcPoint {
    /// Creates a new point on the specified curve.
    pub fn new(group: &EcGroupRef) -> Result<EcPoint, ErrorStack> {
        unsafe { cvt_p(ffi::EC_POINT_new(group.as_ptr())).map(|x| EcPoint::from_ptr(x)) }
    }
    /// Creates point from a binary representation
    pub fn from_bytes(group: &EcGroupRef, buf: &[u8], ctx: &mut BigNumContext) -> Result<EcPoint, ErrorStack> {
        let point = EcPoint::new(group)?;
        unsafe {
            cvt(ffi::EC_POINT_oct2point(
                group.as_ptr(),
                point.as_ptr(),
                buf.as_ptr(),
                buf.len(),
                ctx.as_ptr(),
            ))?;
        }
        Ok(point)
    }
}

impl EcPointRef {
    /// Serializes the point to a binary representation.
    pub fn to_bytes(&self, group: &EcGroupRef, form: ffi::point_conversion_form_t, ctx: &BigNumContextRef) -> Result<Vec<u8>, ErrorStack> {
        unsafe {
            let len = ffi::EC_POINT_point2oct(group.as_ptr(), self.as_ptr(), form, ptr::null_mut(), 0, ctx.as_ptr());
            if len == 0 {
                return Err(ErrorStack::get());
            }
            let mut buf = vec![0; len];
            let len = ffi::EC_POINT_point2oct(group.as_ptr(), self.as_ptr(), form, buf.as_mut_ptr(), len, ctx.as_ptr());
            if len == 0 {
                Err(ErrorStack::get())
            } else {
                Ok(buf)
            }
        }
    }
    /// Checks if point is on a given curve
    pub fn is_on_curve(&self, group: &EcGroupRef, ctx: &BigNumContextRef) -> Result<bool, ErrorStack> {
        unsafe {
            let res = cvt_n(ffi::EC_POINT_is_on_curve(group.as_ptr(), self.as_ptr(), ctx.as_ptr()))?;
            Ok(res == 1)
        }
    }
}

impl ToOwned for EcKeyRef {
    type Owned = EcKey;

    fn to_owned(&self) -> EcKey {
        unsafe {
            let r = ffi::EC_KEY_up_ref(self.as_ptr());
            assert!(r == 1);
            EcKey::from_ptr(self.as_ptr())
        }
    }
}

impl Clone for EcKey {
    fn clone(&self) -> EcKey {
        (**self).to_owned()
    }
}

impl fmt::Debug for EcKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "EcKey")
    }
}
