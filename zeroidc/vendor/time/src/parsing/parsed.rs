//! Information parsed from an input and format description.

use core::convert::{TryFrom, TryInto};
use core::num::{NonZeroU16, NonZeroU8};

use crate::error::TryFromParsed::InsufficientInformation;
use crate::format_description::modifier::{WeekNumberRepr, YearRepr};
use crate::format_description::{Component, FormatItem};
use crate::parsing::component::{
    parse_day, parse_hour, parse_minute, parse_month, parse_offset_hour, parse_offset_minute,
    parse_offset_second, parse_ordinal, parse_period, parse_second, parse_subsecond,
    parse_week_number, parse_weekday, parse_year, Period,
};
use crate::parsing::ParsedItem;
use crate::{error, Date, Month, OffsetDateTime, PrimitiveDateTime, Time, UtcOffset, Weekday};

/// All information parsed.
///
/// This information is directly used to construct the final values.
///
/// Most users will not need think about this struct in any way. It is public to allow for manual
/// control over values, in the instance that the default parser is insufficient.
#[derive(Debug, Clone, Copy)]
pub struct Parsed {
    /// Calendar year.
    year: Option<i32>,
    /// The last two digits of the calendar year.
    year_last_two: Option<u8>,
    /// Year of the [ISO week date](https://en.wikipedia.org/wiki/ISO_week_date).
    iso_year: Option<i32>,
    /// The last two digits of the ISO week year.
    iso_year_last_two: Option<u8>,
    /// Month of the year.
    month: Option<Month>,
    /// Week of the year, where week one begins on the first Sunday of the calendar year.
    sunday_week_number: Option<u8>,
    /// Week of the year, where week one begins on the first Monday of the calendar year.
    monday_week_number: Option<u8>,
    /// Week of the year, where week one is the Monday-to-Sunday period containing January 4.
    iso_week_number: Option<NonZeroU8>,
    /// Day of the week.
    weekday: Option<Weekday>,
    /// Day of the year.
    ordinal: Option<NonZeroU16>,
    /// Day of the month.
    day: Option<NonZeroU8>,
    /// Hour within the day.
    hour_24: Option<u8>,
    /// Hour within the 12-hour period (midnight to noon or vice versa). This is typically used in
    /// conjunction with AM/PM, which is indicated by the `hour_12_is_pm` field.
    hour_12: Option<NonZeroU8>,
    /// Whether the `hour_12` field indicates a time that "PM".
    hour_12_is_pm: Option<bool>,
    /// Minute within the hour.
    minute: Option<u8>,
    /// Second within the minute.
    second: Option<u8>,
    /// Nanosecond within the second.
    subsecond: Option<u32>,
    /// Whole hours of the UTC offset.
    offset_hour: Option<i8>,
    /// Minutes within the hour of the UTC offset.
    offset_minute: Option<i8>,
    /// Seconds within the minute of the UTC offset.
    offset_second: Option<i8>,
    /// Indicates whether a leap second is permitted to be parsed. This is required by some
    /// well-known formats.
    leap_second_allowed: bool,
}

impl Parsed {
    /// Create a new instance of `Parsed` with no information known.
    pub const fn new() -> Self {
        Self {
            year: None,
            year_last_two: None,
            iso_year: None,
            iso_year_last_two: None,
            month: None,
            sunday_week_number: None,
            monday_week_number: None,
            iso_week_number: None,
            weekday: None,
            ordinal: None,
            day: None,
            hour_24: None,
            hour_12: None,
            hour_12_is_pm: None,
            minute: None,
            second: None,
            subsecond: None,
            offset_hour: None,
            offset_minute: None,
            offset_second: None,
            leap_second_allowed: false,
        }
    }

