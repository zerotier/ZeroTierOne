use super::*;
use crate::{
    formats::Strictness, rust::StringWithSeparator, utils::duration::DurationSigned, Separator,
};
use alloc::{
    borrow::Cow,
    boxed::Box,
    collections::{BTreeMap, BTreeSet, BinaryHeap, LinkedList, VecDeque},
    rc::{Rc, Weak as RcWeak},
    string::{String, ToString},
    sync::{Arc, Weak as ArcWeak},
    vec::Vec,
};
use core::{
    cell::{Cell, RefCell},
    convert::TryInto,
    fmt::Display,
    time::Duration,
};
#[cfg(feature = "indexmap")]
use indexmap_crate::{IndexMap, IndexSet};
use serde::ser::Error;
use std::{
    collections::{HashMap, HashSet},
    sync::{Mutex, RwLock},
    time::SystemTime,
};

///////////////////////////////////////////////////////////////////////////////
// region: Simple Wrapper types (e.g., Box, Option)

impl<'a, T, U> SerializeAs<&'a T> for &'a U
where
    U: SerializeAs<T>,
    T: ?Sized,
    U: ?Sized,
{
    fn serialize_as<S>(source: &&'a T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(source).serialize(serializer)
    }
}

impl<'a, T, U> SerializeAs<&'a mut T> for &'a mut U
where
    U: SerializeAs<T>,
    T: ?Sized,
    U: ?Sized,
{
    fn serialize_as<S>(source: &&'a mut T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(source).serialize(serializer)
    }
}

impl<T, U> SerializeAs<Box<T>> for Box<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Box<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(source).serialize(serializer)
    }
}

impl<T, U> SerializeAs<Option<T>> for Option<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Option<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match *source {
            Some(ref value) => serializer.serialize_some(&SerializeAsWrap::<T, U>::new(value)),
            None => serializer.serialize_none(),
        }
    }
}

impl<T, U> SerializeAs<Rc<T>> for Rc<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Rc<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(source).serialize(serializer)
    }
}

impl<T, U> SerializeAs<RcWeak<T>> for RcWeak<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &RcWeak<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<Option<Rc<T>>, Option<Rc<U>>>::new(&source.upgrade())
            .serialize(serializer)
    }
}

impl<T, U> SerializeAs<Arc<T>> for Arc<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Arc<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(source).serialize(serializer)
    }
}

impl<T, U> SerializeAs<ArcWeak<T>> for ArcWeak<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &ArcWeak<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<Option<Arc<T>>, Option<Arc<U>>>::new(&source.upgrade())
            .serialize(serializer)
    }
}

impl<T, U> SerializeAs<Cell<T>> for Cell<U>
where
    U: SerializeAs<T>,
    T: Copy,
{
    fn serialize_as<S>(source: &Cell<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, U>::new(&source.get()).serialize(serializer)
    }
}

impl<T, U> SerializeAs<RefCell<T>> for RefCell<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &RefCell<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match source.try_borrow() {
            Ok(source) => SerializeAsWrap::<T, U>::new(&*source).serialize(serializer),
            Err(_) => Err(S::Error::custom("already mutably borrowed")),
        }
    }
}

impl<T, U> SerializeAs<Mutex<T>> for Mutex<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Mutex<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match source.lock() {
            Ok(source) => SerializeAsWrap::<T, U>::new(&*source).serialize(serializer),
            Err(_) => Err(S::Error::custom("lock poison error while serializing")),
        }
    }
}

impl<T, U> SerializeAs<RwLock<T>> for RwLock<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &RwLock<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match source.read() {
            Ok(source) => SerializeAsWrap::<T, U>::new(&*source).serialize(serializer),
            Err(_) => Err(S::Error::custom("lock poison error while serializing")),
        }
    }
}

impl<T, TAs, E, EAs> SerializeAs<Result<T, E>> for Result<TAs, EAs>
where
    TAs: SerializeAs<T>,
    EAs: SerializeAs<E>,
{
    fn serialize_as<S>(source: &Result<T, E>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        source
            .as_ref()
            .map(SerializeAsWrap::<T, TAs>::new)
            .map_err(SerializeAsWrap::<E, EAs>::new)
            .serialize(serializer)
    }
}

// endregion
///////////////////////////////////////////////////////////////////////////////
// region: Collection Types (e.g., Maps, Sets, Vec)

