use super::*;
use crate::{
    formats::{Flexible, Format, Strict},
    rust::StringWithSeparator,
    utils,
    utils::duration::DurationSigned,
};
use alloc::{
    borrow::{Cow, ToOwned},
    boxed::Box,
    collections::{BTreeMap, BTreeSet, BinaryHeap, LinkedList, VecDeque},
    rc::{Rc, Weak as RcWeak},
    string::String,
    sync::{Arc, Weak as ArcWeak},
    vec::Vec,
};
use core::{
    cell::{Cell, RefCell},
    convert::TryInto,
    fmt::{self, Display},
    hash::{BuildHasher, Hash},
    iter::FromIterator,
    str::FromStr,
    time::Duration,
};
#[cfg(feature = "indexmap")]
use indexmap_crate::{IndexMap, IndexSet};
use serde::de::*;
use std::{
    collections::{HashMap, HashSet},
    sync::{Mutex, RwLock},
    time::SystemTime,
};

///////////////////////////////////////////////////////////////////////////////
// region: Simple Wrapper types (e.g., Box, Option)

impl<'de, T, U> DeserializeAs<'de, Box<T>> for Box<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Box<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Box::new(
            DeserializeAsWrap::<T, U>::deserialize(deserializer)?.into_inner(),
        ))
    }
}

impl<'de, T, U> DeserializeAs<'de, Option<T>> for Option<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Option<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct OptionVisitor<T, U>(PhantomData<(T, U)>);

        impl<'de, T, U> Visitor<'de> for OptionVisitor<T, U>
        where
            U: DeserializeAs<'de, T>,
        {
            type Value = Option<T>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("option")
            }

            #[inline]
            fn visit_unit<E>(self) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(None)
            }

            #[inline]
            fn visit_none<E>(self) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(None)
            }

            #[inline]
            fn visit_some<D>(self, deserializer: D) -> Result<Self::Value, D::Error>
            where
                D: Deserializer<'de>,
            {
                U::deserialize_as(deserializer).map(Some)
            }
        }

        deserializer.deserialize_option(OptionVisitor::<T, U>(PhantomData))
    }
}

impl<'de, T, U> DeserializeAs<'de, Rc<T>> for Rc<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Rc<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Rc::new(
            DeserializeAsWrap::<T, U>::deserialize(deserializer)?.into_inner(),
        ))
    }
}

impl<'de, T, U> DeserializeAs<'de, RcWeak<T>> for RcWeak<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<RcWeak<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        DeserializeAsWrap::<Option<Rc<T>>, Option<Rc<U>>>::deserialize(deserializer)?;
        Ok(RcWeak::new())
    }
}

impl<'de, T, U> DeserializeAs<'de, Arc<T>> for Arc<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Arc<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Arc::new(
            DeserializeAsWrap::<T, U>::deserialize(deserializer)?.into_inner(),
        ))
    }
}

impl<'de, T, U> DeserializeAs<'de, ArcWeak<T>> for ArcWeak<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<ArcWeak<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        DeserializeAsWrap::<Option<Arc<T>>, Option<Arc<U>>>::deserialize(deserializer)?;
        Ok(ArcWeak::new())
    }
}

impl<'de, T, U> DeserializeAs<'de, Cell<T>> for Cell<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Cell<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Cell::new(
            DeserializeAsWrap::<T, U>::deserialize(deserializer)?.into_inner(),
        ))
    }
}

impl<'de, T, U> DeserializeAs<'de, RefCell<T>> for RefCell<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<RefCell<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(RefCell::new(
            DeserializeAsWrap::<T, U>::deserialize(deserializer)?.into_inner(),
        ))
    }
}

impl<'de, T, U> DeserializeAs<'de, Mutex<T>> for Mutex<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Mutex<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Mutex::new(
            DeserializeAsWrap::<T, U>::deserialize(deserializer)?.into_inner(),
        ))
    }
}

impl<'de, T, U> DeserializeAs<'de, RwLock<T>> for RwLock<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<RwLock<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(RwLock::new(
            DeserializeAsWrap::<T, U>::deserialize(deserializer)?.into_inner(),
        ))
    }
}

impl<'de, T, TAs, E, EAs> DeserializeAs<'de, Result<T, E>> for Result<TAs, EAs>
where
    TAs: DeserializeAs<'de, T>,
    EAs: DeserializeAs<'de, E>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Result<T, E>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(
            match Result::<DeserializeAsWrap<T, TAs>, DeserializeAsWrap<E, EAs>>::deserialize(
                deserializer,
            )? {
                Ok(value) => Ok(value.into_inner()),
                Err(err) => Err(err.into_inner()),
            },
        )
    }
}

// endregion
///////////////////////////////////////////////////////////////////////////////
// region: Collection Types (e.g., Maps, Sets, Vec)