    /// Parse a single [`FormatItem`], mutating the struct. The remaining input is returned as the
    /// `Ok` value.
    ///
    /// If a [`FormatItem::Optional`] is passed, parsing will not fail; the input will be returned
    /// as-is if the expected format is not present.
    pub fn parse_item<'a>(
        &mut self,
        input: &'a [u8],
        item: &FormatItem<'_>,
    ) -> Result<&'a [u8], error::ParseFromDescription> {
        match item {
            FormatItem::Literal(literal) => Self::parse_literal(input, literal),
            FormatItem::Component(component) => self.parse_component(input, *component),
            FormatItem::Compound(compound) => self.parse_items(input, compound),
            FormatItem::Optional(item) => self.parse_item(input, item).or(Ok(input)),
            FormatItem::First(items) => {
                let mut first_err = None;

                for item in items.iter() {
                    match self.parse_item(input, item) {
                        Ok(remaining_input) => return Ok(remaining_input),
                        Err(err) if first_err.is_none() => first_err = Some(err),
                        Err(_) => {}
                    }
                }

                match first_err {
                    Some(err) => Err(err),
                    // This location will be reached if the slice is empty, skipping the `for` loop.
                    // As this case is expected to be uncommon, there's no need to check up front.
                    None => Ok(input),
                }
            }
        }
    }

    /// Parse a sequence of [`FormatItem`]s, mutating the struct. The remaining input is returned as
    /// the `Ok` value.
    ///
    /// This method will fail if any of the contained [`FormatItem`]s fail to parse. `self` will not
    /// be mutated in this instance.
    pub fn parse_items<'a>(
        &mut self,
        mut input: &'a [u8],
        items: &[FormatItem<'_>],
    ) -> Result<&'a [u8], error::ParseFromDescription> {
        // Make a copy that we can mutate. It will only be set to the user's copy if everything
        // succeeds.
        let mut this = *self;
        for item in items {
            input = this.parse_item(input, item)?;
        }
        *self = this;
        Ok(input)
    }

    /// Parse a literal byte sequence. The remaining input is returned as the `Ok` value.
    pub fn parse_literal<'a>(
        input: &'a [u8],
        literal: &[u8],
    ) -> Result<&'a [u8], error::ParseFromDescription> {
        input
            .strip_prefix(literal)
            .ok_or(error::ParseFromDescription::InvalidLiteral)
    }

    /// Parse a single component, mutating the struct. The remaining input is returned as the `Ok`
    /// value.
    pub fn parse_component<'a>(
        &mut self,
        input: &'a [u8],
        component: Component,
    ) -> Result<&'a [u8], error::ParseFromDescription> {
        use error::ParseFromDescription::InvalidComponent;

        match component {
            Component::Day(modifiers) => parse_day(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_day(value)))
                .ok_or(InvalidComponent("day")),
            Component::Month(modifiers) => parse_month(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_month(value)))
                .ok_or(InvalidComponent("month")),
            Component::Ordinal(modifiers) => parse_ordinal(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_ordinal(value)))
                .ok_or(InvalidComponent("ordinal")),
            Component::Weekday(modifiers) => parse_weekday(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_weekday(value)))
                .ok_or(InvalidComponent("weekday")),
            Component::WeekNumber(modifiers) => {
                let ParsedItem(remaining, value) =
                    parse_week_number(input, modifiers).ok_or(InvalidComponent("week number"))?;
                match modifiers.repr {
                    WeekNumberRepr::Iso => {
                        NonZeroU8::new(value).and_then(|value| self.set_iso_week_number(value))
                    }
                    WeekNumberRepr::Sunday => self.set_sunday_week_number(value),
                    WeekNumberRepr::Monday => self.set_monday_week_number(value),
                }
                .ok_or(InvalidComponent("week number"))?;
                Ok(remaining)
            }
            Component::Year(modifiers) => {
                let ParsedItem(remaining, value) =
                    parse_year(input, modifiers).ok_or(InvalidComponent("year"))?;
                match (modifiers.iso_week_based, modifiers.repr) {
                    (false, YearRepr::Full) => self.set_year(value),
                    (false, YearRepr::LastTwo) => self.set_year_last_two(value as _),
                    (true, YearRepr::Full) => self.set_iso_year(value),
                    (true, YearRepr::LastTwo) => self.set_iso_year_last_two(value as _),
                }
                .ok_or(InvalidComponent("year"))?;
                Ok(remaining)
            }
            Component::Hour(modifiers) => {
                let ParsedItem(remaining, value) =
                    parse_hour(input, modifiers).ok_or(InvalidComponent("hour"))?;
                if modifiers.is_12_hour_clock {
                    NonZeroU8::new(value).and_then(|value| self.set_hour_12(value))
                } else {
                    self.set_hour_24(value)
                }
                .ok_or(InvalidComponent("hour"))?;
                Ok(remaining)
            }
            Component::Minute(modifiers) => parse_minute(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_minute(value)))
                .ok_or(InvalidComponent("minute")),
            Component::Period(modifiers) => parse_period(input, modifiers)
                .and_then(|parsed| {
                    parsed.consume_value(|value| self.set_hour_12_is_pm(value == Period::Pm))
                })
                .ok_or(InvalidComponent("period")),
            Component::Second(modifiers) => parse_second(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_second(value)))
                .ok_or(InvalidComponent("second")),
            Component::Subsecond(modifiers) => parse_subsecond(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_subsecond(value)))
                .ok_or(InvalidComponent("subsecond")),
            Component::OffsetHour(modifiers) => parse_offset_hour(input, modifiers)
                .and_then(|parsed| parsed.consume_value(|value| self.set_offset_hour(value)))
                .ok_or(InvalidComponent("offset hour")),
            Component::OffsetMinute(modifiers) => parse_offset_minute(input, modifiers)
                .and_then(|parsed| {
                    parsed.consume_value(|value| self.set_offset_minute_signed(value))
                })
                .ok_or(InvalidComponent("offset minute")),
            Component::OffsetSecond(modifiers) => parse_offset_second(input, modifiers)
                .and_then(|parsed| {
                    parsed.consume_value(|value| self.set_offset_second_signed(value))
                })
                .ok_or(InvalidComponent("offset second")),
        }
    }
}

