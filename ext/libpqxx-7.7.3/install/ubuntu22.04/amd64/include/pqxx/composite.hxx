#ifndef PQXX_H_COMPOSITE
#define PQXX_H_COMPOSITE

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/internal/array-composite.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/util.hxx"

namespace pqxx
{
/// Parse a string representation of a value of a composite type.
/** @warning This code is still experimental.  Use with care.
 *
 * You may use this as a helper while implementing your own @ref string_traits
 * for a composite type.
 *
 * This function interprets `text` as the string representation of a value of
 * some composite type, and sets each of `fields` to the respective values of
 * its fields.  The field types must be copy-assignable.
 *
 * The number of fields must match the number of fields in the composite type,
 * and there must not be any other text in the input.  The function is meant to
 * handle any value string that the backend can produce, but not necessarily
 * every valid alternative spelling.
 *
 * Fields in composite types can be null.  When this happens, the C++ type of
 * the corresponding field reference must be of a type that can handle nulls.
 * If you are working with a type that does not have an inherent null value,
 * such as e.g. `int`, consider using `std::optional`.
 */
template<typename... T>
inline void parse_composite(
  pqxx::internal::encoding_group enc, std::string_view text, T &...fields)
{
  static_assert(sizeof...(fields) > 0);

  auto const scan{pqxx::internal::get_glyph_scanner(enc)};
  auto const data{std::data(text)};
  auto const size{std::size(text)};
  if (size == 0)
    throw conversion_error{"Cannot parse composite value from empty string."};

  std::size_t here{0}, next{scan(data, size, here)};
  if (next != 1 or data[here] != '(')
    throw conversion_error{
      internal::concat("Invalid composite value string: ", text)};

  here = next;

  constexpr auto num_fields{sizeof...(fields)};
  std::size_t index{0};
  (pqxx::internal::parse_composite_field(
     index, text, here, fields, scan, num_fields - 1),
   ...);
  if (here != std::size(text))
    throw conversion_error{internal::concat(
      "Composite value did not end at the closing parenthesis: '", text,
      "'.")};
  if (text[here - 1] != ')')
    throw conversion_error{internal::concat(
      "Composive value did not end in parenthesis: '", text, "'")};
}


/// Parse a string representation of a value of a composite type.
/** @warning This version only works for UTF-8 and single-byte encodings.
 *
 * For proper encoding support, use the composite-type support in the
 * `field` class.
 */
template<typename... T>
inline void parse_composite(std::string_view text, T &...fields)
{
  parse_composite(pqxx::internal::encoding_group::MONOBYTE, text, fields...);
}
} // namespace pqxx


namespace pqxx::internal
{
constexpr char empty_composite_str[]{"()"};
} // namespace pqxx::internal


namespace pqxx
{
/// Estimate the buffer size needed to represent a value of a composite type.
/** Returns a conservative estimate.
 */
template<typename... T>
[[nodiscard]] inline std::size_t
composite_size_buffer(T const &...fields) noexcept
{
  constexpr auto num{sizeof...(fields)};

  // Size for a multi-field composite includes room for...
  //  + opening parenthesis
  //  + field budgets
  //  + separating comma per field
  //  - comma after final field
  //  + closing parenthesis
  //  + terminating zero

  if constexpr (num == 0)
    return std::size(pqxx::internal::empty_composite_str);
  else
    return 1 + (pqxx::internal::size_composite_field_buffer(fields) + ...) +
           num + 1;
}


/// Render a series of values as a single composite SQL value.
/** @warning This code is still experimental.  Use with care.
 *
 * You may use this as a helper while implementing your own `string_traits`
 * for a composite type.
 */
template<typename... T>
inline char *composite_into_buf(char *begin, char *end, T const &...fields)
{
  if (std::size_t(end - begin) < composite_size_buffer(fields...))
    throw conversion_error{
      "Buffer space may not be enough to represent composite value."};

  constexpr auto num_fields{sizeof...(fields)};
  if constexpr (num_fields == 0)
  {
    constexpr char empty[]{"()"};
    std::memcpy(begin, empty, std::size(empty));
    return begin + std::size(empty);
  }

  char *pos{begin};
  *pos++ = '(';

  (pqxx::internal::write_composite_field<T>(pos, end, fields), ...);

  // If we've got multiple fields, "backspace" that last comma.
  if constexpr (num_fields > 1)
    --pos;
  *pos++ = ')';
  *pos++ = '\0';
  return pos;
}
} // namespace pqxx
#endif