macro_rules! seq_impl {
    (
        $ty:ident < T $(: $tbound1:ident $(+ $tbound2:ident)*)* $(, $typaram:ident : $bound1:ident $(+ $bound2:ident)* )* >,
        $access:ident,
        $with_capacity:expr,
        $append:ident
    ) => {
        // Fix for clippy regression in macros on stable
        // The bug no longer exists on nightly
        // https://github.com/rust-lang/rust-clippy/issues/7768
        #[allow(clippy::semicolon_if_nothing_returned)]
        impl<'de, T, U $(, $typaram)*> DeserializeAs<'de, $ty<T $(, $typaram)*>> for $ty<U $(, $typaram)*>
        where
            U: DeserializeAs<'de, T>,
            $(T: $tbound1 $(+ $tbound2)*,)*
            $($typaram: $bound1 $(+ $bound2)*),*
        {
            fn deserialize_as<D>(deserializer: D) -> Result<$ty<T $(, $typaram)*>, D::Error>
            where
                D: Deserializer<'de>,
            {
                struct SeqVisitor<T, U $(, $typaram)*> {
                    marker: PhantomData<(T, U $(, $typaram)*)>,
                }

                impl<'de, T, U $(, $typaram)*> Visitor<'de> for SeqVisitor<T, U $(, $typaram)*>
                where
                    U: DeserializeAs<'de, T>,
                    $(T: $tbound1 $(+ $tbound2)*,)*
                    $($typaram: $bound1 $(+ $bound2)*),*
                {
                    type Value = $ty<T $(, $typaram)*>;

                    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                        formatter.write_str("a sequence")
                    }

                    fn visit_seq<A>(self, mut $access: A) -> Result<Self::Value, A::Error>
                    where
                        A: SeqAccess<'de>,
                    {
                        let mut values = $with_capacity;

                        while let Some(value) = $access
                            .next_element()?
                            .map(|v: DeserializeAsWrap<T, U>| v.into_inner())
                        {
                            values.$append(value);
                        }

                        Ok(values.into())
                    }
                }

                let visitor = SeqVisitor::<T, U $(, $typaram)*> {
                    marker: PhantomData,
                };
                deserializer.deserialize_seq(visitor)
            }
        }
    };
}

type BoxedSlice<T> = Box<[T]>;
seq_impl!(
    BinaryHeap<T: Ord>,
    seq,
    BinaryHeap::with_capacity(utils::size_hint_cautious(seq.size_hint())),
    push
);
seq_impl!(
    BoxedSlice<T>,
    seq,
    Vec::with_capacity(utils::size_hint_cautious(seq.size_hint())),
    push
);
seq_impl!(BTreeSet<T: Ord>, seq, BTreeSet::new(), insert);
seq_impl!(
    HashSet<T: Eq + Hash, S: BuildHasher + Default>,
    seq,
    HashSet::with_capacity_and_hasher(utils::size_hint_cautious(seq.size_hint()), S::default()),
    insert
);
seq_impl!(LinkedList<T>, seq, LinkedList::new(), push_back);
seq_impl!(
    Vec<T>,
    seq,
    Vec::with_capacity(utils::size_hint_cautious(seq.size_hint())),
    push
);
seq_impl!(
    VecDeque<T>,
    seq,
    VecDeque::with_capacity(utils::size_hint_cautious(seq.size_hint())),
    push_back
);
#[cfg(feature = "indexmap")]
seq_impl!(
    IndexSet<T: Eq + Hash, S: BuildHasher + Default>,
    seq,
    IndexSet::with_capacity_and_hasher(utils::size_hint_cautious(seq.size_hint()), S::default()),
    insert
);

macro_rules! map_impl {
    (
        $ty:ident < K $(: $kbound1:ident $(+ $kbound2:ident)*)*, V $(, $typaram:ident : $bound1:ident $(+ $bound2:ident)*)* >,
        // We need an external name, such that we can use it in the `with_capacity` expression
        $access:ident,
        $with_capacity:expr
    ) => {
        // Fix for clippy regression in macros on stable
        // The bug no longer exists on nightly
        // https://github.com/rust-lang/rust-clippy/issues/7768
        #[allow(clippy::semicolon_if_nothing_returned)]
        impl<'de, K, V, KU, VU $(, $typaram)*> DeserializeAs<'de, $ty<K, V $(, $typaram)*>> for $ty<KU, VU $(, $typaram)*>
        where
            KU: DeserializeAs<'de, K>,
            VU: DeserializeAs<'de, V>,
            $(K: $kbound1 $(+ $kbound2)*,)*
            $($typaram: $bound1 $(+ $bound2)*),*
        {
            fn deserialize_as<D>(deserializer: D) -> Result<$ty<K, V $(, $typaram)*>, D::Error>
            where
                D: Deserializer<'de>,
            {
                struct MapVisitor<K, V, KU, VU $(, $typaram)*> {
                    marker: PhantomData<$ty<K, V $(, $typaram)*>>,
                    marker2: PhantomData<$ty<KU, VU $(, $typaram)*>>,
                }

                impl<'de, K, V, KU, VU $(, $typaram)*> Visitor<'de> for MapVisitor<K, V, KU, VU $(, $typaram)*>
                where
                        KU: DeserializeAs<'de, K>,
                        VU: DeserializeAs<'de, V>,
                        $(K: $kbound1 $(+ $kbound2)*,)*
                        $($typaram: $bound1 $(+ $bound2)*),*
                {
                    type Value = $ty<K, V $(, $typaram)*>;

                    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                        formatter.write_str("a map")
                    }

                    #[inline]
                    fn visit_map<A>(self, mut $access: A) -> Result<Self::Value, A::Error>
                    where
                        A: MapAccess<'de>,
                    {
                        let mut values = $with_capacity;

                        while let Some((key, value)) = ($access.next_entry())?.map(|(k, v): (DeserializeAsWrap::<K, KU>, DeserializeAsWrap::<V, VU>)| (k.into_inner(), v.into_inner())) {
                            values.insert(key, value);
                        }

                        Ok(values)
                    }
                }

                let visitor = MapVisitor::<K, V, KU, VU $(, $typaram)*> { marker: PhantomData, marker2: PhantomData };
                deserializer.deserialize_map(visitor)
            }
        }
    }
}