/// Generate getters for each of the fields.
macro_rules! getters {
    ($($name:ident: $ty:ty),+ $(,)?) => {$(
        /// Obtain the named component.
        pub const fn $name(&self) -> Option<$ty> {
            self.$name
        }
    )*}
}

/// Getter methods
impl Parsed {
    getters! {
        year: i32,
        year_last_two: u8,
        iso_year: i32,
        iso_year_last_two: u8,
        month: Month,
        sunday_week_number: u8,
        monday_week_number: u8,
        iso_week_number: NonZeroU8,
        weekday: Weekday,
        ordinal: NonZeroU16,
        day: NonZeroU8,
        hour_24: u8,
        hour_12: NonZeroU8,
        hour_12_is_pm: bool,
        minute: u8,
        second: u8,
        subsecond: u32,
        offset_hour: i8,
    }

    /// Obtain the absolute value of the offset minute.
    #[deprecated(since = "0.3.8", note = "use `parsed.offset_minute_signed()` instead")]
    pub const fn offset_minute(&self) -> Option<u8> {
        Some(const_try_opt!(self.offset_minute_signed()).unsigned_abs())
    }

    /// Obtain the offset minute as an `i8`.
    pub const fn offset_minute_signed(&self) -> Option<i8> {
        self.offset_minute
    }

    /// Obtain the absolute value of the offset second.
    #[deprecated(since = "0.3.8", note = "use `parsed.offset_second_signed()` instead")]
    pub const fn offset_second(&self) -> Option<u8> {
        Some(const_try_opt!(self.offset_second_signed()).unsigned_abs())
    }

    /// Obtain the offset second as an `i8`.
    pub const fn offset_second_signed(&self) -> Option<i8> {
        self.offset_second
    }

    /// Obtain whether leap seconds are permitted in the current format.
    pub(crate) const fn leap_second_allowed(&self) -> bool {
        self.leap_second_allowed
    }
}

/// Generate setters for each of the fields.
///
/// This macro should only be used for fields where the value is not validated beyond its type.
macro_rules! setters {
    ($($setter_name:ident $name:ident: $ty:ty),+ $(,)?) => {$(
        /// Set the named component.
        pub fn $setter_name(&mut self, value: $ty) -> Option<()> {
            self.$name = Some(value);
            Some(())
        }
    )*}
}

/// Setter methods
///
/// All setters return `Option<()>`, which is `Some` if the value was set, and `None` if not. The
/// setters _may_ fail if the value is invalid, though behavior is not guaranteed.
impl Parsed {
    setters! {
        set_year year: i32,
        set_year_last_two year_last_two: u8,
        set_iso_year iso_year: i32,
        set_iso_year_last_two iso_year_last_two: u8,
        set_month month: Month,
        set_sunday_week_number sunday_week_number: u8,
        set_monday_week_number monday_week_number: u8,
        set_iso_week_number iso_week_number: NonZeroU8,
        set_weekday weekday: Weekday,
        set_ordinal ordinal: NonZeroU16,
        set_day day: NonZeroU8,
        set_hour_24 hour_24: u8,
        set_hour_12 hour_12: NonZeroU8,
        set_hour_12_is_pm hour_12_is_pm: bool,
        set_minute minute: u8,
        set_second second: u8,
        set_subsecond subsecond: u32,
        set_offset_hour offset_hour: i8,
    }

