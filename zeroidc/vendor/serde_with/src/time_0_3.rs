//! De/Serialization of [time v0.3][time] types
//!
//! This modules is only available if using the `time_0_3` feature of the crate.
//!
//! [time]: https://docs.rs/time/0.3/

use crate::{
    de::DeserializeAs,
    formats::{Flexible, Format, Strict, Strictness},
    ser::SerializeAs,
    utils::duration::{DurationSigned, Sign},
    DurationMicroSeconds, DurationMicroSecondsWithFrac, DurationMilliSeconds,
    DurationMilliSecondsWithFrac, DurationNanoSeconds, DurationNanoSecondsWithFrac,
    DurationSeconds, DurationSecondsWithFrac, TimestampMicroSeconds, TimestampMicroSecondsWithFrac,
    TimestampMilliSeconds, TimestampMilliSecondsWithFrac, TimestampNanoSeconds,
    TimestampNanoSecondsWithFrac, TimestampSeconds, TimestampSecondsWithFrac,
};
use alloc::{format, string::String};
use serde::{de, ser::Error as _, Deserializer, Serialize, Serializer};
use std::{convert::TryInto, fmt, time::Duration as StdDuration};
use time_0_3::{
    format_description::well_known::{Rfc2822, Rfc3339},
    Duration, OffsetDateTime, PrimitiveDateTime,
};

/// Create a [`PrimitiveDateTime`] for the Unix Epoch
fn unix_epoch_primitive() -> PrimitiveDateTime {
    PrimitiveDateTime::new(
        time_0_3::Date::from_ordinal_date(1970, 1).unwrap(),
        time_0_3::Time::from_hms_nano(0, 0, 0, 0).unwrap(),
    )
}

/// Convert a [`time::Duration`][time_0_3::Duration] into a [`DurationSigned`]
fn duration_into_duration_signed(dur: &Duration) -> DurationSigned {
    let std_dur = StdDuration::new(
        dur.whole_seconds().unsigned_abs(),
        dur.subsec_nanoseconds().unsigned_abs(),
    );

    DurationSigned::with_duration(
        // A duration of 0 is not positive, so check for negative value.
        if dur.is_negative() {
            Sign::Negative
        } else {
            Sign::Positive
        },
        std_dur,
    )
}

/// Convert a [`DurationSigned`] into a [`time_0_3::Duration`]
fn duration_from_duration_signed<'de, D>(sdur: DurationSigned) -> Result<Duration, D::Error>
where
    D: Deserializer<'de>,
{
    let mut dur: Duration = match sdur.duration.try_into() {
        Ok(dur) => dur,
        Err(msg) => {
            return Err(de::Error::custom(format!(
                "Duration is outside of the representable range: {}",
                msg
            )))
        }
    };
    if sdur.sign.is_negative() {
        dur = -dur;
    }
    Ok(dur)
}

macro_rules! use_duration_signed_ser {
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
            impl<$($tbound ,)*> SerializeAs<$ty> for $main_trait<$format, $strictness>
            where
                $($tbound: $bound,)*
            {
                fn serialize_as<S>(source: &$ty, serializer: S) -> Result<S::Ok, S::Error>
                where
                    S: Serializer,
                {
                    let dur: DurationSigned = $converter(source);
                    $internal_trait::<$format, $strictness>::serialize_as(
                        &dur,
                        serializer,
                    )
                }
            }
        )*
    };
    (
        $( $main_trait:ident $internal_trait:ident, )+ => $rest:tt
    ) => {
        $( use_duration_signed_ser!($main_trait $internal_trait => $rest); )+
    };
}

fn offset_datetime_to_duration(source: &OffsetDateTime) -> DurationSigned {
    duration_into_duration_signed(&(*source - OffsetDateTime::UNIX_EPOCH))
}

fn primitive_datetime_to_duration(source: &PrimitiveDateTime) -> DurationSigned {
    duration_into_duration_signed(&(*source - unix_epoch_primitive()))
}