map_impl!(
    BTreeMap<K: Ord, V>,
    map,
    BTreeMap::new());
map_impl!(
    HashMap<K: Eq + Hash, V, S: BuildHasher + Default>,
    map,
    HashMap::with_capacity_and_hasher(utils::size_hint_cautious(map.size_hint()), S::default()));
#[cfg(feature = "indexmap")]
map_impl!(
    IndexMap<K: Eq + Hash, V, S: BuildHasher + Default>,
    map,
    IndexMap::with_capacity_and_hasher(utils::size_hint_cautious(map.size_hint()), S::default()));

macro_rules! tuple_impl {
    ($len:literal $($n:tt $t:ident $tas:ident)+) => {
        impl<'de, $($t, $tas,)+> DeserializeAs<'de, ($($t,)+)> for ($($tas,)+)
        where
            $($tas: DeserializeAs<'de, $t>,)+
        {
            fn deserialize_as<D>(deserializer: D) -> Result<($($t,)+), D::Error>
            where
                D: Deserializer<'de>,
            {
                struct TupleVisitor<$($t,)+>(PhantomData<($($t,)+)>);

                impl<'de, $($t, $tas,)+> Visitor<'de>
                    for TupleVisitor<$(DeserializeAsWrap<$t, $tas>,)+>
                where
                    $($tas: DeserializeAs<'de, $t>,)+
                {
                    type Value = ($($t,)+);

                    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                        formatter.write_str(concat!("a tuple of size ", $len))
                    }

                    #[allow(non_snake_case)]
                    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
                    where
                        A: SeqAccess<'de>,
                    {
                        $(
                            let $t: DeserializeAsWrap<$t, $tas> = match seq.next_element()? {
                                Some(value) => value,
                                None => return Err(Error::invalid_length($n, &self)),
                            };
                        )+

                        Ok(($($t.into_inner(),)+))
                    }
                }

                deserializer.deserialize_tuple(
                    $len,
                    TupleVisitor::<$(DeserializeAsWrap<$t, $tas>,)+>(PhantomData),
                )
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
    ($ty:ident < K $(: $kbound1:ident $(+ $kbound2:ident)*)*, V>) => {
        impl<'de, K, KAs, V, VAs> DeserializeAs<'de, $ty<K, V>> for Vec<(KAs, VAs)>
        where
            KAs: DeserializeAs<'de, K>,
            VAs: DeserializeAs<'de, V>,
            $(K: $kbound1 $(+ $kbound2)*,)*
        {
            fn deserialize_as<D>(deserializer: D) -> Result<$ty<K, V>, D::Error>
            where
                D: Deserializer<'de>,
            {
                struct SeqVisitor<K, KAs, V, VAs> {
                    marker: PhantomData<(K, KAs, V, VAs)>,
                }

                impl<'de, K, KAs, V, VAs> Visitor<'de> for SeqVisitor<K, KAs, V, VAs>
                where
                    KAs: DeserializeAs<'de, K>,
                    VAs: DeserializeAs<'de, V>,
                    $(K: $kbound1 $(+ $kbound2)*,)*
                {
                    type Value = $ty<K, V>;

                    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                        formatter.write_str("a sequence")
                    }

                    #[inline]
                    fn visit_seq<A>(self, access: A) -> Result<Self::Value, A::Error>
                    where
                        A: SeqAccess<'de>,
                    {
                        let iter = utils::SeqIter::new(access);
                        iter.map(|res| {
                            res.map(
                                |(k, v): (DeserializeAsWrap<K, KAs>, DeserializeAsWrap<V, VAs>)| {
                                    (k.into_inner(), v.into_inner())
                                },
                            )
                        })
                        .collect()
                    }
                }

                let visitor = SeqVisitor::<K, KAs, V, VAs> {
                    marker: PhantomData,
                };
                deserializer.deserialize_seq(visitor)
            }
        }
    };
}
map_as_tuple_seq!(BTreeMap<K: Ord, V>);
map_as_tuple_seq!(HashMap<K: Eq + Hash, V>);
#[cfg(feature = "indexmap")]
map_as_tuple_seq!(IndexMap<K: Eq + Hash, V>);

// endregion
///////////////////////////////////////////////////////////////////////////////
// region: Conversion types which cause different serialization behavior

impl<'de, T: Deserialize<'de>> DeserializeAs<'de, T> for Same {
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        T::deserialize(deserializer)
    }
}

impl<'de, T> DeserializeAs<'de, T> for DisplayFromStr
where
    T: FromStr,
    T::Err: Display,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        crate::rust::display_fromstr::deserialize(deserializer)
    }
}

