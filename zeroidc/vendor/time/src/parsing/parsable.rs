//! A trait that can be used to parse an item from an input.

use core::convert::TryInto;
use core::ops::Deref;

use crate::error::TryFromParsed;
use crate::format_description::well_known::{Rfc2822, Rfc3339};
use crate::format_description::FormatItem;
use crate::parsing::{Parsed, ParsedItem};
use crate::{error, Date, Month, OffsetDateTime, PrimitiveDateTime, Time, UtcOffset, Weekday};

/// A type that can be parsed.
#[cfg_attr(__time_03_docs, doc(notable_trait))]
pub trait Parsable: sealed::Sealed {}
impl Parsable for FormatItem<'_> {}
impl Parsable for [FormatItem<'_>] {}
impl Parsable for Rfc2822 {}
impl Parsable for Rfc3339 {}
impl<T: Deref> Parsable for T where T::Target: Parsable {}

/// Seal the trait to prevent downstream users from implementing it, while still allowing it to
/// exist in generic bounds.
mod sealed {

    #[allow(clippy::wildcard_imports)]
    use super::*;

    /// Parse the item using a format description and an input.
    #[cfg_attr(__time_03_docs, doc(cfg(feature = "parsing")))]
    pub trait Sealed {
        /// Parse the item into the provided [`Parsed`] struct.
        ///
        /// This method can be used to parse a single component without parsing the full value.
        fn parse_into<'a>(
            &self,
            input: &'a [u8],
            parsed: &mut Parsed,
        ) -> Result<&'a [u8], error::Parse>;

        /// Parse the item into a new [`Parsed`] struct.
        ///
        /// This method can only be used to parse a complete value of a type. If any characters
        /// remain after parsing, an error will be returned.
        fn parse(&self, input: &[u8]) -> Result<Parsed, error::Parse> {
            let mut parsed = Parsed::new();
            if self.parse_into(input, &mut parsed)?.is_empty() {
                Ok(parsed)
            } else {
                Err(error::Parse::UnexpectedTrailingCharacters)
            }
        }

        /// Parse a [`Date`] from the format description.
        fn parse_date(&self, input: &[u8]) -> Result<Date, error::Parse> {
            Ok(self.parse(input)?.try_into()?)
        }

        /// Parse a [`Time`] from the format description.
        fn parse_time(&self, input: &[u8]) -> Result<Time, error::Parse> {
            Ok(self.parse(input)?.try_into()?)
        }

        /// Parse a [`UtcOffset`] from the format description.
        fn parse_offset(&self, input: &[u8]) -> Result<UtcOffset, error::Parse> {
            Ok(self.parse(input)?.try_into()?)
        }

        /// Parse a [`PrimitiveDateTime`] from the format description.
        fn parse_date_time(&self, input: &[u8]) -> Result<PrimitiveDateTime, error::Parse> {
            Ok(self.parse(input)?.try_into()?)
        }

        /// Parse a [`OffsetDateTime`] from the format description.
        fn parse_offset_date_time(&self, input: &[u8]) -> Result<OffsetDateTime, error::Parse> {
            Ok(self.parse(input)?.try_into()?)
        }
    }
}

// region: custom formats
impl sealed::Sealed for FormatItem<'_> {
    fn parse_into<'a>(
        &self,
        input: &'a [u8],
        parsed: &mut Parsed,
    ) -> Result<&'a [u8], error::Parse> {
        Ok(parsed.parse_item(input, self)?)
    }
}

impl sealed::Sealed for [FormatItem<'_>] {
    fn parse_into<'a>(
        &self,
        input: &'a [u8],
        parsed: &mut Parsed,
    ) -> Result<&'a [u8], error::Parse> {
        Ok(parsed.parse_items(input, self)?)
    }
}