    /// Set the named component.
    #[deprecated(
        since = "0.3.8",
        note = "use `parsed.set_offset_minute_signed()` instead"
    )]
    pub fn set_offset_minute(&mut self, value: u8) -> Option<()> {
        if value > i8::MAX as u8 {
            None
        } else {
            self.set_offset_minute_signed(value as _)
        }
    }

    /// Set the `offset_minute` component.
    pub fn set_offset_minute_signed(&mut self, value: i8) -> Option<()> {
        self.offset_minute = Some(value);
        Some(())
    }

    /// Set the named component.
    #[deprecated(
        since = "0.3.8",
        note = "use `parsed.set_offset_second_signed()` instead"
    )]
    pub fn set_offset_second(&mut self, value: u8) -> Option<()> {
        if value > i8::MAX as u8 {
            None
        } else {
            self.set_offset_second_signed(value as _)
        }
    }

    /// Set the `offset_second` component.
    pub fn set_offset_second_signed(&mut self, value: i8) -> Option<()> {
        self.offset_second = Some(value);
        Some(())
    }

    /// Set the leap second allowed flag.
    pub(crate) fn set_leap_second_allowed(&mut self, value: bool) {
        self.leap_second_allowed = value;
    }
}

/// Generate build methods for each of the fields.
///
/// This macro should only be used for fields where the value is not validated beyond its type.
macro_rules! builders {
    ($($builder_name:ident $name:ident: $ty:ty),+ $(,)?) => {$(
        /// Set the named component and return `self`.
        pub const fn $builder_name(mut self, value: $ty) -> Option<Self> {
            self.$name = Some(value);
            Some(self)
        }
    )*}
}

/// Builder methods
///
/// All builder methods return `Option<Self>`, which is `Some` if the value was set, and `None` if
/// not. The builder methods _may_ fail if the value is invalid, though behavior is not guaranteed.
impl Parsed {
    builders! {
        with_year year: i32,
        with_year_last_two year_last_two: u8,
        with_iso_year iso_year: i32,
        with_iso_year_last_two iso_year_last_two: u8,
        with_month month: Month,
        with_sunday_week_number sunday_week_number: u8,
        with_monday_week_number monday_week_number: u8,
        with_iso_week_number iso_week_number: NonZeroU8,
        with_weekday weekday: Weekday,
        with_ordinal ordinal: NonZeroU16,
        with_day day: NonZeroU8,
        with_hour_24 hour_24: u8,
        with_hour_12 hour_12: NonZeroU8,
        with_hour_12_is_pm hour_12_is_pm: bool,
        with_minute minute: u8,
        with_second second: u8,
        with_subsecond subsecond: u32,
        with_offset_hour offset_hour: i8,
    }

    /// Set the named component and return `self`.
    #[deprecated(
        since = "0.3.8",
        note = "use `parsed.with_offset_minute_signed()` instead"
    )]
    pub const fn with_offset_minute(self, value: u8) -> Option<Self> {
        if value > i8::MAX as u8 {
            None
        } else {
            self.with_offset_minute_signed(value as _)
        }
    }

    /// Set the `offset_minute` component and return `self`.
    pub const fn with_offset_minute_signed(mut self, value: i8) -> Option<Self> {
        self.offset_minute = Some(value);
        Some(self)
    }

    /// Set the named component and return `self`.
    #[deprecated(
        since = "0.3.8",
        note = "use `parsed.with_offset_second_signed()` instead"
    )]
    pub const fn with_offset_second(self, value: u8) -> Option<Self> {
        if value > i8::MAX as u8 {
            None
        } else {
            self.with_offset_second_signed(value as _)
        }
    }

    /// Set the `offset_second` component and return `self`.
    pub const fn with_offset_second_signed(mut self, value: i8) -> Option<Self> {
        self.offset_second = Some(value);
        Some(self)
    }
}

impl TryFrom<Parsed> for Date {
    type Error = error::TryFromParsed;