impl<'de, T, U> DeserializeAs<'de, Vec<T>> for VecSkipError<U>
where
    U: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Vec<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        #[serde(
            untagged,
            bound(deserialize = "DeserializeAsWrap<T, TAs>: Deserialize<'de>")
        )]
        enum GoodOrError<'a, T, TAs>
        where
            TAs: DeserializeAs<'a, T>,
        {
            Good(DeserializeAsWrap<T, TAs>),
            // This consumes one "item" when `T` errors while deserializing.
            // This is necessary to make this work, when instead of having a direct value
            // like integer or string, the deserializer sees a list or map.
            Error(IgnoredAny),
            #[serde(skip)]
            _JustAMarkerForTheLifetime(PhantomData<&'a u32>),
        }

        struct SeqVisitor<T, U> {
            marker: PhantomData<T>,
            marker2: PhantomData<U>,
        }

        impl<'de, T, U> Visitor<'de> for SeqVisitor<T, U>
        where
            U: DeserializeAs<'de, T>,
        {
            type Value = Vec<T>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a sequence")
            }

            fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut values = Vec::with_capacity(seq.size_hint().unwrap_or_default());

                while let Some(value) = seq.next_element()? {
                    if let GoodOrError::<T, U>::Good(value) = value {
                        values.push(value.into_inner());
                    }
                }
                Ok(values)
            }
        }

        let visitor = SeqVisitor::<T, U> {
            marker: PhantomData,
            marker2: PhantomData,
        };
        deserializer.deserialize_seq(visitor)
    }
}

impl<'de, Str> DeserializeAs<'de, Option<Str>> for NoneAsEmptyString
where
    Str: FromStr,
    Str::Err: Display,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Option<Str>, D::Error>
    where
        D: Deserializer<'de>,
    {
        crate::rust::string_empty_as_none::deserialize(deserializer)
    }
}

macro_rules! tuple_seq_as_map_impl_intern {
    ($tyorig:ident < (K $(: $($kbound:ident $(+)?)+)?, V $(: $($vbound:ident $(+)?)+)?)>, $ty:ident <KAs, VAs>) => {
        #[allow(clippy::implicit_hasher)]
        impl<'de, K, KAs, V, VAs> DeserializeAs<'de, $tyorig < (K, V) >> for $ty<KAs, VAs>
        where
            KAs: DeserializeAs<'de, K>,
            VAs: DeserializeAs<'de, V>,
            K: $($($kbound +)*)*,
            V: $($($vbound +)*)*,
        {
            fn deserialize_as<D>(deserializer: D) -> Result<$tyorig < (K, V) >, D::Error>
            where
                D: Deserializer<'de>,
            {
                struct MapVisitor<K, KAs, V, VAs> {
                    marker: PhantomData<(K, KAs, V, VAs)>,
                }

                impl<'de, K, KAs, V, VAs> Visitor<'de> for MapVisitor<K, KAs, V, VAs>
                where
                    KAs: DeserializeAs<'de, K>,
                    VAs: DeserializeAs<'de, V>,
                    K: $($($kbound +)*)*,
                    V: $($($vbound +)*)*,
                {
                    type Value = $tyorig < (K, V) >;

                    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                        formatter.write_str("a map")
                    }

                    #[inline]
                    fn visit_map<A>(self, access: A) -> Result<Self::Value, A::Error>
                    where
                        A: MapAccess<'de>,
                    {
                        let iter = utils::MapIter::new(access);
                        iter.map(|res| {
                            res.map(
                                |(k, v): (DeserializeAsWrap<K, KAs>, DeserializeAsWrap<V, VAs>)| {
                                    (k.into_inner(), v.into_inner())
                                },
                            )
                        })
                        .collect()
                    }
                }

                let visitor = MapVisitor::<K, KAs, V, VAs> {
                    marker: PhantomData,
                };
                deserializer.deserialize_map(visitor)
            }
        }
    }
}
macro_rules! tuple_seq_as_map_impl {
    ($($tyorig:ident < (K $(: $($kbound:ident $(+)?)+)?, V $(: $($vbound:ident $(+)?)+)?)> $(,)?)+) => {$(
        tuple_seq_as_map_impl_intern!($tyorig < (K $(: $($kbound +)+)?, V $(: $($vbound +)+)?) >, BTreeMap<KAs, VAs>);
        tuple_seq_as_map_impl_intern!($tyorig < (K $(: $($kbound +)+)?, V $(: $($vbound +)+)?) >, HashMap<KAs, VAs>);
    )+}
}

