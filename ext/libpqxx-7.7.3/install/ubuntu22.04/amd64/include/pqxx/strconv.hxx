/* String conversion definitions.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/stringconv instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_STRCONV
#define PQXX_H_STRCONV

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

#if __has_include(<charconv>)
#  include <charconv>
#endif

#if defined(PQXX_HAVE_RANGES) && __has_include(<ranges>)
#  include <ranges>
#endif

#include "pqxx/except.hxx"
#include "pqxx/util.hxx"
#include "pqxx/zview.hxx"


namespace pqxx::internal
{
/// Attempt to demangle @c std::type_info::name() to something human-readable.
PQXX_LIBEXPORT std::string demangle_type_name(char const[]);
} // namespace pqxx::internal


namespace pqxx
{
/**
 * @defgroup stringconversion String conversion
 *
 * The PostgreSQL server accepts and represents data in string form.  It has
 * its own formats for various data types.  The string conversions define how
 * various C++ types translate to and from their respective PostgreSQL text
 * representations.
 *
 * Each conversion is defined by a specialisations of @c string_traits.  It
 * gets complicated if you want top performance, but until you do, all you
 * really need to care about when converting values between C++ in-memory
 * representations such as @c int and the postgres string representations is
 * the @c pqxx::to_string and @c pqxx::from_string functions.
 *
 * If you need to convert a type which is not supported out of the box, you'll
 * need to define your own specialisations for these templates, similar to the
 * ones defined here and in `pqxx/conversions.hxx`.  Any conversion code which
 * "sees" your specialisation will now support your conversion.  In particular,
 * you'll be able to read result fields into a variable of the new type.
 *
 * There is a macro to help you define conversions for individual enumeration
 * types.  The conversion will represent enumeration values as numeric strings.
 */
//@{

/// A human-readable name for a type, used in error messages and such.
/** Actually this may not always be very user-friendly.  It uses
 * @c std::type_info::name().  On gcc-like compilers we try to demangle its
 * output.  Visual Studio produces human-friendly names out of the box.
 *
 * This variable is not inline.  Inlining it gives rise to "memory leak"
 * warnings from asan, the address sanitizer, possibly from use of
 * @c std::type_info::name.
 */
template<typename TYPE>
std::string const type_name{internal::demangle_type_name(typeid(TYPE).name())};


/// Traits describing a type's "null value," if any.
/** Some C++ types have a special value or state which correspond directly to
 * SQL's NULL.
 *
 * The @c nullness traits describe whether it exists, and whether a particular
 * value is null.
 */
template<typename TYPE, typename ENABLE = void> struct nullness
{
  /// Does this type have a null value?
  static bool has_null;

  /// Is this type always null?
  static bool always_null;

  /// Is @c value a null?
  static bool is_null(TYPE const &value);

  /// Return a null value.
  /** Don't use this in generic code to compare a value and see whether it is
   * null.  Some types may have multiple null values which do not compare as
   * equal, or may define a null value which is not equal to anything including
   * itself, like in SQL.
   */
  [[nodiscard]] static TYPE null();
};


/// Nullness traits describing a type which does not have a null value.
template<typename TYPE> struct no_null
{
  /// Does @c TYPE have a "built-in null value"?
  /** For example, a pointer can equal @c nullptr, which makes a very natural
   * representation of an SQL null value.  For such types, the code sometimes
   * needs to make special allowances.
   *
   * for most types, such as @c int or @c std::string, there is no built-in
   * null.  If you want to represent an SQL null value for such a type, you
   * would have to wrap it in something that does have a null value.  For
   * example, you could use @c std::optional<int> for "either an @c int or a
   * null value."
   */
  static constexpr bool has_null = false;

  /// Are all values of this type null?
  /** There are a few special C++ types which are always null - mainly
   * @c std::nullptr_t.
   */
  static constexpr bool always_null = false;

  /// Does a given value correspond to an SQL null value?
  /** Most C++ types, such as @c int or @c std::string, have no inherent null
   * value.  But some types such as C-style string pointers do have a natural
   * equivalent to an SQL null.
   */
  [[nodiscard]] static constexpr bool is_null(TYPE const &) noexcept
  {
    return false;
  }
};


/// Traits class for use in string conversions.
/** Specialize this template for a type for which you wish to add to_string
 * and from_string support.
 *
 * String conversions are not meant to work for nulls.  Check for null before
 * converting a value of @c TYPE to a string, or vice versa.
 */