macro_rules! seq_impl {
    ($ty:ident < T $(: $tbound1:ident $(+ $tbound2:ident)*)* $(, $typaram:ident : $bound:ident )* >) => {
        impl<T, U $(, $typaram)*> SerializeAs<$ty<T $(, $typaram)*>> for $ty<U $(, $typaram)*>
        where
            U: SerializeAs<T>,
            $(T: ?Sized + $tbound1 $(+ $tbound2)*,)*
            $($typaram: ?Sized + $bound,)*
        {
            fn serialize_as<S>(source: &$ty<T $(, $typaram)*>, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_seq(source.iter().map(|item| SerializeAsWrap::<T, U>::new(item)))
            }
        }
    }
}

type BoxedSlice<T> = Box<[T]>;
type Slice<T> = [T];
seq_impl!(BinaryHeap<T>);
seq_impl!(BoxedSlice<T>);
seq_impl!(BTreeSet<T>);
seq_impl!(HashSet<T, H: Sized>);
seq_impl!(LinkedList<T>);
seq_impl!(Slice<T>);
seq_impl!(Vec<T>);
seq_impl!(VecDeque<T>);
#[cfg(feature = "indexmap")]
seq_impl!(IndexSet<T, H: Sized>);

macro_rules! map_impl {
    ($ty:ident < K $(: $kbound1:ident $(+ $kbound2:ident)*)*, V $(, $typaram:ident : $bound:ident)* >) => {
        impl<K, KU, V, VU $(, $typaram)*> SerializeAs<$ty<K, V $(, $typaram)*>> for $ty<KU, VU $(, $typaram)*>
        where
            KU: SerializeAs<K>,
            VU: SerializeAs<V>,
            $(K: ?Sized + $kbound1 $(+ $kbound2)*,)*
            $($typaram: ?Sized + $bound,)*
        {
            fn serialize_as<S>(source: &$ty<K, V $(, $typaram)*>, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_map(source.iter().map(|(k, v)| (SerializeAsWrap::<K, KU>::new(k), SerializeAsWrap::<V, VU>::new(v))))
            }
        }
    }
}

map_impl!(BTreeMap<K, V>);
map_impl!(HashMap<K, V, H: Sized>);
#[cfg(feature = "indexmap")]
map_impl!(IndexMap<K, V, H: Sized>);

macro_rules! tuple_impl {
    ($len:literal $($n:tt $t:ident $tas:ident)+) => {
        impl<$($t, $tas,)+> SerializeAs<($($t,)+)> for ($($tas,)+)
        where
            $($tas: SerializeAs<$t>,)+
        {
            fn serialize_as<S>(tuple: &($($t,)+), serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                use serde::ser::SerializeTuple;
                let mut tup = serializer.serialize_tuple($len)?;
                $(
                    tup.serialize_element(&SerializeAsWrap::<$t, $tas>::new(&tuple.$n))?;
                )+
                tup.end()
            }
        }
    };
}

tuple_impl!(1 0 T0 As0);
tuple_impl!(2 0 T0 As0 1 T1 As1);
tuple_impl!(3 0 T0 As0 1 T1 As1 2 T2 As2);
tuple_impl!(4 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3);
tuple_impl!(5 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4);
tuple_impl!(6 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5);
tuple_impl!(7 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6);
tuple_impl!(8 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7);
tuple_impl!(9 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8);
tuple_impl!(10 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9);
tuple_impl!(11 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10);
tuple_impl!(12 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11);
tuple_impl!(13 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12);
tuple_impl!(14 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12 13 T13 As13);
tuple_impl!(15 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12 13 T13 As13 14 T14 As14);
tuple_impl!(16 0 T0 As0 1 T1 As1 2 T2 As2 3 T3 As3 4 T4 As4 5 T5 As5 6 T6 As6 7 T7 As7 8 T8 As8 9 T9 As9 10 T10 As10 11 T11 As11 12 T12 As12 13 T13 As13 14 T14 As14 15 T15 As15);

macro_rules! map_as_tuple_seq {
    ($ty:ident < K $(: $kbound1:ident $(+ $kbound2:ident)*)*, V >) => {
        impl<K, KAs, V, VAs> SerializeAs<$ty<K, V>> for Vec<(KAs, VAs)>
        where
            KAs: SerializeAs<K>,
            VAs: SerializeAs<V>,
        {
            fn serialize_as<S>(source: &$ty<K, V>, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_seq(source.iter().map(|(k, v)| {
                    (
                        SerializeAsWrap::<K, KAs>::new(k),
                        SerializeAsWrap::<V, VAs>::new(v),
                    )
                }))
            }
        }
    };
}
map_as_tuple_seq!(BTreeMap<K, V>);
// TODO HashMap with a custom hasher support would be better, but results in "unconstrained type parameter"
map_as_tuple_seq!(HashMap<K, V>);
#[cfg(feature = "indexmap")]
map_as_tuple_seq!(IndexMap<K, V>);