tuple_seq_as_map_impl! {
    BinaryHeap<(K: Ord, V: Ord)>,
    BTreeSet<(K: Ord, V: Ord)>,
    LinkedList<(K, V)>,
    Vec<(K, V)>,
    VecDeque<(K, V)>,
}
tuple_seq_as_map_impl!(HashSet<(K: Eq + Hash, V: Eq + Hash)>);
#[cfg(feature = "indexmap")]
tuple_seq_as_map_impl!(IndexSet<(K: Eq + Hash, V: Eq + Hash)>);

macro_rules! tuple_seq_as_map_option_impl {
    ($($ty:ident $(,)?)+) => {$(
        #[allow(clippy::implicit_hasher)]
        impl<'de, K, KAs, V, VAs> DeserializeAs<'de, Option<(K, V)>> for $ty<KAs, VAs>
        where
            KAs: DeserializeAs<'de, K>,
            VAs: DeserializeAs<'de, V>,
        {
            fn deserialize_as<D>(deserializer: D) -> Result<Option<(K, V)>, D::Error>
            where
                D: Deserializer<'de>,
            {
                struct MapVisitor<K, KAs, V, VAs> {
                    marker: PhantomData<(K, KAs, V, VAs)>,
                }

                impl<'de, K, KAs, V, VAs> Visitor<'de> for MapVisitor<K, KAs, V, VAs>
                where
                    KAs: DeserializeAs<'de, K>,
                    VAs: DeserializeAs<'de, V>,
                {
                    type Value = Option<(K, V)>;

                    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                        formatter.write_str("a map of size 1")
                    }

                    #[inline]
                    fn visit_map<A>(self, access: A) -> Result<Self::Value, A::Error>
                    where
                        A: MapAccess<'de>,
                    {
                        let iter = utils::MapIter::new(access);
                        iter.map(|res| {
                            res.map(
                                |(k, v): (DeserializeAsWrap<K, KAs>, DeserializeAsWrap<V, VAs>)| {
                                    (k.into_inner(), v.into_inner())
                                },
                            )
                        })
                        .next()
                        .transpose()
                    }
                }

                let visitor = MapVisitor::<K, KAs, V, VAs> {
                    marker: PhantomData,
                };
                deserializer.deserialize_map(visitor)
            }
        }
    )+}
}
tuple_seq_as_map_option_impl!(BTreeMap);
tuple_seq_as_map_option_impl!(HashMap);

impl<'de, T, TAs> DeserializeAs<'de, T> for DefaultOnError<TAs>
where
    TAs: DeserializeAs<'de, T>,
    T: Default,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        #[serde(
            untagged,
            bound(deserialize = "DeserializeAsWrap<T, TAs>: Deserialize<'de>")
        )]
        enum GoodOrError<'a, T, TAs>
        where
            TAs: DeserializeAs<'a, T>,
        {
            Good(DeserializeAsWrap<T, TAs>),
            // This consumes one "item" when `T` errors while deserializing.
            // This is necessary to make this work, when instead of having a direct value
            // like integer or string, the deserializer sees a list or map.
            Error(IgnoredAny),
            #[serde(skip)]
            _JustAMarkerForTheLifetime(PhantomData<&'a u32>),
        }

        Ok(match Deserialize::deserialize(deserializer) {
            Ok(GoodOrError::<T, TAs>::Good(res)) => res.into_inner(),
            _ => Default::default(),
        })
    }
}

impl<'de> DeserializeAs<'de, Vec<u8>> for BytesOrString {
    fn deserialize_as<D>(deserializer: D) -> Result<Vec<u8>, D::Error>
    where
        D: Deserializer<'de>,
    {
        crate::rust::bytes_or_string::deserialize(deserializer)
    }
}

impl<'de, SEPARATOR, I, T> DeserializeAs<'de, I> for StringWithSeparator<SEPARATOR, T>
where
    SEPARATOR: Separator,
    I: FromIterator<T>,
    T: FromStr,
    T::Err: Display,
{
    fn deserialize_as<D>(deserializer: D) -> Result<I, D::Error>
    where
        D: Deserializer<'de>,
    {
        let s = String::deserialize(deserializer)?;
        if s.is_empty() {
            Ok(None.into_iter().collect())
        } else {
            s.split(SEPARATOR::separator())
                .map(FromStr::from_str)
                .collect::<Result<_, _>>()
                .map_err(Error::custom)
        }
    }
}