template<typename TYPE> struct string_traits
{
  /// Return a @c string_view representing value, plus terminating zero.
  /** Produces a @c string_view containing the PostgreSQL string representation
   * for @c value.
   *
   * Uses the space from @c begin to @c end as a buffer, if needed.  The
   * returned string may lie somewhere in that buffer, or it may be a
   * compile-time constant, or it may be null if value was a null value.  Even
   * if the string is stored in the buffer, its @c begin() may or may not be
   * the same as @c begin.
   *
   * The @c string_view is guaranteed to be valid as long as the buffer from
   * @c begin to @c end remains accessible and unmodified.
   *
   * @throws pqxx::conversion_overrun if the provided buffer space may not be
   * enough.  For maximum performance, this is a conservative estimate.  It may
   * complain about a buffer which is actually large enough for your value, if
   * an exact check gets too expensive.
   */
  [[nodiscard]] static inline zview
  to_buf(char *begin, char *end, TYPE const &value);

  /// Write value's string representation into buffer at @c begin.
  /** Assumes that value is non-null.
   *
   * Writes value's string representation into the buffer, starting exactly at
   * @c begin, and ensuring a trailing zero.  Returns the address just beyond
   * the trailing zero, so the caller could use it as the @c begin for another
   * call to @c into_buf writing a next value.
   */
  static inline char *into_buf(char *begin, char *end, TYPE const &value);

  /// Parse a string representation of a @c TYPE value.
  /** Throws @c conversion_error if @c value does not meet the expected format
   * for a value of this type.
   */
  [[nodiscard]] static inline TYPE from_string(std::string_view text);

  // C++20: Can we make these all constexpr?
  /// Estimate how much buffer space is needed to represent value.
  /** The estimate may be a little pessimistic, if it saves time.
   *
   * The estimate includes the terminating zero.
   */
  [[nodiscard]] static inline std::size_t
  size_buffer(TYPE const &value) noexcept;
};


/// Nullness: Enums do not have an inherent null value.
template<typename ENUM>
struct nullness<ENUM, std::enable_if_t<std::is_enum_v<ENUM>>> : no_null<ENUM>
{};
} // namespace pqxx


namespace pqxx::internal
{
/// Helper class for defining enum conversions.
/** The conversion will convert enum values to numeric strings, and vice versa.
 *
 * To define a string conversion for an enum type, derive a @c string_traits
 * specialisation for the enum from this struct.
 *
 * There's usually an easier way though: the @c PQXX_DECLARE_ENUM_CONVERSION
 * macro.  Use @c enum_traits manually only if you need to customise your
 * traits type in more detail.
 */
template<typename ENUM> struct enum_traits
{
  using impl_type = std::underlying_type_t<ENUM>;
  using impl_traits = string_traits<impl_type>;

  [[nodiscard]] static constexpr zview
  to_buf(char *begin, char *end, ENUM const &value)
  {
    return impl_traits::to_buf(begin, end, to_underlying(value));
  }

  static constexpr char *into_buf(char *begin, char *end, ENUM const &value)
  {
    return impl_traits::into_buf(begin, end, to_underlying(value));
  }

  [[nodiscard]] static ENUM from_string(std::string_view text)
  {
    return static_cast<ENUM>(impl_traits::from_string(text));
  }

  [[nodiscard]] static std::size_t size_buffer(ENUM const &value) noexcept
  {
    return impl_traits::size_buffer(to_underlying(value));
  }

private:
  // C++23: Replace with std::to_underlying.
  static constexpr impl_type to_underlying(ENUM const &value) noexcept
  {
    return static_cast<impl_type>(value);
  }
};
} // namespace pqxx::internal


/// Macro: Define a string conversion for an enum type.
/** This specialises the @c pqxx::string_traits template, so use it in the
 * @c ::pqxx namespace.
 *
 * For example:
 *
 *      #include <iostream>
 *      #include <pqxx/strconv>
 *      enum X { xa, xb };
 *      namespace pqxx { PQXX_DECLARE_ENUM_CONVERSION(x); }
 *      int main() { std::cout << pqxx::to_string(xa) << std::endl; }
 */
