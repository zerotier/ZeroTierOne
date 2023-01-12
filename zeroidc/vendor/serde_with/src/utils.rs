pub(crate) mod duration;

use alloc::string::String;
use core::marker::PhantomData;
use serde::de::{Deserialize, MapAccess, SeqAccess};

/// Re-Implementation of `serde::private::de::size_hint::cautious`
#[inline]
pub(crate) fn size_hint_cautious(hint: Option<usize>) -> usize {
    core::cmp::min(hint.unwrap_or(0), 4096)
}

pub(crate) const NANOS_PER_SEC: u32 = 1_000_000_000;
// pub(crate) const NANOS_PER_MILLI: u32 = 1_000_000;
// pub(crate) const NANOS_PER_MICRO: u32 = 1_000;
// pub(crate) const MILLIS_PER_SEC: u64 = 1_000;
// pub(crate) const MICROS_PER_SEC: u64 = 1_000_000;

pub(crate) struct MapIter<'de, A, K, V> {
    pub(crate) access: A,
    marker: PhantomData<(&'de (), K, V)>,
}

impl<'de, A, K, V> MapIter<'de, A, K, V> {
    pub(crate) fn new(access: A) -> Self
    where
        A: MapAccess<'de>,
    {
        Self {
            access,
            marker: PhantomData,
        }
    }
}

impl<'de, A, K, V> Iterator for MapIter<'de, A, K, V>
where
    A: MapAccess<'de>,
    K: Deserialize<'de>,
    V: Deserialize<'de>,
{
    type Item = Result<(K, V), A::Error>;

    fn next(&mut self) -> Option<Self::Item> {
        self.access.next_entry().transpose()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        match self.access.size_hint() {
            Some(size) => (size, Some(size)),
            None => (0, None),
        }
    }
}

pub(crate) struct SeqIter<'de, A, T> {
    access: A,
    marker: PhantomData<(&'de (), T)>,
}

impl<'de, A, T> SeqIter<'de, A, T> {
    pub(crate) fn new(access: A) -> Self
    where
        A: SeqAccess<'de>,
    {
        Self {
            access,
            marker: PhantomData,
        }
    }
}

impl<'de, A, T> Iterator for SeqIter<'de, A, T>
where
    A: SeqAccess<'de>,
    T: Deserialize<'de>,
{
    type Item = Result<T, A::Error>;

    fn next(&mut self) -> Option<Self::Item> {
        self.access.next_element().transpose()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        match self.access.size_hint() {
            Some(size) => (size, Some(size)),
            None => (0, None),
        }
    }
}

pub(crate) fn duration_as_secs_f64(dur: &core::time::Duration) -> f64 {
    (dur.as_secs() as f64) + (dur.subsec_nanos() as f64) / (NANOS_PER_SEC as f64)
}

pub(crate) fn duration_signed_from_secs_f64(
    secs: f64,
) -> Result<self::duration::DurationSigned, String> {
    const MAX_NANOS_F64: f64 = ((u64::max_value() as u128 + 1) * (NANOS_PER_SEC as u128)) as f64;
    // TODO why are the seconds converted to nanoseconds first?
    // Does it make sense to just truncate the value?
    let mut nanos = secs * (NANOS_PER_SEC as f64);
    if !nanos.is_finite() {
        return Err("got non-finite value when converting float to duration".into());
    }
    if nanos >= MAX_NANOS_F64 {
        return Err("overflow when converting float to duration".into());
    }
    let mut sign = self::duration::Sign::Positive;
    if nanos < 0.0 {
        nanos = -nanos;
        sign = self::duration::Sign::Negative;
    }
    let nanos = nanos as u128;
    Ok(self::duration::DurationSigned::new(
        sign,
        (nanos / (NANOS_PER_SEC as u128)) as u64,
        (nanos % (NANOS_PER_SEC as u128)) as u32,
    ))
}
