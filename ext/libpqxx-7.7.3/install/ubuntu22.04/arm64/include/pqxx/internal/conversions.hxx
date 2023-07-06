#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <numeric>
#include <optional>

#if defined(PQXX_HAVE_SPAN) && __has_include(<span>)
#  include <span>
#endif

#include <type_traits>
#include <variant>
#include <vector>

#include "pqxx/types.hxx"
#include "pqxx/util.hxx"


/* Internal helpers for string conversion, and conversion implementations.
 *
 * Do not include this header directly.  The libpqxx headers do it for you.
 */
namespace pqxx::internal
{
/// Convert a number in [0, 9] to its ASCII digit.
inline constexpr char number_to_digit(int i) noexcept
{
  return static_cast<char>(i + '0');
}


/// Compute numeric value of given textual digit (assuming that it is a digit).
constexpr int digit_to_number(char c) noexcept
{
  return c - '0';
}


/// Summarize buffer overrun.
/** Don't worry about the exact parameter types: the sizes will be reasonably
 * small, and nonnegative.
 */
std::string PQXX_LIBEXPORT
state_buffer_overrun(int have_bytes, int need_bytes);


template<typename HAVE, typename NEED>
inline std::string state_buffer_overrun(HAVE have_bytes, NEED need_bytes)
{
  return state_buffer_overrun(
    static_cast<int>(have_bytes), static_cast<int>(need_bytes));
}


/// Throw exception for attempt to convert null to given type.
[[noreturn]] PQXX_LIBEXPORT void
throw_null_conversion(std::string const &type);


/// Deliberately nonfunctional conversion traits for `char` types.
/** There are no string conversions for `char` and its signed and unsigned
 * variants.  Such a conversion would be dangerously ambiguous: should we treat
 * it as text, or as a small integer?  It'd be an open invitation for bugs.
 *
 * But the error message when you get this wrong is very cryptic.  So, we
 * derive dummy @ref string_traits implementations from this dummy type, and
 * ensure that the compiler disallows their use.  The compiler error message
 * will at least contain a hint of the root of the problem.
 */
template<typename CHAR_TYPE> struct disallowed_ambiguous_char_conversion
{
  static char *into_buf(char *, char *, CHAR_TYPE) = delete;
  static constexpr zview
  to_buf(char *, char *, CHAR_TYPE const &) noexcept = delete;

  static constexpr std::size_t
  size_buffer(CHAR_TYPE const &) noexcept = delete;
  static CHAR_TYPE from_string(std::string_view) = delete;
};


template<typename T> PQXX_LIBEXPORT extern std::string to_string_float(T);


/// Generic implementation for into_buf, on top of to_buf.
template<typename T>
inline char *generic_into_buf(char *begin, char *end, T const &value)
{
  zview const text{string_traits<T>::to_buf(begin, end, value)};
  auto const space{end - begin};
  // Include the trailing zero.
  auto const len = std::size(text) + 1;
  if (internal::cmp_greater(len, space))
    throw conversion_overrun{
      "Not enough buffer space to insert " + type_name<T> + ".  " +
      state_buffer_overrun(space, len)};
  std::memmove(begin, text.data(), len);
  return begin + len;
}


/// String traits for builtin integral types (though not bool).
template<typename T> struct integral_traits
{
  static PQXX_LIBEXPORT T from_string(std::string_view text);
  static PQXX_LIBEXPORT zview to_buf(char *begin, char *end, T const &value);
  static PQXX_LIBEXPORT char *into_buf(char *begin, char *end, T const &value);

  static constexpr std::size_t size_buffer(T const &) noexcept
  {
    /** Includes a sign if needed; the number of base-10 digits which the type
     * can reliably represent; the one extra base-10 digit which the type can
     * only partially represent; and the terminating zero.
     */
    return std::is_signed_v<T> + std::numeric_limits<T>::digits10 + 1 + 1;
  }
};


/// String traits for builtin floating-point types.
template<typename T> struct float_traits
{
  static PQXX_LIBEXPORT T from_string(std::string_view text);
  static PQXX_LIBEXPORT zview to_buf(char *begin, char *end, T const &value);
  static PQXX_LIBEXPORT char *into_buf(char *begin, char *end, T const &value);

  // Return a nonnegative integral value's number of decimal digits.
  static constexpr std::size_t digits10(std::size_t value) noexcept
  {
    if (value < 10)
      return 1;
    else
      return 1 + digits10(value / 10);
  }

  static constexpr std::size_t size_buffer(T const &) noexcept
  {
    using lims = std::numeric_limits<T>;
    // See #328 for a detailed discussion on the maximum number of digits.
    //
    // In a nutshell: for the big cases, the scientific notation is always
    // the shortest one, and therefore the one that to_chars will pick.
    //
    // So... How long can the scientific notation get?  1 (for sign) + 1 (for
    // decimal point) + 1 (for 'e') + 1 (for exponent sign) + max_digits10 +
    // max number of digits in the exponent + 1 (terminating zero).
    //
    // What's the max number of digits in the exponent?  It's the max number of
    // digits out of the most negative exponent and the most positive one.
    //
    // The longest positive exponent is easy: 1 + ceil(log10(max_exponent10)).
    // (The extra 1 is because 10^n takes up 1 + n digits, not n.)
    //
    // The longest negative exponent is a bit harder: min_exponent10 gives us
    // the smallest power of 10 which a normalised version of T can represent.
    // But the smallest denormalised power of 10 that T can represent is
    // another max_digits10 powers of 10 below that.
    // needs a minus sign.
    //
    // All this stuff messes with my head a bit because it's on the order of
    // log10(log10(n)).  It's easy to get the number of logs wrong.
    auto const max_pos_exp{digits10(lims::max_exponent10)};
    // Really want std::abs(lims::min_exponent10), but MSVC 2017 apparently has
    // problems with std::abs.  So we use -lims::min_exponent10 instead.
    auto const max_neg_exp{
      digits10(lims::max_digits10 - lims::min_exponent10)};
    return 1 +                                    // Sign.
           1 +                                    // Decimal point.
           std::numeric_limits<T>::max_digits10 + // Mantissa digits.
           1 +                                    // Exponent "e".
           1 +                                    // Exponent sign.
           // Spell this weirdly to stop Windows compilers from reading this as
           // a call to their "max" macro when NOMINMAX is not defined.
           (std::max)(max_pos_exp, max_neg_exp) + // Exponent digits.
           1;                                     // Terminating zero.
  }
};
} // namespace pqxx::internal