use_duration_signed_ser!(
    DurationSeconds DurationSeconds,
    DurationMilliSeconds DurationMilliSeconds,
    DurationMicroSeconds DurationMicroSeconds,
    DurationNanoSeconds DurationNanoSeconds,
    => {
        Duration; duration_into_duration_signed =>
        {i64, STRICTNESS => STRICTNESS: Strictness}
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_duration_signed_ser!(
    TimestampSeconds DurationSeconds,
    TimestampMilliSeconds DurationMilliSeconds,
    TimestampMicroSeconds DurationMicroSeconds,
    TimestampNanoSeconds DurationNanoSeconds,
    => {
        OffsetDateTime; offset_datetime_to_duration =>
        {i64, STRICTNESS => STRICTNESS: Strictness}
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_duration_signed_ser!(
    TimestampSeconds DurationSeconds,
    TimestampMilliSeconds DurationMilliSeconds,
    TimestampMicroSeconds DurationMicroSeconds,
    TimestampNanoSeconds DurationNanoSeconds,
    => {
        PrimitiveDateTime; primitive_datetime_to_duration =>
        {i64, STRICTNESS => STRICTNESS: Strictness}
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);

// Duration/Timestamp WITH FRACTIONS
use_duration_signed_ser!(
    DurationSecondsWithFrac DurationSecondsWithFrac,
    DurationMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    DurationMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    DurationNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        Duration; duration_into_duration_signed =>
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_duration_signed_ser!(
    TimestampSecondsWithFrac DurationSecondsWithFrac,
    TimestampMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    TimestampMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    TimestampNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        OffsetDateTime; offset_datetime_to_duration =>
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);
use_duration_signed_ser!(
    TimestampSecondsWithFrac DurationSecondsWithFrac,
    TimestampMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    TimestampMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    TimestampNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        PrimitiveDateTime; primitive_datetime_to_duration =>
        {f64, STRICTNESS => STRICTNESS: Strictness}
        {String, STRICTNESS => STRICTNESS: Strictness}
    }
);

macro_rules! use_duration_signed_de {
    (
        $main_trait:ident $internal_trait:ident =>
        {
            $ty:ty; $converter:ident =>
            $({
                $format:ty, $strictness:ty =>
                $($tbound:ident: $bound:ident)*
            })*
        }
    ) =>{
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
                    $converter::<D>(dur)
                }
            }
        )*
    };
    (
        $( $main_trait:ident $internal_trait:ident, )+ => $rest:tt
    ) => {
        $( use_duration_signed_de!($main_trait $internal_trait => $rest); )+
    };
}

fn duration_to_offset_datetime<'de, D>(dur: DurationSigned) -> Result<OffsetDateTime, D::Error>
where
    D: Deserializer<'de>,
{
    Ok(OffsetDateTime::UNIX_EPOCH + duration_from_duration_signed::<D>(dur)?)
}

fn duration_to_primitive_datetime<'de, D>(
    dur: DurationSigned,
) -> Result<PrimitiveDateTime, D::Error>
where
    D: Deserializer<'de>,
{
    Ok(unix_epoch_primitive() + duration_from_duration_signed::<D>(dur)?)
}

// No subsecond precision
use_duration_signed_de!(
    DurationSeconds DurationSeconds,
    DurationMilliSeconds DurationMilliSeconds,
    DurationMicroSeconds DurationMicroSeconds,
    DurationNanoSeconds DurationNanoSeconds,
    => {
        Duration; duration_from_duration_signed =>
        {i64, Strict =>}
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);
use_duration_signed_de!(
    TimestampSeconds DurationSeconds,
    TimestampMilliSeconds DurationMilliSeconds,
    TimestampMicroSeconds DurationMicroSeconds,
    TimestampNanoSeconds DurationNanoSeconds,
    => {
        OffsetDateTime; duration_to_offset_datetime =>
        {i64, Strict =>}
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);
use_duration_signed_de!(
    TimestampSeconds DurationSeconds,
    TimestampMilliSeconds DurationMilliSeconds,
    TimestampMicroSeconds DurationMicroSeconds,
    TimestampNanoSeconds DurationNanoSeconds,
    => {
        PrimitiveDateTime; duration_to_primitive_datetime =>
        {i64, Strict =>}
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);

// Duration/Timestamp WITH FRACTIONS
use_duration_signed_de!(
    DurationSecondsWithFrac DurationSecondsWithFrac,
    DurationMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    DurationMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    DurationNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        Duration; duration_from_duration_signed =>
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);
use_duration_signed_de!(
    TimestampSecondsWithFrac DurationSecondsWithFrac,
    TimestampMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    TimestampMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    TimestampNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        OffsetDateTime; duration_to_offset_datetime =>
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);
use_duration_signed_de!(
    TimestampSecondsWithFrac DurationSecondsWithFrac,
    TimestampMilliSecondsWithFrac DurationMilliSecondsWithFrac,
    TimestampMicroSecondsWithFrac DurationMicroSecondsWithFrac,
    TimestampNanoSecondsWithFrac DurationNanoSecondsWithFrac,
    => {
        PrimitiveDateTime; duration_to_primitive_datetime =>
        {f64, Strict =>}
        {String, Strict =>}
        {FORMAT, Flexible => FORMAT: Format}
    }
);

impl SerializeAs<OffsetDateTime> for Rfc2822 {
    fn serialize_as<S>(datetime: &OffsetDateTime, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        datetime
            .format(&Rfc2822)
            .map_err(S::Error::custom)?
            .serialize(serializer)
    }
}

impl<'de> DeserializeAs<'de, OffsetDateTime> for Rfc2822 {
    fn deserialize_as<D>(deserializer: D) -> Result<OffsetDateTime, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct Visitor;
        impl<'de> de::Visitor<'de> for Visitor {
            type Value = OffsetDateTime;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a RFC2822-formatted `OffsetDateTime`")
            }

            fn visit_str<E: de::Error>(self, value: &str) -> Result<Self::Value, E> {
                Self::Value::parse(value, &Rfc2822).map_err(E::custom)
            }
        }

        deserializer.deserialize_str(Visitor)
    }
}

impl SerializeAs<OffsetDateTime> for Rfc3339 {
    fn serialize_as<S>(datetime: &OffsetDateTime, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        datetime
            .format(&Rfc3339)
            .map_err(S::Error::custom)?
            .serialize(serializer)
    }
}

impl<'de> DeserializeAs<'de, OffsetDateTime> for Rfc3339 {
    fn deserialize_as<D>(deserializer: D) -> Result<OffsetDateTime, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct Visitor;
        impl<'de> de::Visitor<'de> for Visitor {
            type Value = OffsetDateTime;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a RFC3339-formatted `OffsetDateTime`")
            }

            fn visit_str<E: de::Error>(self, value: &str) -> Result<Self::Value, E> {
                Self::Value::parse(value, &Rfc3339).map_err(E::custom)
            }
        }

        deserializer.deserialize_str(Visitor)
    }
}
