use super::*;
use alloc::collections::BTreeMap;
use std::collections::HashMap;

macro_rules! array_impl {
    ($($len:literal)+) => {$(
        impl<T, As> SerializeAs<[T; $len]> for [As; $len]
        where
            As: SerializeAs<T>,
        {
            fn serialize_as<S>(array: &[T; $len], serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                use serde::ser::SerializeTuple;
                let mut arr = serializer.serialize_tuple($len)?;
                for elem in array {
                    arr.serialize_element(&SerializeAsWrap::<T, As>::new(elem))?;
                }
                arr.end()
            }
        }
    )+};
}

array_impl!(0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32);

tuple_seq_as_map_impl! {
    [(K, V); 0], [(K, V); 1], [(K, V); 2], [(K, V); 3], [(K, V); 4], [(K, V); 5], [(K, V); 6],
    [(K, V); 7], [(K, V); 8], [(K, V); 9], [(K, V); 10], [(K, V); 11], [(K, V); 12], [(K, V); 13],
    [(K, V); 14], [(K, V); 15], [(K, V); 16], [(K, V); 17], [(K, V); 18], [(K, V); 19], [(K, V); 20],
    [(K, V); 21], [(K, V); 22], [(K, V); 23], [(K, V); 24], [(K, V); 25], [(K, V); 26], [(K, V); 27],
    [(K, V); 28], [(K, V); 29], [(K, V); 30], [(K, V); 31], [(K, V); 32],
}