#define PQXX_DECLARE_ENUM_CONVERSION(ENUM)                                    \
  template<> struct string_traits<ENUM> : pqxx::internal::enum_traits<ENUM>   \
  {};                                                                         \
  template<> inline std::string const type_name<ENUM> { #ENUM }


namespace pqxx
{
/// Parse a value in postgres' text format as a TYPE.
/** If the form of the value found in the string does not match the expected
 * type, e.g. if a decimal point is found when converting to an integer type,
 * the conversion fails.  Overflows (e.g. converting "9999999999" to a 16-bit
 * C++ type) are also treated as errors.  If in some cases this behaviour
 * should be inappropriate, convert to something bigger such as @c long @c int
 * first and then truncate the resulting value.
 *
 * Only the simplest possible conversions are supported.  Fancy features like
 * hexadecimal or octal, spurious signs, or exponent notation won't work.
 * Whitespace is not stripped away.  Only the kinds of strings that come out of
 * PostgreSQL and out of to_string() can be converted.
 */
template<typename TYPE>
[[nodiscard]] inline TYPE from_string(std::string_view text)
{
  return string_traits<TYPE>::from_string(text);
}


/// "Convert" a std::string_view to a std::string_view.
/** Just returns its input.
 *
 * @warning Of course the result is only valid for as long as the original
 * string remains valid!  Never access the string referenced by the return
 * value after the original has been destroyed.
 */
template<>
[[nodiscard]] inline std::string_view from_string(std::string_view text)
{
  return text;
}


/// Attempt to convert postgres-generated string to given built-in object.
/** This is like the single-argument form of the function, except instead of
 * returning the value, it sets @c value.
 *
 * You may find this more convenient in that it infers the type you want from
 * the argument you pass.  But there are disadvantages: it requires an
 * assignment operator, and it may be less efficient.
 */
template<typename T> inline void from_string(std::string_view text, T &value)
{
  value = from_string<T>(text);
}


/// Convert a value to a readable string that PostgreSQL will understand.
/** The conversion does no special formatting, and ignores any locale settings.
 * The resulting string will be human-readable and in a format suitable for use
 * in SQL queries.  It won't have niceties such as "thousands separators"
 * though.
 */
template<typename TYPE> inline std::string to_string(TYPE const &value);


/// Convert multiple values to strings inside a single buffer.
/** There must be enough room for all values, or this will throw
 * @c conversion_overrun.  You can obtain a conservative estimate of the buffer
 * space required by calling @c size_buffer() on the values.
 *
 * The @c std::string_view results may point into the buffer, so don't assume
 * that they will remain valid after you destruct or move the buffer.
 */
template<typename... TYPE>
[[nodiscard]] inline std::vector<std::string_view>
to_buf(char *here, char const *end, TYPE... value)
{
  return {[&here, end](auto v) {
    auto begin = here;
    here = string_traits<decltype(v)>::into_buf(begin, end, v);
    // Exclude the trailing zero out of the string_view.
    auto len{static_cast<std::size_t>(here - begin) - 1};
    return std::string_view{begin, len};
  }(value)...};
}

/// Convert a value to a readable string that PostgreSQL will understand.
/** This variant of to_string can sometimes save a bit of time in loops, by
 * re-using a std::string for multiple conversions.
 */
template<typename TYPE>
inline void into_string(TYPE const &value, std::string &out);


/// Is @c value null?
template<typename TYPE>
[[nodiscard]] inline constexpr bool is_null(TYPE const &value) noexcept
{
  return nullness<strip_t<TYPE>>::is_null(value);
}


/// Estimate how much buffer space is needed to represent values as a string.
/** The estimate may be a little pessimistic, if it saves time.  It also
 * includes room for a terminating zero after each value.
 */
template<typename... TYPE>
[[nodiscard]] inline std::size_t size_buffer(TYPE const &...value) noexcept
{
  return (string_traits<strip_t<TYPE>>::size_buffer(value) + ...);
}


/// Does this type translate to an SQL array?
/** Specialisations may override this to be true for container types.
 *
 * This may not always be a black-and-white choice.  For instance, a
 * @c std::string is a container, but normally it translates to an SQL string,
 * not an SQL array.
 */
template<typename TYPE> inline constexpr bool is_sql_array{false};


/// Can we use this type in arrays and composite types without quoting them?
/** Define this as @c true only if values of @c TYPE can never contain any
 * special characters that might need escaping or confuse the parsing of array
 * or composite * types, such as commas, quotes, parentheses, braces, newlines,
 * and so on.
 *
 * When converting a value of such a type to a string in an array or a field in
 * a composite type, we do not need to add quotes, nor escape any special
 * characters.
 *
 * This is just an optimisation, so it defaults to @c false to err on the side
 * of slow correctness.
 */
template<typename TYPE> inline constexpr bool is_unquoted_safe{false};


/// Element separator between SQL array elements of this type.
template<typename T> inline constexpr char array_separator{','};


/// What's the preferred format for passing non-null parameters of this type?
/** This affects how we pass parameters of @c TYPE when calling parameterised
 * statements or prepared statements.
 *
 * Generally we pass parameters in text format, but binary strings are the
 * exception.  We also pass nulls in binary format, so this function need not
 * handle null values.
 */
template<typename TYPE> inline constexpr format param_format(TYPE const &)
{
  return format::text;
}


/// Implement @c string_traits<TYPE>::to_buf by calling @c into_buf.
/** When you specialise @c string_traits for a new type, most of the time its
 * @c to_buf implementation has no special optimisation tricks and just writes
 * its text into the buffer it receives from the caller, starting at the
 * beginning.
 *
 * In that common situation, you can implement @c to_buf as just a call to
 * @c generic_to_buf.  It will call @c into_buf and return the right result for
 * @c to_buf.
 */
template<typename TYPE>
inline zview generic_to_buf(char *begin, char *end, TYPE const &value)
{
  using traits = string_traits<TYPE>;
  // The trailing zero does not count towards the zview's size, so subtract 1
  // from the result we get from into_buf().
  if (is_null(value))
    return {};
  else
    return {begin, traits::into_buf(begin, end, value) - begin - 1};
}


#if defined(PQXX_HAVE_CONCEPTS)
/// Concept: Binary string, akin to @c std::string for binary data.
/** Any type that satisfies this concept can represent an SQL BYTEA value.
 *
 * A @c binary has a @c begin(), @c end(), @c size(), and @data().  Each byte
 * is a @c std::byte, and they must all be laid out contiguously in memory so
 * we can reference them by a pointer.
 */
template<class TYPE>
concept binary = std::ranges::contiguous_range<TYPE> and
  std::is_same_v<strip_t<value_type<TYPE>>, std::byte>;
#endif
//@}
} // namespace pqxx


#include "pqxx/internal/conversions.hxx"
#endif