// endregion
///////////////////////////////////////////////////////////////////////////////
// region: Conversion types which cause different serialization behavior

impl<T> SerializeAs<T> for Same
where
    T: Serialize + ?Sized,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        source.serialize(serializer)
    }
}

impl<T> SerializeAs<T> for DisplayFromStr
where
    T: Display,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        crate::rust::display_fromstr::serialize(source, serializer)
    }
}

impl<T, U> SerializeAs<Vec<T>> for VecSkipError<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Vec<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        Vec::<U>::serialize_as(source, serializer)
    }
}

impl<AsRefStr> SerializeAs<Option<AsRefStr>> for NoneAsEmptyString
where
    AsRefStr: AsRef<str>,
{
    fn serialize_as<S>(source: &Option<AsRefStr>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        crate::rust::string_empty_as_none::serialize(source, serializer)
    }
}

macro_rules! tuple_seq_as_map_impl_intern {
    ($tyorig:ty, $ty:ident <K, V>) => {
        #[allow(clippy::implicit_hasher)]
        impl<K, KAs, V, VAs> SerializeAs<$tyorig> for $ty<KAs, VAs>
        where
            KAs: SerializeAs<K>,
            VAs: SerializeAs<V>,
        {
            fn serialize_as<S>(source: &$tyorig, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_map(source.iter().map(|(k, v)| {
                    (
                        SerializeAsWrap::<K, KAs>::new(k),
                        SerializeAsWrap::<V, VAs>::new(v),
                    )
                }))
            }
        }
    };
}
macro_rules! tuple_seq_as_map_impl {
    ($($ty:ty $(,)?)+) => {$(
        tuple_seq_as_map_impl_intern!($ty, BTreeMap<K, V>);
        tuple_seq_as_map_impl_intern!($ty, HashMap<K, V>);
    )+}
}

tuple_seq_as_map_impl! {
    BinaryHeap<(K, V)>,
    BTreeSet<(K, V)>,
    LinkedList<(K, V)>,
    Option<(K, V)>,
    Vec<(K, V)>,
    VecDeque<(K, V)>,
}
tuple_seq_as_map_impl!(HashSet<(K, V)>);
#[cfg(feature = "indexmap")]
tuple_seq_as_map_impl!(IndexSet<(K, V)>);

impl<T, TAs> SerializeAs<T> for DefaultOnError<TAs>
where
    TAs: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        TAs::serialize_as(source, serializer)
    }
}

impl SerializeAs<Vec<u8>> for BytesOrString {
    fn serialize_as<S>(source: &Vec<u8>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        source.serialize(serializer)
    }
}

impl<SEPARATOR, I, T> SerializeAs<I> for StringWithSeparator<SEPARATOR, T>
where
    SEPARATOR: Separator,
    for<'a> &'a I: IntoIterator<Item = &'a T>,
    T: ToString,
{
    fn serialize_as<S>(source: &I, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut s = String::new();
        for v in source {
            s.push_str(&*v.to_string());
            s.push_str(SEPARATOR::separator());
        }
        serializer.serialize_str(if !s.is_empty() {
            // remove trailing separator if present
            &s[..s.len() - SEPARATOR::separator().len()]
        } else {
            &s[..]
        })
    }
}

macro_rules! use_signed_duration {
    (
        $main_trait:ident $internal_trait:ident =>
        {
            $ty:ty =>
            $({
                $format:ty, $strictness:ty =>
                $($tbound:ident: $bound:ident $(,)?)*
            })*
        }
    ) => {
        $(
            impl<$($tbound,)*> SerializeAs<$ty> for $main_trait<$format, $strictness>
            where
                $($tbound: $bound,)*
            {
                fn serialize_as<S>(source: &$ty, serializer: S) -> Result<S::Ok, S::Error>
                where
                    S: Serializer,
                {
                    $internal_trait::<$format, $strictness>::serialize_as(
                        &DurationSigned::from(source),
                        serializer,
                    )
                }
            }
        )*
    };
    (
        $( $main_trait:ident $internal_trait:ident, )+ => $rest:tt
    ) => {
        $( use_signed_duration!($main_trait $internal_trait => $rest); )+
    };
}