    fn try_from(parsed: Parsed) -> Result<Self, Self::Error> {
        /// Match on the components that need to be present.
        macro_rules! match_ {
            (_ => $catch_all:expr $(,)?) => {
                $catch_all
            };
            (($($name:ident),* $(,)?) => $arm:expr, $($rest:tt)*) => {
                if let ($(Some($name)),*) = ($(parsed.$name()),*) {
                    $arm
                } else {
                    match_!($($rest)*)
                }
            };
        }

        /// Get the value needed to adjust the ordinal day for Sunday and Monday-based week
        /// numbering.
        const fn adjustment(year: i32) -> i16 {
            match Date::__from_ordinal_date_unchecked(year, 1).weekday() {
                Weekday::Monday => 7,
                Weekday::Tuesday => 1,
                Weekday::Wednesday => 2,
                Weekday::Thursday => 3,
                Weekday::Friday => 4,
                Weekday::Saturday => 5,
                Weekday::Sunday => 6,
            }
        }

        // TODO Only the basics have been covered. There are many other valid values that are not
        // currently constructed from the information known.

        match_! {
            (year, ordinal) => Ok(Self::from_ordinal_date(year, ordinal.get())?),
            (year, month, day) => Ok(Self::from_calendar_date(year, month, day.get())?),
            (iso_year, iso_week_number, weekday) => Ok(Self::from_iso_week_date(
                iso_year,
                iso_week_number.get(),
                weekday,
            )?),
            (year, sunday_week_number, weekday) => Ok(Self::from_ordinal_date(
                year,
                (sunday_week_number as i16 * 7 + weekday.number_days_from_sunday() as i16
                    - adjustment(year)
                    + 1) as u16,
            )?),
            (year, monday_week_number, weekday) => Ok(Self::from_ordinal_date(
                year,
                (monday_week_number as i16 * 7 + weekday.number_days_from_monday() as i16
                    - adjustment(year)
                    + 1) as u16,
            )?),
            _ => Err(InsufficientInformation),
        }
    }
}

impl TryFrom<Parsed> for Time {
    type Error = error::TryFromParsed;

    fn try_from(parsed: Parsed) -> Result<Self, Self::Error> {
        let hour = match (parsed.hour_24(), parsed.hour_12(), parsed.hour_12_is_pm()) {
            (Some(hour), _, _) => hour,
            (_, Some(hour), Some(false)) if hour.get() == 12 => 0,
            (_, Some(hour), Some(true)) if hour.get() == 12 => 12,
            (_, Some(hour), Some(false)) => hour.get(),
            (_, Some(hour), Some(true)) => hour.get() + 12,
            _ => return Err(InsufficientInformation),
        };
        if parsed.hour_24().is_none()
            && parsed.hour_12().is_some()
            && parsed.hour_12_is_pm().is_some()
            && parsed.minute().is_none()
            && parsed.second().is_none()
            && parsed.subsecond().is_none()
        {
            return Ok(Self::from_hms_nano(hour, 0, 0, 0)?);
        }
        let minute = parsed.minute().ok_or(InsufficientInformation)?;
        let second = parsed.second().unwrap_or(0);
        let subsecond = parsed.subsecond().unwrap_or(0);
        Ok(Self::from_hms_nano(hour, minute, second, subsecond)?)
    }
}

impl TryFrom<Parsed> for UtcOffset {
    type Error = error::TryFromParsed;

    fn try_from(parsed: Parsed) -> Result<Self, Self::Error> {
        let hour = parsed.offset_hour().ok_or(InsufficientInformation)?;
        let minute = parsed.offset_minute_signed().unwrap_or(0);
        let second = parsed.offset_second_signed().unwrap_or(0);

        Self::from_hms(hour, minute, second).map_err(|mut err| {
            // Provide the user a more accurate error.
            if err.name == "hours" {
                err.name = "offset hour";
            } else if err.name == "minutes" {
                err.name = "offset minute";
            } else if err.name == "seconds" {
                err.name = "offset second";
            }
            err.into()
        })
    }
}

impl TryFrom<Parsed> for PrimitiveDateTime {
    type Error = error::TryFromParsed;

    fn try_from(parsed: Parsed) -> Result<Self, Self::Error> {
        Ok(Self::new(parsed.try_into()?, parsed.try_into()?))
    }
}

impl TryFrom<Parsed> for OffsetDateTime {
    type Error = error::TryFromParsed;

    #[allow(clippy::unwrap_in_result)] // We know the values are valid.
    fn try_from(mut parsed: Parsed) -> Result<Self, Self::Error> {
        // Some well-known formats explicitly allow leap seconds. We don't currently support them,
        // so treat it as the nearest preceding moment that can be represented. Because leap seconds
        // always fall at the end of a month UTC, reject any that are at other times.
        let leap_second_input = if parsed.leap_second_allowed() && parsed.second() == Some(60) {
            parsed.set_second(59).expect("59 is a valid second");
            parsed
                .set_subsecond(999_999_999)
                .expect("999_999_999 is a valid subsecond");
            true
        } else {
            false
        };
        let dt = PrimitiveDateTime::try_from(parsed)?.assume_offset(parsed.try_into()?);
        if leap_second_input && !dt.is_valid_leap_second_stand_in() {
            return Err(error::TryFromParsed::ComponentRange(
                error::ComponentRange {
                    name: "second",
                    minimum: 0,
                    maximum: 59,
                    value: 60,
                    conditional_range: true,
                },
            ));
        }
        Ok(dt)
    }
}
