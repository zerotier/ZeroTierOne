use super::*;
use core::fmt;
use serde::de::*;

macro_rules! array_impl {
    ($len:literal $($idx:tt)*) => {
        impl<'de, T, As> DeserializeAs<'de, [T; $len]> for [As; $len]
        where
            As: DeserializeAs<'de, T>,
        {
            fn deserialize_as<D>(deserializer: D) -> Result<[T; $len], D::Error>
            where
                D: Deserializer<'de>,
            {
                struct ArrayVisitor<T>(PhantomData<T>);

                impl<'de, T, As> Visitor<'de>
                    for ArrayVisitor<DeserializeAsWrap<T, As>>
                where
                    As: DeserializeAs<'de, T>,
                {
                    type Value = [T; $len];

                    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                        formatter.write_str(concat!("an array of size ", $len))
                    }

                    #[allow(non_snake_case)]
                    // Because of 0-size arrays
                    #[allow(unused_variables, unused_mut)]
                    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
                    where
                        A: SeqAccess<'de>,
                    {
                        Ok([$(
                            match seq.next_element::<DeserializeAsWrap<T, As>>()? {
                                Some(value) => value.into_inner(),
                                None => return Err(Error::invalid_length($idx, &self)),
                            },
                        )*])
                    }
                }

                deserializer.deserialize_tuple(
                    $len,
                    ArrayVisitor::<DeserializeAsWrap<T, As>>(PhantomData),
                )
            }
        }
    };
}

array_impl!(0);
array_impl!(1 0);
array_impl!(2 0 1);
array_impl!(3 0 1 2);
array_impl!(4 0 1 2 3);
array_impl!(5 0 1 2 3 4);
array_impl!(6 0 1 2 3 4 5);
array_impl!(7 0 1 2 3 4 5 6);
array_impl!(8 0 1 2 3 4 5 6 7);
array_impl!(9 0 1 2 3 4 5 6 7 8);
array_impl!(10 0 1 2 3 4 5 6 7 8 9);
array_impl!(11 0 1 2 3 4 5 6 7 8 9 10);
array_impl!(12 0 1 2 3 4 5 6 7 8 9 10 11);
array_impl!(13 0 1 2 3 4 5 6 7 8 9 10 11 12);
array_impl!(14 0 1 2 3 4 5 6 7 8 9 10 11 12 13);
array_impl!(15 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14);
array_impl!(16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15);
array_impl!(17 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16);
array_impl!(18 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17);
array_impl!(19 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18);
array_impl!(20 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19);
array_impl!(21 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20);
array_impl!(22 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21);
array_impl!(23 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22);
array_impl!(24 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23);
array_impl!(25 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24);
array_impl!(26 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25);
array_impl!(27 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26);
array_impl!(28 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27);
array_impl!(29 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28);
array_impl!(30 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29);
array_impl!(31 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30);
array_impl!(32 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31);