macro_rules! use_signed_duration {
    (
        $main_trait:ident $internal_trait:ident =>
        {
            $ty:ty; $converter:ident =>
            $({
                $format:ty, $strictness:ty =>
                $($tbound:ident: $bound:ident $(,)?)*
            })*
        }
    ) => {
        $(
            impl<'de, $($tbound,)*> DeserializeAs<'de, $ty> for $main_trait<$format, $strictness>
            where
                $($tbound: $bound,)*
            {
                fn deserialize_as<D>(deserializer: D) -> Result<$ty, D::Error>
                where
                    D: Deserializer<'de>,
                {
                    let dur: DurationSigned = $internal_trait::<$format, $strictness>::deserialize_as(deserializer)?;
                    dur.$converter::<D>()
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
        Duration; to_std_duration =>
        {u64, Strict =>}
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);
use_signed_duration!(
    DurationSecondsWithFrac DurationSecondsWithFrac,
    DurationMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    DurationMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    DurationNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        Duration; to_std_duration =>
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);

use_signed_duration!(
    TimestampSeconds DurationSeconds,
    TimestampMilliSeconds DurationMilliSeconds,
    TimestampMicroSeconds DurationMicroSeconds,
    TimestampNanoSeconds DurationNanoSeconds,
    => {
        SystemTime; to_system_time =>
        {i64, Strict =>}
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);
use_signed_duration!(
    TimestampSecondsWithFrac DurationSecondsWithFrac,
    TimestampMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    TimestampMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    TimestampNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        SystemTime; to_system_time =>
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);

impl<'de, T, U> DeserializeAs<'de, T> for DefaultOnNull<U>
where
    U: DeserializeAs<'de, T>,
    T: Default,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(Option::<U>::deserialize_as(deserializer)?.unwrap_or_default())
    }
}

impl<'de> DeserializeAs<'de, &'de [u8]> for Bytes {
    fn deserialize_as<D>(deserializer: D) -> Result<&'de [u8], D::Error>
    where
        D: Deserializer<'de>,
    {
        <&'de [u8]>::deserialize(deserializer)
    }
}

// serde_bytes implementation for ByteBuf
// https://github.com/serde-rs/bytes/blob/cbae606b9dc225fc094b031cc84eac9493da2058/src/bytebuf.rs#L196
//
// Implements:
// * visit_seq
// * visit_bytes
// * visit_byte_buf
// * visit_str
// * visit_string
impl<'de> DeserializeAs<'de, Vec<u8>> for Bytes {
    fn deserialize_as<D>(deserializer: D) -> Result<Vec<u8>, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct VecVisitor;

        impl<'de> Visitor<'de> for VecVisitor {
            type Value = Vec<u8>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a byte array")
            }

            fn visit_seq<A>(self, seq: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                utils::SeqIter::new(seq).collect::<Result<_, _>>()
            }

            fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v.to_vec())
            }

            fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v)
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v.as_bytes().to_vec())
            }

            fn visit_string<E>(self, v: String) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v.into_bytes())
            }
        }

        deserializer.deserialize_byte_buf(VecVisitor)
    }
}

impl<'de> DeserializeAs<'de, Box<[u8]>> for Bytes {
    fn deserialize_as<D>(deserializer: D) -> Result<Box<[u8]>, D::Error>
    where
        D: Deserializer<'de>,
    {
        <Bytes as DeserializeAs<'de, Vec<u8>>>::deserialize_as(deserializer)
            .map(|vec| vec.into_boxed_slice())
    }
}

// serde_bytes implementation for Cow<'a, [u8]>
// https://github.com/serde-rs/bytes/blob/cbae606b9dc225fc094b031cc84eac9493da2058/src/de.rs#L77
//
// Implements:
// * visit_borrowed_bytes
// * visit_borrowed_str
// * visit_bytes
// * visit_str
// * visit_byte_buf
// * visit_string
// * visit_seq
impl<'de> DeserializeAs<'de, Cow<'de, [u8]>> for Bytes {
    fn deserialize_as<D>(deserializer: D) -> Result<Cow<'de, [u8]>, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct CowVisitor;

        impl<'de> Visitor<'de> for CowVisitor {
            type Value = Cow<'de, [u8]>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a byte array")
            }

            fn visit_borrowed_bytes<E>(self, v: &'de [u8]) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Borrowed(v))
            }

            fn visit_borrowed_str<E>(self, v: &'de str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Borrowed(v.as_bytes()))
            }

            fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Owned(v.to_vec()))
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Owned(v.as_bytes().to_vec()))
            }

            fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Owned(v))
            }

            fn visit_string<E>(self, v: String) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Owned(v.into_bytes()))
            }

            fn visit_seq<V>(self, seq: V) -> Result<Self::Value, V::Error>
            where
                V: SeqAccess<'de>,
            {
                Ok(Cow::Owned(
                    utils::SeqIter::new(seq).collect::<Result<_, _>>()?,
                ))
            }
        }

        deserializer.deserialize_bytes(CowVisitor)
    }
}

