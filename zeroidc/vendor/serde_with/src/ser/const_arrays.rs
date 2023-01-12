use super::*;
use alloc::{borrow::Cow, boxed::Box, collections::BTreeMap};
use std::collections::HashMap;

impl<T, As, const N: usize> SerializeAs<[T; N]> for [As; N]
where
    As: SerializeAs<T>,
{
    fn serialize_as<S>(array: &[T; N], serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        use serde::ser::SerializeTuple;
        let mut arr = serializer.serialize_tuple(N)?;
        for elem in array {
            arr.serialize_element(&SerializeAsWrap::<T, As>::new(elem))?;
        }
        arr.end()
    }
}

macro_rules! tuple_seq_as_map_impl_intern {
    ($tyorig:ty, $ty:ident <K, V>) => {
        #[allow(clippy::implicit_hasher)]
        impl<K, KAs, V, VAs, const N: usize> SerializeAs<$tyorig> for $ty<KAs, VAs>
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
tuple_seq_as_map_impl_intern!([(K, V); N], BTreeMap<K, V>);
tuple_seq_as_map_impl_intern!([(K, V); N], HashMap<K, V>);

impl<const N: usize> SerializeAs<[u8; N]> for Bytes {
    fn serialize_as<S>(bytes: &[u8; N], serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(bytes)
    }
}

impl<const N: usize> SerializeAs<&[u8; N]> for Bytes {
    fn serialize_as<S>(bytes: &&[u8; N], serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(*bytes)
    }
}

impl<const N: usize> SerializeAs<Box<[u8; N]>> for Bytes {
    fn serialize_as<S>(bytes: &Box<[u8; N]>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(&**bytes)
    }
}

impl<'a, const N: usize> SerializeAs<Cow<'a, [u8; N]>> for Bytes {
    fn serialize_as<S>(bytes: &Cow<'a, [u8; N]>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(bytes.as_ref())
    }
}

impl<'a, const N: usize> SerializeAs<Cow<'a, [u8; N]>> for BorrowCow {
    fn serialize_as<S>(value: &Cow<'a, [u8; N]>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.collect_seq(value.iter())
    }
}