impl<T: Deref> sealed::Sealed for T
where
    T::Target: sealed::Sealed,
{
    fn parse_into<'a>(
        &self,
        input: &'a [u8],
        parsed: &mut Parsed,
    ) -> Result<&'a [u8], error::Parse> {
        self.deref().parse_into(input, parsed)
    }
}
// endregion custom formats

// region: well-known formats
impl sealed::Sealed for Rfc2822 {
    fn parse_into<'a>(
        &self,
        input: &'a [u8],
        parsed: &mut Parsed,
    ) -> Result<&'a [u8], error::Parse> {
        use crate::error::ParseFromDescription::{InvalidComponent, InvalidLiteral};
        use crate::parsing::combinator::rfc::rfc2822::{cfws, fws};
        use crate::parsing::combinator::{
            ascii_char, exactly_n_digits, first_match, n_to_m_digits, opt, sign,
        };

        let colon = ascii_char::<b':'>;
        let comma = ascii_char::<b','>;

        let input = opt(fws)(input).into_inner();
        let input = first_match(
            [
                (&b"Mon"[..], Weekday::Monday),
                (&b"Tue"[..], Weekday::Tuesday),
                (&b"Wed"[..], Weekday::Wednesday),
                (&b"Thu"[..], Weekday::Thursday),
                (&b"Fri"[..], Weekday::Friday),
                (&b"Sat"[..], Weekday::Saturday),
                (&b"Sun"[..], Weekday::Sunday),
            ],
            false,
        )(input)
        .and_then(|item| item.consume_value(|value| parsed.set_weekday(value)))
        .ok_or(InvalidComponent("weekday"))?;
        let input = comma(input).ok_or(InvalidLiteral)?.into_inner();
        let input = cfws(input).ok_or(InvalidLiteral)?.into_inner();
        let input = n_to_m_digits::<_, 1, 2>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_day(value)))
            .ok_or(InvalidComponent("day"))?;
        let input = cfws(input).ok_or(InvalidLiteral)?.into_inner();
        let input = first_match(
            [
                (&b"Jan"[..], Month::January),
                (&b"Feb"[..], Month::February),
                (&b"Mar"[..], Month::March),
                (&b"Apr"[..], Month::April),
                (&b"May"[..], Month::May),
                (&b"Jun"[..], Month::June),
                (&b"Jul"[..], Month::July),
                (&b"Aug"[..], Month::August),
                (&b"Sep"[..], Month::September),
                (&b"Oct"[..], Month::October),
                (&b"Nov"[..], Month::November),
                (&b"Dec"[..], Month::December),
            ],
            false,
        )(input)
        .and_then(|item| item.consume_value(|value| parsed.set_month(value)))
        .ok_or(InvalidComponent("month"))?;
        let input = cfws(input).ok_or(InvalidLiteral)?.into_inner();
        let input = match exactly_n_digits::<u32, 4>(input) {
            Some(item) => {
                let input = item
                    .flat_map(|year| if year >= 1900 { Some(year) } else { None })
                    .and_then(|item| item.consume_value(|value| parsed.set_year(value as _)))
                    .ok_or(InvalidComponent("year"))?;
                let input = fws(input).ok_or(InvalidLiteral)?.into_inner();
                input
            }
            None => {
                let input = exactly_n_digits::<u32, 2>(input)
                    .and_then(|item| {
                        item.map(|year| if year < 50 { year + 2000 } else { year + 1900 })
                            .map(|year| year as _)
                            .consume_value(|value| parsed.set_year(value))
                    })
                    .ok_or(InvalidComponent("year"))?;
                let input = cfws(input).ok_or(InvalidLiteral)?.into_inner();
                input
            }
        };

        let input = exactly_n_digits::<_, 2>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_hour_24(value)))
            .ok_or(InvalidComponent("hour"))?;
        let input = opt(cfws)(input).into_inner();
        let input = colon(input).ok_or(InvalidLiteral)?.into_inner();
        let input = opt(cfws)(input).into_inner();
        let input = exactly_n_digits::<_, 2>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_minute(value)))
            .ok_or(InvalidComponent("minute"))?;

        let input = if let Some(input) = colon(opt(cfws)(input).into_inner()) {
            let input = input.into_inner(); // discard the colon
            let input = opt(cfws)(input).into_inner();
            let input = exactly_n_digits::<_, 2>(input)
                .and_then(|item| item.consume_value(|value| parsed.set_second(value)))
                .ok_or(InvalidComponent("second"))?;
            let input = cfws(input).ok_or(InvalidLiteral)?.into_inner();
            input
        } else {
            cfws(input).ok_or(InvalidLiteral)?.into_inner()
        };

        // The RFC explicitly allows leap seconds.
        parsed.set_leap_second_allowed(true);

        #[allow(clippy::unnecessary_lazy_evaluations)] // rust-lang/rust-clippy#8522
        let zone_literal = first_match(
            [
                (&b"UT"[..], 0),
                (&b"GMT"[..], 0),
                (&b"EST"[..], -5),
                (&b"EDT"[..], -4),
                (&b"CST"[..], -6),
                (&b"CDT"[..], -5),
                (&b"MST"[..], -7),
                (&b"MDT"[..], -6),
                (&b"PST"[..], -8),
                (&b"PDT"[..], -7),
            ],
            false,
        )(input)
        .or_else(|| match input {
            [
                b'a'..=b'i' | b'k'..=b'z' | b'A'..=b'I' | b'K'..=b'Z',
                rest @ ..,
            ] => Some(ParsedItem(rest, 0)),
            _ => None,
        });
        if let Some(zone_literal) = zone_literal {
            let input = zone_literal
                .consume_value(|value| parsed.set_offset_hour(value))
                .ok_or(InvalidComponent("offset hour"))?;
            parsed
                .set_offset_minute_signed(0)
                .ok_or(InvalidComponent("offset minute"))?;
            parsed
                .set_offset_second_signed(0)
                .ok_or(InvalidComponent("offset second"))?;
            return Ok(input);
        }

        let ParsedItem(input, offset_sign) = sign(input).ok_or(InvalidComponent("offset hour"))?;
        let input = exactly_n_digits::<u8, 2>(input)
            .and_then(|item| {
                item.map(|offset_hour| {
                    if offset_sign == b'-' {
                        -(offset_hour as i8)
                    } else {
                        offset_hour as _
                    }
                })
                .consume_value(|value| parsed.set_offset_hour(value))
            })
            .ok_or(InvalidComponent("offset hour"))?;
        let input = exactly_n_digits::<u8, 2>(input)
            .and_then(|item| {
                item.consume_value(|value| parsed.set_offset_minute_signed(value as _))
            })
            .ok_or(InvalidComponent("offset minute"))?;

        Ok(input)
    }
}