impl<'de, T, U, FORMAT> DeserializeAs<'de, Vec<T>> for OneOrMany<U, FORMAT>
where
    U: DeserializeAs<'de, T>,
    FORMAT: Format,
{
    fn deserialize_as<D>(deserializer: D) -> Result<Vec<T>, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        #[serde(
            untagged,
            bound(deserialize = r#"DeserializeAsWrap<T, U>: Deserialize<'de>,
                DeserializeAsWrap<Vec<T>, Vec<U>>: Deserialize<'de>"#),
            expecting = "a list or single element"
        )]
        enum Helper<'a, T, U>
        where
            U: DeserializeAs<'a, T>,
        {
            One(DeserializeAsWrap<T, U>),
            Many(DeserializeAsWrap<Vec<T>, Vec<U>>),
            #[serde(skip)]
            _JustAMarkerForTheLifetime(PhantomData<&'a u32>),
        }

        let h: Helper<'de, T, U> = Deserialize::deserialize(deserializer)?;
        match h {
            Helper::One(one) => Ok(alloc::vec![one.into_inner()]),
            Helper::Many(many) => Ok(many.into_inner()),
            Helper::_JustAMarkerForTheLifetime(_) => unreachable!(),
        }
    }
}

impl<'de, T, TAs1> DeserializeAs<'de, T> for PickFirst<(TAs1,)>
where
    TAs1: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(DeserializeAsWrap::<T, TAs1>::deserialize(deserializer)?.into_inner())
    }
}

impl<'de, T, TAs1, TAs2> DeserializeAs<'de, T> for PickFirst<(TAs1, TAs2)>
where
    TAs1: DeserializeAs<'de, T>,
    TAs2: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        #[serde(
            untagged,
            bound(deserialize = r#"
                DeserializeAsWrap<T, TAs1>: Deserialize<'de>,
                DeserializeAsWrap<T, TAs2>: Deserialize<'de>,
            "#),
            expecting = "PickFirst could not deserialize data"
        )]
        enum Helper<'a, T, TAs1, TAs2>
        where
            TAs1: DeserializeAs<'a, T>,
            TAs2: DeserializeAs<'a, T>,
        {
            First(DeserializeAsWrap<T, TAs1>),
            Second(DeserializeAsWrap<T, TAs2>),
            #[serde(skip)]
            _JustAMarkerForTheLifetime(PhantomData<&'a u32>),
        }

        let h: Helper<'de, T, TAs1, TAs2> = Deserialize::deserialize(deserializer)?;
        match h {
            Helper::First(first) => Ok(first.into_inner()),
            Helper::Second(second) => Ok(second.into_inner()),
            Helper::_JustAMarkerForTheLifetime(_) => unreachable!(),
        }
    }
}

impl<'de, T, TAs1, TAs2, TAs3> DeserializeAs<'de, T> for PickFirst<(TAs1, TAs2, TAs3)>
where
    TAs1: DeserializeAs<'de, T>,
    TAs2: DeserializeAs<'de, T>,
    TAs3: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        #[serde(
            untagged,
            bound(deserialize = r#"
                DeserializeAsWrap<T, TAs1>: Deserialize<'de>,
                DeserializeAsWrap<T, TAs2>: Deserialize<'de>,
                DeserializeAsWrap<T, TAs3>: Deserialize<'de>,
            "#),
            expecting = "PickFirst could not deserialize data"
        )]
        enum Helper<'a, T, TAs1, TAs2, TAs3>
        where
            TAs1: DeserializeAs<'a, T>,
            TAs2: DeserializeAs<'a, T>,
            TAs3: DeserializeAs<'a, T>,
        {
            First(DeserializeAsWrap<T, TAs1>),
            Second(DeserializeAsWrap<T, TAs2>),
            Third(DeserializeAsWrap<T, TAs3>),
            #[serde(skip)]
            _JustAMarkerForTheLifetime(PhantomData<&'a u32>),
        }

        let h: Helper<'de, T, TAs1, TAs2, TAs3> = Deserialize::deserialize(deserializer)?;
        match h {
            Helper::First(first) => Ok(first.into_inner()),
            Helper::Second(second) => Ok(second.into_inner()),
            Helper::Third(third) => Ok(third.into_inner()),
            Helper::_JustAMarkerForTheLifetime(_) => unreachable!(),
        }
    }
}

impl<'de, T, TAs1, TAs2, TAs3, TAs4> DeserializeAs<'de, T> for PickFirst<(TAs1, TAs2, TAs3, TAs4)>
where
    TAs1: DeserializeAs<'de, T>,
    TAs2: DeserializeAs<'de, T>,
    TAs3: DeserializeAs<'de, T>,
    TAs4: DeserializeAs<'de, T>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        #[serde(
            untagged,
            bound(deserialize = r#"
                DeserializeAsWrap<T, TAs1>: Deserialize<'de>,
                DeserializeAsWrap<T, TAs2>: Deserialize<'de>,
                DeserializeAsWrap<T, TAs3>: Deserialize<'de>,
                DeserializeAsWrap<T, TAs4>: Deserialize<'de>,
            "#),
            expecting = "PickFirst could not deserialize data"
        )]
        enum Helper<'a, T, TAs1, TAs2, TAs3, TAs4>
        where
            TAs1: DeserializeAs<'a, T>,
            TAs2: DeserializeAs<'a, T>,
            TAs3: DeserializeAs<'a, T>,
            TAs4: DeserializeAs<'a, T>,
        {
            First(DeserializeAsWrap<T, TAs1>),
            Second(DeserializeAsWrap<T, TAs2>),
            Third(DeserializeAsWrap<T, TAs3>),
            Forth(DeserializeAsWrap<T, TAs4>),
            #[serde(skip)]
            _JustAMarkerForTheLifetime(PhantomData<&'a u32>),
        }

        let h: Helper<'de, T, TAs1, TAs2, TAs3, TAs4> = Deserialize::deserialize(deserializer)?;
        match h {
            Helper::First(first) => Ok(first.into_inner()),
            Helper::Second(second) => Ok(second.into_inner()),
            Helper::Third(third) => Ok(third.into_inner()),
            Helper::Forth(forth) => Ok(forth.into_inner()),
            Helper::_JustAMarkerForTheLifetime(_) => unreachable!(),
        }
    }
}