namespace pqxx
{
/// The built-in arithmetic types do not have inherent null values.
template<typename T>
struct nullness<T, std::enable_if_t<std::is_arithmetic_v<T>>> : no_null<T>
{};


template<> struct string_traits<short> : internal::integral_traits<short>
{};
template<> inline constexpr bool is_unquoted_safe<short>{true};
template<>
struct string_traits<unsigned short>
        : internal::integral_traits<unsigned short>
{};
template<> inline constexpr bool is_unquoted_safe<unsigned short>{true};
template<> struct string_traits<int> : internal::integral_traits<int>
{};
template<> inline constexpr bool is_unquoted_safe<int>{true};
template<> struct string_traits<unsigned> : internal::integral_traits<unsigned>
{};
template<> inline constexpr bool is_unquoted_safe<unsigned>{true};
template<> struct string_traits<long> : internal::integral_traits<long>
{};
template<> inline constexpr bool is_unquoted_safe<long>{true};
template<>
struct string_traits<unsigned long> : internal::integral_traits<unsigned long>
{};
template<> inline constexpr bool is_unquoted_safe<unsigned long>{true};
template<>
struct string_traits<long long> : internal::integral_traits<long long>
{};
template<> inline constexpr bool is_unquoted_safe<long long>{true};
template<>
struct string_traits<unsigned long long>
        : internal::integral_traits<unsigned long long>
{};
template<> inline constexpr bool is_unquoted_safe<unsigned long long>{true};
template<> struct string_traits<float> : internal::float_traits<float>
{};
template<> inline constexpr bool is_unquoted_safe<float>{true};
template<> struct string_traits<double> : internal::float_traits<double>
{};
template<> inline constexpr bool is_unquoted_safe<double>{true};
template<>
struct string_traits<long double> : internal::float_traits<long double>
{};
template<> inline constexpr bool is_unquoted_safe<long double>{true};


template<> struct string_traits<bool>
{
  static PQXX_LIBEXPORT bool from_string(std::string_view text);

  static constexpr zview to_buf(char *, char *, bool const &value) noexcept
  {
    return value ? "true"_zv : "false"_zv;
  }

  static char *into_buf(char *begin, char *end, bool const &value)
  {
    return pqxx::internal::generic_into_buf(begin, end, value);
  }

  static constexpr std::size_t size_buffer(bool const &) noexcept { return 6; }
};


/// We don't support conversion to/from `char` types.
/** Why are these disallowed?  Because they are ambiguous.  It's not inherently
 * clear whether we should treat values of these types as text or as small
 * integers.  Either choice would lead to bugs.
 */
template<>
struct string_traits<char>
        : internal::disallowed_ambiguous_char_conversion<char>
{};

/// We don't support conversion to/from `char` types.
/** Why are these disallowed?  Because they are ambiguous.  It's not inherently
 * clear whether we should treat values of these types as text or as small
 * integers.  Either choice would lead to bugs.
 */
template<>
struct string_traits<signed char>
        : internal::disallowed_ambiguous_char_conversion<signed char>
{};

/// We don't support conversion to/from `char` types.
/** Why are these disallowed?  Because they are ambiguous.  It's not inherently
 * clear whether we should treat values of these types as text or as small
 * integers.  Either choice would lead to bugs.
 */
template<>
struct string_traits<unsigned char>
        : internal::disallowed_ambiguous_char_conversion<unsigned char>
{};


template<> inline constexpr bool is_unquoted_safe<bool>{true};


template<typename T> struct nullness<std::optional<T>>
{
  static constexpr bool has_null = true;
  /// Technically, you could have an optional of an always-null type.
  static constexpr bool always_null = nullness<T>::always_null;
  static constexpr bool is_null(std::optional<T> const &v) noexcept
  {
    return ((not v.has_value()) or pqxx::is_null(*v));
  }
  static constexpr std::optional<T> null() { return {}; }
};


template<typename T>
inline constexpr format param_format(std::optional<T> const &value)
{
  return param_format(*value);
}


template<typename T> struct string_traits<std::optional<T>>
{
  static char *into_buf(char *begin, char *end, std::optional<T> const &value)
  {
    return string_traits<T>::into_buf(begin, end, *value);
  }

  static zview to_buf(char *begin, char *end, std::optional<T> const &value)
  {
    if (value.has_value())
      return string_traits<T>::to_buf(begin, end, *value);
    else
      return {};
  }

  static std::optional<T> from_string(std::string_view text)
  {
    return std::optional<T>{
      std::in_place, string_traits<T>::from_string(text)};
  }

  static std::size_t size_buffer(std::optional<T> const &value) noexcept
  {
    return pqxx::size_buffer(value.value());
  }
};


template<typename T>
inline constexpr bool is_unquoted_safe<std::optional<T>>{is_unquoted_safe<T>};


template<typename... T> struct nullness<std::variant<T...>>
{
  static constexpr bool has_null = (nullness<T>::has_null or ...);
  static constexpr bool always_null = (nullness<T>::always_null and ...);
  static constexpr bool is_null(std::variant<T...> const &value) noexcept
  {
    return std::visit(
      [](auto const &i) noexcept {
        return nullness<strip_t<decltype(i)>>::is_null(i);
      },
      value);
  }

  // We don't support `null()` for `std::variant`.
  /** It would be technically possible to have a `null` in the case where just
   * one of the types has a null, but it gets complicated and arbitrary.
   */
  static constexpr std::variant<T...> null() = delete;
};


template<typename... T> struct string_traits<std::variant<T...>>
{
  static char *
  into_buf(char *begin, char *end, std::variant<T...> const &value)
  {
    return std::visit(
      [begin, end](auto const &i) {
        return string_traits<strip_t<decltype(i)>>::into_buf(begin, end, i);
      },
      value);
  }
  static zview to_buf(char *begin, char *end, std::variant<T...> const &value)
  {
    return std::visit(
      [begin, end](auto const &i) {
        return string_traits<strip_t<decltype(i)>>::to_buf(begin, end, i);
      },
      value);
  }
  static std::size_t size_buffer(std::variant<T...> const &value) noexcept
  {
    return std::visit(
      [](auto const &i) noexcept { return pqxx::size_buffer(i); }, value);
  }

  /** There's no from_string for std::variant.  We could have one with a rule
   * like "pick the first type which fits the value," but we'd have to look
   * into how natural that API feels to users.
   */
  static std::variant<T...> from_string(std::string_view) = delete;
};


template<typename... Args>
inline constexpr format param_format(std::variant<Args...> const &value)
{
  return std::visit([](auto &v) { return param_format(v); }, value);
}


template<typename... T>
inline constexpr bool is_unquoted_safe<std::variant<T...>>{
  (is_unquoted_safe<T> and ...)};


template<typename T> inline T from_string(std::stringstream const &text)
{
  return from_string<T>(text.str());
}


template<> struct string_traits<std::nullptr_t>
{
  static char *into_buf(char *, char *, std::nullptr_t) = delete;

  static constexpr zview
  to_buf(char *, char *, std::nullptr_t const &) noexcept
  {
    return {};
  }

  static constexpr std::size_t size_buffer(std::nullptr_t = nullptr) noexcept
  {
    return 0;
  }
  static std::nullptr_t from_string(std::string_view) = delete;
};


template<> struct string_traits<std::nullopt_t>
{
  static char *into_buf(char *, char *, std::nullopt_t) = delete;

  static constexpr zview
  to_buf(char *, char *, std::nullopt_t const &) noexcept
  {
    return {};
  }

  static constexpr std::size_t size_buffer(std::nullopt_t) noexcept
  {
    return 0;
  }
  static std::nullopt_t from_string(std::string_view) = delete;
};


template<> struct string_traits<std::monostate>
{
  static char *into_buf(char *, char *, std::monostate) = delete;

  static constexpr zview
  to_buf(char *, char *, std::monostate const &) noexcept
  {
    return {};
  }

  static constexpr std::size_t size_buffer(std::monostate) noexcept
  {
    return 0;
  }
  static std::monostate from_string(std::string_view) = delete;
};


template<> inline constexpr bool is_unquoted_safe<std::nullptr_t>{true};


template<> struct nullness<char const *>
{
  static constexpr bool has_null = true;
  static constexpr bool always_null = false;
  static constexpr bool is_null(char const *t) noexcept
  {
    return t == nullptr;
  }
  static constexpr char const *null() noexcept { return nullptr; }
};


/// String traits for C-style string ("pointer to char const").
template<> struct string_traits<char const *>
{
  static char const *from_string(std::string_view text) { return text.data(); }

  static zview to_buf(char *begin, char *end, char const *const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static char *into_buf(char *begin, char *end, char const *const &value)
  {
    auto const space{end - begin};
    // Count the trailing zero, even though std::strlen() and friends don't.
    auto const len{std::strlen(value) + 1};
    if (space < ptrdiff_t(len))
      throw conversion_overrun{
        "Could not copy string: buffer too small.  " +
        pqxx::internal::state_buffer_overrun(space, len)};
    std::memmove(begin, value, len);
    return begin + len;
  }

  static std::size_t size_buffer(char const *const &value) noexcept
  {
    return std::strlen(value) + 1;
  }
};


template<> struct nullness<char *>
{
  static constexpr bool has_null = true;
  static constexpr bool always_null = false;
  static constexpr bool is_null(char const *t) noexcept
  {
    return t == nullptr;
  }
  static constexpr char const *null() { return nullptr; }
};


/// String traits for non-const C-style string ("pointer to char").
template<> struct string_traits<char *>
{
  static char *into_buf(char *begin, char *end, char *const &value)
  {
    return string_traits<char const *>::into_buf(begin, end, value);
  }
  static zview to_buf(char *begin, char *end, char *const &value)
  {
    return string_traits<char const *>::to_buf(begin, end, value);
  }
  static std::size_t size_buffer(char *const &value) noexcept
  {
    return string_traits<char const *>::size_buffer(value);
  }

  /// Don't allow conversion to this type since it breaks const-safety.
  static char *from_string(std::string_view) = delete;
};


template<std::size_t N> struct nullness<char[N]> : no_null<char[N]>
{};


/// String traits for C-style string constant ("array of char").
/** @warning This assumes that every array-of-char is a C-style string literal.
 * So, it must include a trailing zero. and it must have static duration.
 */
template<std::size_t N> struct string_traits<char[N]>
{
  static constexpr zview
  to_buf(char *, char *, char const (&value)[N]) noexcept
  {
    return zview{value, N - 1};
  }

  static char *into_buf(char *begin, char *end, char const (&value)[N])
  {
    if (internal::cmp_less(end - begin, size_buffer(value)))
      throw conversion_overrun{
        "Could not convert char[] to string: too long for buffer."};
    std::memcpy(begin, value, N);
    return begin + N;
  }
  static constexpr std::size_t size_buffer(char const (&)[N]) noexcept
  {
    return N;
  }

  /// Don't allow conversion to this type.
  static void from_string(std::string_view) = delete;
};


template<> struct nullness<std::string> : no_null<std::string>
{};


template<> struct string_traits<std::string>
{
  static std::string from_string(std::string_view text)
  {
    return std::string{text};
  }

  static char *into_buf(char *begin, char *end, std::string const &value)
  {
    if (internal::cmp_greater_equal(std::size(value), end - begin))
      throw conversion_overrun{
        "Could not convert string to string: too long for buffer."};
    // Include the trailing zero.
    value.copy(begin, std::size(value));
    begin[std::size(value)] = '\0';
    return begin + std::size(value) + 1;
  }

  static zview to_buf(char *begin, char *end, std::string const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static std::size_t size_buffer(std::string const &value) noexcept
  {
    return std::size(value) + 1;
  }
};


/// There's no real null for `std::string_view`.
/** I'm not sure how clear-cut this is: a `string_view` may have a null
 * data pointer, which is analogous to a null `char` pointer.
 */
template<> struct nullness<std::string_view> : no_null<std::string_view>
{};


/// String traits for `string_view`.
template<> struct string_traits<std::string_view>
{
  static constexpr std::size_t
  size_buffer(std::string_view const &value) noexcept
  {
    return std::size(value) + 1;
  }

  static char *into_buf(char *begin, char *end, std::string_view const &value)
  {
    if (internal::cmp_greater_equal(std::size(value), end - begin))
      throw conversion_overrun{
        "Could not store string_view: too long for buffer."};
    value.copy(begin, std::size(value));
    begin[std::size(value)] = '\0';
    return begin + std::size(value) + 1;
  }

  /// Don't convert to this type; it has nowhere to store its contents.
  static std::string_view from_string(std::string_view) = delete;
};


template<> struct nullness<zview> : no_null<zview>
{};


/// String traits for `zview`.
template<> struct string_traits<zview>
{
  static constexpr std::size_t
  size_buffer(std::string_view const &value) noexcept
  {
    return std::size(value) + 1;
  }

  static char *into_buf(char *begin, char *end, zview const &value)
  {
    auto const size{std::size(value)};
    if (internal::cmp_less_equal(end - begin, std::size(value)))
      throw conversion_overrun{"Not enough buffer space to store this zview."};
    value.copy(begin, size);
    begin[size] = '\0';
    return begin + size + 1;
  }

  static std::string_view to_buf(char *begin, char *end, zview const &value)
  {
    return {into_buf(begin, end, value), std::size(value)};
  }

  /// Don't convert to this type; it has nowhere to store its contents.
  static zview from_string(std::string_view) = delete;
};


template<> struct nullness<std::stringstream> : no_null<std::stringstream>
{};


template<> struct string_traits<std::stringstream>
{
  static std::size_t size_buffer(std::stringstream const &) = delete;

  static std::stringstream from_string(std::string_view text)
  {
    std::stringstream stream;
    stream.write(text.data(), std::streamsize(std::size(text)));
    return stream;
  }

  static char *into_buf(char *, char *, std::stringstream const &) = delete;
  static std::string_view
  to_buf(char *, char *, std::stringstream const &) = delete;
};


template<> struct nullness<std::nullptr_t>
{
  static constexpr bool has_null = true;
  static constexpr bool always_null = true;
  static constexpr bool is_null(std::nullptr_t const &) noexcept
  {
    return true;
  }
  static constexpr std::nullptr_t null() noexcept { return nullptr; }
};


template<> struct nullness<std::nullopt_t>
{
  static constexpr bool has_null = true;
  static constexpr bool always_null = true;
  static constexpr bool is_null(std::nullopt_t const &) noexcept
  {
    return true;
  }
  static constexpr std::nullopt_t null() noexcept { return std::nullopt; }
};


template<> struct nullness<std::monostate>
{
  static constexpr bool has_null = true;
  static constexpr bool always_null = true;
  static constexpr bool is_null(std::monostate const &) noexcept
  {
    return true;
  }
  static constexpr std::monostate null() noexcept { return {}; }
};


template<typename T> struct nullness<std::unique_ptr<T>>
{
  static constexpr bool has_null = true;
  static constexpr bool always_null = false;
  static constexpr bool is_null(std::unique_ptr<T> const &t) noexcept
  {
    return not t or pqxx::is_null(*t);
  }
  static constexpr std::unique_ptr<T> null() { return {}; }
};


template<typename T, typename... Args>
struct string_traits<std::unique_ptr<T, Args...>>
{
  static std::unique_ptr<T> from_string(std::string_view text)
  {
    return std::make_unique<T>(string_traits<T>::from_string(text));
  }

  static char *
  into_buf(char *begin, char *end, std::unique_ptr<T, Args...> const &value)
  {
    return string_traits<T>::into_buf(begin, end, *value);
  }

  static zview
  to_buf(char *begin, char *end, std::unique_ptr<T, Args...> const &value)
  {
    if (value)
      return string_traits<T>::to_buf(begin, end, *value);
    else
      return {};
  }

  static std::size_t
  size_buffer(std::unique_ptr<T, Args...> const &value) noexcept
  {
    return pqxx::size_buffer(*value.get());
  }
};


template<typename T, typename... Args>
inline format param_format(std::unique_ptr<T, Args...> const &value)
{
  return param_format(*value);
}


template<typename T, typename... Args>
inline constexpr bool is_unquoted_safe<std::unique_ptr<T, Args...>>{
  is_unquoted_safe<T>};


template<typename T> struct nullness<std::shared_ptr<T>>
{
  static constexpr bool has_null = true;
  static constexpr bool always_null = false;
  static constexpr bool is_null(std::shared_ptr<T> const &t) noexcept
  {
    return not t or pqxx::is_null(*t);
  }
  static constexpr std::shared_ptr<T> null() { return {}; }
};


template<typename T> struct string_traits<std::shared_ptr<T>>
{
  static std::shared_ptr<T> from_string(std::string_view text)
  {
    return std::make_shared<T>(string_traits<T>::from_string(text));
  }

  static zview to_buf(char *begin, char *end, std::shared_ptr<T> const &value)
  {
    return string_traits<T>::to_buf(begin, end, *value);
  }
  static char *
  into_buf(char *begin, char *end, std::shared_ptr<T> const &value)
  {
    return string_traits<T>::into_buf(begin, end, *value);
  }
  static std::size_t size_buffer(std::shared_ptr<T> const &value) noexcept
  {
    return pqxx::size_buffer(*value);
  }
};


template<typename T> format param_format(std::shared_ptr<T> const &value)
{
  return param_format(*value);
}


template<typename T>
inline constexpr bool is_unquoted_safe<std::shared_ptr<T>>{
  is_unquoted_safe<T>};


template<>
struct nullness<std::basic_string<std::byte>>
        : no_null<std::basic_string<std::byte>>
{};


#if defined(PQXX_HAVE_CONCEPTS)
template<binary DATA> struct nullness<DATA> : no_null<DATA>
{};


template<binary DATA> inline constexpr format param_format(DATA const &)
{
  return format::binary;
}


template<binary DATA> struct string_traits<DATA>
{
  static std::size_t size_buffer(DATA const &value) noexcept
  {
    return internal::size_esc_bin(std::size(value));
  }

  static zview to_buf(char *begin, char *end, DATA const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static char *into_buf(char *begin, char *end, DATA const &value)
  {
    auto const budget{size_buffer(value)};
    if (internal::cmp_less(end - begin, budget))
      throw conversion_overrun{
        "Not enough buffer space to escape binary data."};
    internal::esc_bin(value, begin);
    return begin + budget;
  }

  static DATA from_string(std::string_view text)
  {
    auto const size{pqxx::internal::size_unesc_bin(std::size(text))};
    std::basic_string<std::byte> buf;
    buf.resize(size);
    pqxx::internal::unesc_bin(text, reinterpret_cast<std::byte *>(buf.data()));
    return buf;
  }
};
#endif // PQXX_HAVE_CONCEPTS


template<> struct string_traits<std::basic_string<std::byte>>
{
  static std::size_t
  size_buffer(std::basic_string<std::byte> const &value) noexcept
  {
    return internal::size_esc_bin(std::size(value));
  }

  static zview
  to_buf(char *begin, char *end, std::basic_string<std::byte> const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static char *
  into_buf(char *begin, char *end, std::basic_string<std::byte> const &value)
  {
    auto const budget{size_buffer(value)};
    if (internal::cmp_less(end - begin, budget))
      throw conversion_overrun{
        "Not enough buffer space to escape binary data."};
    internal::esc_bin(value, begin);
    return begin + budget;
  }

  static std::basic_string<std::byte> from_string(std::string_view text)
  {
    auto const size{pqxx::internal::size_unesc_bin(std::size(text))};
    std::basic_string<std::byte> buf;
    buf.resize(size);
    pqxx::internal::unesc_bin(text, reinterpret_cast<std::byte *>(buf.data()));
    return buf;
  }
};


template<>
inline constexpr format param_format(std::basic_string<std::byte> const &)
{
  return format::binary;
}


template<>
struct nullness<std::basic_string_view<std::byte>>
        : no_null<std::basic_string_view<std::byte>>
{};


template<> struct string_traits<std::basic_string_view<std::byte>>
{
  static std::size_t
  size_buffer(std::basic_string_view<std::byte> const &value) noexcept
  {
    return internal::size_esc_bin(std::size(value));
  }

  static zview to_buf(
    char *begin, char *end, std::basic_string_view<std::byte> const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static char *into_buf(
    char *begin, char *end, std::basic_string_view<std::byte> const &value)
  {
    auto const budget{size_buffer(value)};
    if (internal::cmp_less(end - begin, budget))
      throw conversion_overrun{
        "Not enough buffer space to escape binary data."};
    internal::esc_bin(value, begin);
    return begin + budget;
  }

  // There's no from_string, because there's nobody to hold the data.
};

template<>
inline constexpr format param_format(std::basic_string_view<std::byte> const &)
{
  return format::binary;
}
} // namespace pqxx


namespace pqxx::internal
{
/// String traits for SQL arrays.
template<typename Container> struct array_string_traits
{
private:
  using elt_type = strip_t<value_type<Container>>;
  using elt_traits = string_traits<elt_type>;
  static constexpr zview s_null{"NULL"};

public:
  static zview to_buf(char *begin, char *end, Container const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static char *into_buf(char *begin, char *end, Container const &value)
  {
    std::size_t const budget{size_buffer(value)};
    if (internal::cmp_less(end - begin, budget))
      throw conversion_overrun{
        "Not enough buffer space to convert array to string."};

    char *here = begin;
    *here++ = '{';

    bool nonempty{false};
    for (auto const &elt : value)
    {
      if (is_null(elt))
      {
        s_null.copy(here, std::size(s_null));
        here += std::size(s_null);
      }
      else if constexpr (is_sql_array<elt_type>)
      {
        // Render nested array in-place.  Then erase the trailing zero.
        here = elt_traits::into_buf(here, end, elt) - 1;
      }
      else if constexpr (is_unquoted_safe<elt_type>)
      {
        // No need to quote or escape.  Just convert the value straight into
        // its place in the array, and "backspace" the trailing zero.
        here = elt_traits::into_buf(here, end, elt) - 1;
      }
      else
      {
        *here++ = '"';

        // Use the tail end of the destination buffer as an intermediate
        // buffer.
        auto const elt_budget{pqxx::size_buffer(elt)};
        for (char const c : elt_traits::to_buf(end - elt_budget, end, elt))
        {
          if (c == '\\' or c == '"')
            *here++ = '\\';
          *here++ = c;
        }
        *here++ = '"';
      }
      *here++ = array_separator<elt_type>;
      nonempty = true;
    }

    // Erase that last comma, if present.
    if (nonempty)
      here--;

    *here++ = '}';
    *here++ = '\0';

    return here;
  }

  static std::size_t size_buffer(Container const &value) noexcept
  {
    if constexpr (is_unquoted_safe<elt_type>)
      return 3 + std::accumulate(
                   std::begin(value), std::end(value), std::size_t{},
                   [](std::size_t acc, elt_type const &elt) {
                     return acc +
                            (pqxx::is_null(elt) ?
                               std::size(s_null) :
                               elt_traits::size_buffer(elt)) -
                            1;
                   });
    else
      return 3 + std::accumulate(
                   std::begin(value), std::end(value), std::size_t{},
                   [](std::size_t acc, elt_type const &elt) {
                     // Opening and closing quotes, plus worst-case escaping,
                     // but don't count the trailing zeroes.
                     std::size_t const elt_size{
                       pqxx::is_null(elt) ? std::size(s_null) :
                                            elt_traits::size_buffer(elt) - 1};
                     return acc + 2 * elt_size + 2;
                   });
  }

  // We don't yet support parsing of array types using from_string.  Doing so
  // would require a reference to the connection.
};
} // namespace pqxx::internal


namespace pqxx
{
template<typename T, typename... Args>
struct nullness<std::vector<T, Args...>> : no_null<std::vector<T>>
{};


template<typename T, typename... Args>
struct string_traits<std::vector<T, Args...>>
        : internal::array_string_traits<std::vector<T, Args...>>
{};


/// We don't know how to pass array params in binary format, so pass as text.
template<typename T, typename... Args>
inline constexpr format param_format(std::vector<T, Args...> const &)
{
  return format::text;
}


/// A `std::vector<std::byte>` is a binary string.  Other vectors are not.
template<typename... Args>
inline constexpr format param_format(std::vector<std::byte, Args...> const &)
{
  return format::binary;
}


template<typename T> inline constexpr bool is_sql_array<std::vector<T>>{true};


template<typename T, std::size_t N>
struct nullness<std::array<T, N>> : no_null<std::array<T, N>>
{};


template<typename T, std::size_t N>
struct string_traits<std::array<T, N>>
        : internal::array_string_traits<std::array<T, N>>
{};


/// We don't know how to pass array params in binary format, so pass as text.
template<typename T, typename... Args, Args... args>
inline constexpr format param_format(std::array<T, args...> const &)
{
  return format::text;
}


/// An array of `std::byte` is a binary string.
template<typename... Args, Args... args>
inline constexpr format param_format(std::array<std::byte, args...> const &)
{
  return format::binary;
}


template<typename T, std::size_t N>
inline constexpr bool is_sql_array<std::array<T, N>>{true};
} // namespace pqxx


namespace pqxx
{
template<typename T> inline std::string to_string(T const &value)
{
  if (is_null(value))
    throw conversion_error{
      "Attempt to convert null " + type_name<T> + " to a string."};

  std::string buf;
  // We can't just reserve() space; modifying the terminating zero leads to
  // undefined behaviour.
  buf.resize(size_buffer(value));
  auto const data{buf.data()};
  auto const end{
    string_traits<T>::into_buf(data, data + std::size(buf), value)};
  buf.resize(static_cast<std::size_t>(end - data - 1));
  return buf;
}


template<> inline std::string to_string(float const &value)
{
  return internal::to_string_float(value);
}
template<> inline std::string to_string(double const &value)
{
  return internal::to_string_float(value);
}
template<> inline std::string to_string(long double const &value)
{
  return internal::to_string_float(value);
}
template<> inline std::string to_string(std::stringstream const &value)
{
  return value.str();
}


template<typename T> inline void into_string(T const &value, std::string &out)
{
  if (is_null(value))
    throw conversion_error{
      "Attempt to convert null " + type_name<T> + " to a string."};

  // We can't just reserve() data; modifying the terminating zero leads to
  // undefined behaviour.
  out.resize(size_buffer(value) + 1);
  auto const data{out.data()};
  auto const end{
    string_traits<T>::into_buf(data, data + std::size(out), value)};
  out.resize(static_cast<std::size_t>(end - data - 1));
}
} // namespace pqxx