impl sealed::Sealed for Rfc3339 {
    fn parse_into<'a>(
        &self,
        input: &'a [u8],
        parsed: &mut Parsed,
    ) -> Result<&'a [u8], error::Parse> {
        use crate::error::ParseFromDescription::{InvalidComponent, InvalidLiteral};
        use crate::parsing::combinator::{
            any_digit, ascii_char, ascii_char_ignore_case, exactly_n_digits, sign,
        };

        let dash = ascii_char::<b'-'>;
        let colon = ascii_char::<b':'>;

        let input = exactly_n_digits::<u32, 4>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_year(value as _)))
            .ok_or(InvalidComponent("year"))?;
        let input = dash(input).ok_or(InvalidLiteral)?.into_inner();
        let input = exactly_n_digits::<_, 2>(input)
            .and_then(|item| item.flat_map(|value| Month::from_number(value).ok()))
            .and_then(|item| item.consume_value(|value| parsed.set_month(value)))
            .ok_or(InvalidComponent("month"))?;
        let input = dash(input).ok_or(InvalidLiteral)?.into_inner();
        let input = exactly_n_digits::<_, 2>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_day(value)))
            .ok_or(InvalidComponent("day"))?;
        let input = ascii_char_ignore_case::<b'T'>(input)
            .ok_or(InvalidLiteral)?
            .into_inner();
        let input = exactly_n_digits::<_, 2>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_hour_24(value)))
            .ok_or(InvalidComponent("hour"))?;
        let input = colon(input).ok_or(InvalidLiteral)?.into_inner();
        let input = exactly_n_digits::<_, 2>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_minute(value)))
            .ok_or(InvalidComponent("minute"))?;
        let input = colon(input).ok_or(InvalidLiteral)?.into_inner();
        let input = exactly_n_digits::<_, 2>(input)
            .and_then(|item| item.consume_value(|value| parsed.set_second(value)))
            .ok_or(InvalidComponent("second"))?;
        let input = if let Some(ParsedItem(input, ())) = ascii_char::<b'.'>(input) {
            let ParsedItem(mut input, mut value) = any_digit(input)
                .ok_or(InvalidComponent("subsecond"))?
                .map(|v| (v - b'0') as u32 * 100_000_000);

            let mut multiplier = 10_000_000;
            while let Some(ParsedItem(new_input, digit)) = any_digit(input) {
                value += (digit - b'0') as u32 * multiplier;
                input = new_input;
                multiplier /= 10;
            }

            parsed
                .set_subsecond(value)
                .ok_or(InvalidComponent("subsecond"))?;
            input
        } else {
            input
        };

        // The RFC explicitly allows leap seconds.
        parsed.set_leap_second_allowed(true);

        if let Some(ParsedItem(input, ())) = ascii_char_ignore_case::<b'Z'>(input) {
            parsed
                .set_offset_hour(0)
                .ok_or(InvalidComponent("offset hour"))?;
            parsed
                .set_offset_minute_signed(0)
                .ok_or(InvalidComponent("offset minute"))?;
            parsed
                .set_offset_second_signed(0)
                .ok_or(InvalidComponent("offset second"))?;
            return Ok(input);
        }

        let ParsedItem(input, offset_sign) = sign(input).ok_or(InvalidComponent("offset hour"))?;
        let input = exactly_n_digits::<u8, 2>(input)
            .and_then(|item| {
                item.map(|offset_hour| {
                    if offset_sign == b'-' {
                        -(offset_hour as i8)
                    } else {
                        offset_hour as _
                    }
                })
                .consume_value(|value| parsed.set_offset_hour(value))
            })
            .ok_or(InvalidComponent("offset hour"))?;
        let input = colon(input).ok_or(InvalidLiteral)?.into_inner();
        let input = exactly_n_digits::<u8, 2>(input)
            .and_then(|item| {
                item.consume_value(|value| parsed.set_offset_minute_signed(value as _))
            })
            .ok_or(InvalidComponent("offset minute"))?;

        Ok(input)
    }

    fn parse_offset_date_time(&self, input: &[u8]) -> Result<OffsetDateTime, error::Parse> {
        use crate::error::ParseFromDescription::{InvalidComponent, InvalidLiteral};
        use crate::parsing::combinator::{
            any_digit, ascii_char, ascii_char_ignore_case, exactly_n_digits, sign,
        };

        let dash = ascii_char::<b'-'>;
        let colon = ascii_char::<b':'>;

        let ParsedItem(input, year) =
            exactly_n_digits::<u32, 4>(input).ok_or(InvalidComponent("year"))?;
        let input = dash(input).ok_or(InvalidLiteral)?.into_inner();
        let ParsedItem(input, month) =
            exactly_n_digits::<_, 2>(input).ok_or(InvalidComponent("month"))?;
        let input = dash(input).ok_or(InvalidLiteral)?.into_inner();
        let ParsedItem(input, day) =
            exactly_n_digits::<_, 2>(input).ok_or(InvalidComponent("day"))?;
        let input = ascii_char_ignore_case::<b'T'>(input)
            .ok_or(InvalidLiteral)?
            .into_inner();
        let ParsedItem(input, hour) =
            exactly_n_digits::<_, 2>(input).ok_or(InvalidComponent("hour"))?;
        let input = colon(input).ok_or(InvalidLiteral)?.into_inner();
        let ParsedItem(input, minute) =
            exactly_n_digits::<_, 2>(input).ok_or(InvalidComponent("minute"))?;
        let input = colon(input).ok_or(InvalidLiteral)?.into_inner();
        let ParsedItem(input, mut second) =
            exactly_n_digits::<_, 2>(input).ok_or(InvalidComponent("second"))?;
        let ParsedItem(input, mut nanosecond) =
            if let Some(ParsedItem(input, ())) = ascii_char::<b'.'>(input) {
                let ParsedItem(mut input, mut value) = any_digit(input)
                    .ok_or(InvalidComponent("subsecond"))?
                    .map(|v| (v - b'0') as u32 * 100_000_000);

                let mut multiplier = 10_000_000;
                while let Some(ParsedItem(new_input, digit)) = any_digit(input) {
                    value += (digit - b'0') as u32 * multiplier;
                    input = new_input;
                    multiplier /= 10;
                }

                ParsedItem(input, value)
            } else {
                ParsedItem(input, 0)
            };
        let ParsedItem(input, offset) = {
            if let Some(ParsedItem(input, ())) = ascii_char_ignore_case::<b'Z'>(input) {
                ParsedItem(input, UtcOffset::UTC)
            } else {
                let ParsedItem(input, offset_sign) =
                    sign(input).ok_or(InvalidComponent("offset hour"))?;
                let ParsedItem(input, offset_hour) =
                    exactly_n_digits::<u8, 2>(input).ok_or(InvalidComponent("offset hour"))?;
                let input = colon(input).ok_or(InvalidLiteral)?.into_inner();
                let ParsedItem(input, offset_minute) =
                    exactly_n_digits::<u8, 2>(input).ok_or(InvalidComponent("offset minute"))?;
                UtcOffset::from_hms(
                    if offset_sign == b'-' {
                        -(offset_hour as i8)
                    } else {
                        offset_hour as _
                    },
                    offset_minute as _,
                    0,
                )
                .map(|offset| ParsedItem(input, offset))
                .map_err(|mut err| {
                    // Provide the user a more accurate error.
                    if err.name == "hours" {
                        err.name = "offset hour";
                    } else if err.name == "minutes" {
                        err.name = "offset minute";
                    }
                    err
                })
                .map_err(TryFromParsed::ComponentRange)?
            }
        };

        if !input.is_empty() {
            return Err(error::Parse::UnexpectedTrailingCharacters);
        }

        // The RFC explicitly permits leap seconds. We don't currently support them, so treat it as
        // the preceding nanosecond. However, leap seconds can only occur as the last second of the
        // month UTC.
        let leap_second_input = if second == 60 {
            second = 59;
            nanosecond = 999_999_999;
            true
        } else {
            false
        };

        let dt = Month::from_number(month)
            .and_then(|month| Date::from_calendar_date(year as _, month, day))
            .and_then(|date| date.with_hms_nano(hour, minute, second, nanosecond))
            .map(|date| date.assume_offset(offset))
            .map_err(TryFromParsed::ComponentRange)?;

        if leap_second_input && !dt.is_valid_leap_second_stand_in() {
            return Err(error::Parse::TryFromParsed(TryFromParsed::ComponentRange(
                error::ComponentRange {
                    name: "second",
                    minimum: 0,
                    maximum: 59,
                    value: 60,
                    conditional_range: true,
                },
            )));
        }

        Ok(dt)
    }
}
// endregion well-known formats