impl<'de, T, U> DeserializeAs<'de, T> for FromInto<U>
where
    U: Into<T>,
    U: Deserialize<'de>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(U::deserialize(deserializer)?.into())
    }
}

impl<'de, T, U> DeserializeAs<'de, T> for TryFromInto<U>
where
    U: TryInto<T>,
    <U as TryInto<T>>::Error: Display,
    U: Deserialize<'de>,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        U::deserialize(deserializer)?
            .try_into()
            .map_err(Error::custom)
    }
}

impl<'de> DeserializeAs<'de, Cow<'de, str>> for BorrowCow {
    fn deserialize_as<D>(deserializer: D) -> Result<Cow<'de, str>, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct CowVisitor;

        impl<'de> Visitor<'de> for CowVisitor {
            type Value = Cow<'de, str>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("an optionally borrowed string")
            }

            fn visit_borrowed_str<E>(self, v: &'de str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Borrowed(v))
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Owned(v.to_owned()))
            }

            fn visit_string<E>(self, v: String) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(Cow::Owned(v))
            }
        }

        deserializer.deserialize_str(CowVisitor)
    }
}

impl<'de> DeserializeAs<'de, Cow<'de, [u8]>> for BorrowCow {
    fn deserialize_as<D>(deserializer: D) -> Result<Cow<'de, [u8]>, D::Error>
    where
        D: Deserializer<'de>,
    {
        Bytes::deserialize_as(deserializer)
    }
}

impl<'de> DeserializeAs<'de, bool> for BoolFromInt<Strict> {
    fn deserialize_as<D>(deserializer: D) -> Result<bool, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct U8Visitor;
        impl<'de> Visitor<'de> for U8Visitor {
            type Value = bool;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("an integer 0 or 1")
            }

            fn visit_u8<E>(self, v: u8) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match v {
                    0 => Ok(false),
                    1 => Ok(true),
                    unexp => Err(Error::invalid_value(
                        Unexpected::Unsigned(unexp as u64),
                        &"0 or 1",
                    )),
                }
            }

            fn visit_i8<E>(self, v: i8) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match v {
                    0 => Ok(false),
                    1 => Ok(true),
                    unexp => Err(Error::invalid_value(
                        Unexpected::Signed(unexp as i64),
                        &"0 or 1",
                    )),
                }
            }

            fn visit_u64<E>(self, v: u64) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match v {
                    0 => Ok(false),
                    1 => Ok(true),
                    unexp => Err(Error::invalid_value(Unexpected::Unsigned(unexp), &"0 or 1")),
                }
            }

            fn visit_i64<E>(self, v: i64) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match v {
                    0 => Ok(false),
                    1 => Ok(true),
                    unexp => Err(Error::invalid_value(Unexpected::Signed(unexp), &"0 or 1")),
                }
            }

            fn visit_u128<E>(self, v: u128) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match v {
                    0 => Ok(false),
                    1 => Ok(true),
                    unexp => Err(Error::invalid_value(
                        Unexpected::Unsigned(unexp as u64),
                        &"0 or 1",
                    )),
                }
            }

            fn visit_i128<E>(self, v: i128) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match v {
                    0 => Ok(false),
                    1 => Ok(true),
                    unexp => Err(Error::invalid_value(
                        Unexpected::Unsigned(unexp as u64),
                        &"0 or 1",
                    )),
                }
            }
        }

        deserializer.deserialize_u8(U8Visitor)
    }
}

impl<'de> DeserializeAs<'de, bool> for BoolFromInt<Flexible> {
    fn deserialize_as<D>(deserializer: D) -> Result<bool, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct U8Visitor;
        impl<'de> Visitor<'de> for U8Visitor {
            type Value = bool;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("an integer")
            }

            fn visit_u8<E>(self, v: u8) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v != 0)
            }

            fn visit_i8<E>(self, v: i8) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v != 0)
            }

            fn visit_u64<E>(self, v: u64) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v != 0)
            }

            fn visit_i64<E>(self, v: i64) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v != 0)
            }

            fn visit_u128<E>(self, v: u128) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v != 0)
            }

            fn visit_i128<E>(self, v: i128) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(v != 0)
            }
        }

        deserializer.deserialize_u8(U8Visitor)
    }
}

// endregion