use_signed_duration!(
    DurationSeconds DurationSeconds,
    DurationMilliSeconds DurationMilliSeconds,
    DurationMicroSeconds DurationMicroSeconds,
    DurationNanoSeconds DurationNanoSeconds,
    => {
        Duration =>
        {u64, STRICTNESS => STRICTNESS: Strictness}
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_signed_duration!(
    DurationSecondsWithFrac DurationSecondsWithFrac,
    DurationMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    DurationMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    DurationNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        Duration =>
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);

use_signed_duration!(
    TimestampSeconds DurationSeconds,
    TimestampMilliSeconds DurationMilliSeconds,
    TimestampMicroSeconds DurationMicroSeconds,
    TimestampNanoSeconds DurationNanoSeconds,
    => {
        SystemTime =>
        {i64, STRICTNESS => STRICTNESS: Strictness}
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_signed_duration!(
    TimestampSecondsWithFrac DurationSecondsWithFrac,
    TimestampMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    TimestampMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    TimestampNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        SystemTime =>
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);

impl<T, U> SerializeAs<T> for DefaultOnNull<U>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_some(&SerializeAsWrap::<T, U>::new(source))
    }
}

impl SerializeAs<&[u8]> for Bytes {
    fn serialize_as<S>(bytes: &&[u8], serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(bytes)
    }
}

impl SerializeAs<Vec<u8>> for Bytes {
    fn serialize_as<S>(bytes: &Vec<u8>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(bytes)
    }
}

impl SerializeAs<Box<[u8]>> for Bytes {
    fn serialize_as<S>(bytes: &Box<[u8]>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(bytes)
    }
}

impl<'a> SerializeAs<Cow<'a, [u8]>> for Bytes {
    fn serialize_as<S>(bytes: &Cow<'a, [u8]>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(bytes)
    }
}

impl<T, U> SerializeAs<Vec<T>> for OneOrMany<U, formats::PreferOne>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Vec<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match source.len() {
            1 => SerializeAsWrap::<T, U>::new(source.iter().next().expect("Cannot be empty"))
                .serialize(serializer),
            _ => SerializeAsWrap::<Vec<T>, Vec<U>>::new(source).serialize(serializer),
        }
    }
}

impl<T, U> SerializeAs<Vec<T>> for OneOrMany<U, formats::PreferMany>
where
    U: SerializeAs<T>,
{
    fn serialize_as<S>(source: &Vec<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<Vec<T>, Vec<U>>::new(source).serialize(serializer)
    }
}

impl<T, TAs1> SerializeAs<T> for PickFirst<(TAs1,)>
where
    TAs1: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, TAs1>::new(source).serialize(serializer)
    }
}

impl<T, TAs1, TAs2> SerializeAs<T> for PickFirst<(TAs1, TAs2)>
where
    TAs1: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, TAs1>::new(source).serialize(serializer)
    }
}

impl<T, TAs1, TAs2, TAs3> SerializeAs<T> for PickFirst<(TAs1, TAs2, TAs3)>
where
    TAs1: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, TAs1>::new(source).serialize(serializer)
    }
}

impl<T, TAs1, TAs2, TAs3, TAs4> SerializeAs<T> for PickFirst<(TAs1, TAs2, TAs3, TAs4)>
where
    TAs1: SerializeAs<T>,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        SerializeAsWrap::<T, TAs1>::new(source).serialize(serializer)
    }
}

impl<T, U> SerializeAs<T> for FromInto<U>
where
    T: Into<U> + Clone,
    U: Serialize,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        source.clone().into().serialize(serializer)
    }
}

impl<T, U> SerializeAs<T> for TryFromInto<U>
where
    T: TryInto<U> + Clone,
    <T as TryInto<U>>::Error: Display,
    U: Serialize,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        source
            .clone()
            .try_into()
            .map_err(S::Error::custom)?
            .serialize(serializer)
    }
}

impl<'a> SerializeAs<Cow<'a, str>> for BorrowCow {
    fn serialize_as<S>(source: &Cow<'a, str>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(source)
    }
}

impl<'a> SerializeAs<Cow<'a, [u8]>> for BorrowCow {
    fn serialize_as<S>(value: &Cow<'a, [u8]>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.collect_seq(value.iter())
    }
}

impl<STRICTNESS: Strictness> SerializeAs<bool> for BoolFromInt<STRICTNESS> {
    fn serialize_as<S>(source: &bool, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_u8(*source as u8)
    }
}

// endregion
