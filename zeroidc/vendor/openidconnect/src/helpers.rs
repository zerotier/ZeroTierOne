use serde::de::value::MapDeserializer;
use serde::de::{DeserializeOwned, Deserializer, MapAccess, Visitor};
use serde::{Deserialize, Serialize};
use serde_value::{Value, ValueDeserializer};

use std::cmp::PartialEq;
use std::fmt::{Debug, Formatter, Result as FormatterResult};
use std::marker::PhantomData;

pub(crate) trait FlattenFilter {
    fn should_include(field_name: &str) -> bool;
}

/// Helper container for filtering map keys out of serde(flatten). This is needed because
/// [`crate::StandardClaims`] doesn't have a fixed set of field names due to its support for
/// localized claims. Consequently, serde by default passes all of the claims to the deserializer
/// for `AC` (additional claims), leading to duplicate claims. [`FilteredFlatten`] is used for
/// eliminating the duplicate claims.
#[derive(Serialize)]
pub(crate) struct FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: DeserializeOwned + Serialize,
{
    // We include another level of flattening here because the derived flatten
    // ([`serde::private::de::FlatMapDeserializer`]) seems to support a wider set of types
    // (e.g., various forms of enum tagging) than [`serde_value::ValueDeserializer`].
    #[serde(flatten)]
    inner: Flatten<T>,
    #[serde(skip)]
    _phantom: PhantomData<F>,
}
impl<F, T> From<T> for FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: DeserializeOwned + Serialize,
{
    fn from(value: T) -> Self {
        Self {
            inner: Flatten { inner: value },
            _phantom: PhantomData,
        }
    }
}
impl<F, T> AsRef<T> for FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: DeserializeOwned + Serialize,
{
    fn as_ref(&self) -> &T {
        self.inner.as_ref()
    }
}
impl<F, T> AsMut<T> for FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: DeserializeOwned + Serialize,
{
    fn as_mut(&mut self) -> &mut T {
        self.inner.as_mut()
    }
}
impl<F, T> PartialEq for FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: DeserializeOwned + PartialEq + Serialize,
{
    fn eq(&self, other: &Self) -> bool {
        self.inner == other.inner
    }
}
impl<F, T> Clone for FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: Clone + DeserializeOwned + Serialize,
{
    fn clone(&self) -> Self {
        Self {
            inner: Flatten {
                inner: self.inner.inner.clone(),
            },
            _phantom: PhantomData,
        }
    }
}
impl<F, T> Debug for FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: Debug + DeserializeOwned + Serialize,
{
    // Transparent Debug since we don't care about this struct.
    fn fmt(&self, f: &mut Formatter) -> FormatterResult {
        Debug::fmt(&self.inner, f)
    }
}

impl<'de, F, T> Deserialize<'de> for FilteredFlatten<F, T>
where
    F: FlattenFilter,
    T: DeserializeOwned + Serialize,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct MapVisitor<F: FlattenFilter, T: DeserializeOwned + Serialize>(PhantomData<(F, T)>);

        impl<'de, F, T> Visitor<'de> for MapVisitor<F, T>
        where
            F: FlattenFilter,
            T: DeserializeOwned + Serialize,
        {
            type Value = Flatten<T>;

            fn expecting(&self, formatter: &mut Formatter) -> FormatterResult {
                formatter.write_str("map type T")
            }

            fn visit_map<V>(self, mut map: V) -> Result<Self::Value, V::Error>
            where
                V: MapAccess<'de>,
            {
                let mut entries = Vec::<(Value, Value)>::new();
                // JSON only supports String keys, and we really only need to support JSON input.
                while let Some(key) = map.next_key::<Value>()? {
                    let key_str = String::deserialize(ValueDeserializer::new(key.clone()))?;
                    if F::should_include(&key_str) {
                        entries.push((key, map.next_value()?));
                    }
                }

                Deserialize::deserialize(MapDeserializer::new(entries.into_iter()))
                    .map_err(serde_value::DeserializerError::into_error)
            }
        }

        Ok(FilteredFlatten {
            inner: deserializer.deserialize_map(MapVisitor(PhantomData::<(F, T)>))?,
            _phantom: PhantomData,
        })
    }
}

#[derive(Deserialize, Serialize)]
struct Flatten<T>
where
    T: DeserializeOwned + Serialize,
{
    #[serde(flatten, bound = "T: DeserializeOwned + Serialize")]
    inner: T,
}
impl<T> AsRef<T> for Flatten<T>
where
    T: DeserializeOwned + Serialize,
{
    fn as_ref(&self) -> &T {
        &self.inner
    }
}
impl<T> AsMut<T> for Flatten<T>
where
    T: DeserializeOwned + Serialize,
{
    fn as_mut(&mut self) -> &mut T {
        &mut self.inner
    }
}
impl<T> PartialEq for Flatten<T>
where
    T: DeserializeOwned + PartialEq + Serialize,
{
    fn eq(&self, other: &Self) -> bool {
        self.inner == other.inner
    }
}
impl<T> Debug for Flatten<T>
where
    T: Debug + DeserializeOwned + Serialize,
{
    // Transparent Debug since we don't care about this struct.
    fn fmt(&self, f: &mut Formatter) -> FormatterResult {
        Debug::fmt(&self.inner, f)
    }
}
