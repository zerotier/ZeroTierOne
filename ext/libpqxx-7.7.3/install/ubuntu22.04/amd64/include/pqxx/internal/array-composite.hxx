#if !defined(PQXX_ARRAY_COMPOSITE_HXX)
#  define PQXX_ARRAY_COMPOSITE_HXX

#  include <cassert>

#  include "pqxx/strconv.hxx"

namespace pqxx::internal
{
// Find the end of a double-quoted string.
/** `input[pos]` must be the opening double quote.
 *
 * Returns the offset of the first position after the closing quote.
 */
inline std::size_t scan_double_quoted_string(
  char const input[], std::size_t size, std::size_t pos,
  pqxx::internal::glyph_scanner_func *scan)
{
  // XXX: find_char<'"', '\\'>().
  auto next{scan(input, size, pos)};
  bool at_quote{false};
  for (pos = next, next = scan(input, size, pos); pos < size;
       pos = next, next = scan(input, size, pos))
  {
    if (at_quote)
    {
      if (next - pos == 1 and input[pos] == '"')
      {
        // We just read a pair of double quotes.  Carry on.
        at_quote = false;
      }
      else
      {
        // We just read one double quote, and now we're at a character that's
        // not a second double quote.  Ergo, that last character was the
        // closing double quote and this is the position right after it.
        return pos;
      }
    }
    else if (next - pos == 1)
    {
      switch (input[pos])
      {
      case '\\':
        // Backslash escape.  Skip ahead by one more character.
        pos = next;
        next = scan(input, size, pos);
        break;

      case '"':
        // This is either the closing double quote, or the first of a pair of
        // double quotes.
        at_quote = true;
        break;
      }
    }
    else
    {
      // Multibyte character.  Carry on.
    }
  }
  if (not at_quote)
    throw argument_error{
      "Missing closing double-quote: " + std::string{input}};
  return pos;
}


/// Un-quote and un-escape a double-quoted SQL string.
inline std::string parse_double_quoted_string(
  char const input[], std::size_t end, std::size_t pos,
  pqxx::internal::glyph_scanner_func *scan)
{
  std::string output;
  // Maximum output size is same as the input size, minus the opening and
  // closing quotes.  Or in the extreme opposite case, the real number could be
  // half that.  Usually it'll be a pretty close estimate.
  output.reserve(std::size_t(end - pos - 2));

  for (auto here{scan(input, end, pos)}, next{scan(input, end, here)};
       here < end - 1; here = next, next = scan(input, end, here))
  {
    // A backslash here is always an escape.  So is a double-quote, since we're
    // inside the double-quoted string.  In either case, we can just ignore the
    // escape character and use the next character.  This is the one redeeming
    // feature of SQL's escaping system.
    if ((next - here == 1) and (input[here] == '\\' or input[here] == '"'))
    {
      // Skip escape.
      here = next;
      next = scan(input, end, here);
    }
    output.append(input + here, input + next);
  }
  return output;
}


/// Find the end of an unquoted string in an array or composite-type value.
/** Stops when it gets to the end of the input; or when it sees any of the
 * characters in STOP which has not been escaped.
 *
 * For array values, STOP is a comma, a semicolon, or a closing brace.  For
 * a value of a composite type, STOP is a comma or a closing parenthesis.
 */
template<char... STOP>
inline std::size_t scan_unquoted_string(
  char const input[], std::size_t size, std::size_t pos,
  pqxx::internal::glyph_scanner_func *scan)
{
  bool at_backslash{false};
  auto next{scan(input, size, pos)};
  while ((pos < size) and
         ((next - pos) > 1 or at_backslash or ((input[pos] != STOP) and ...)))
  {
    pos = next;
    next = scan(input, size, pos);
    at_backslash =
      ((not at_backslash) and ((next - pos) == 1) and (input[pos] == '\\'));
  }
  return pos;
}


/// Parse an unquoted array entry or cfield of a composite-type field.
inline std::string parse_unquoted_string(
  char const input[], std::size_t end, std::size_t pos,
  pqxx::internal::glyph_scanner_func *scan)
{
  std::string output;
  bool at_backslash{false};
  output.reserve(end - pos);
  for (auto next{scan(input, end, pos)}; pos < end;
       pos = next, next = scan(input, end, pos))
  {
    at_backslash =
      ((not at_backslash) and ((next - pos) == 1) and (input[pos] == '\\'));
    if (not at_backslash)
      output.append(input + pos, next - pos);
  }
  return output;
}


/// Parse a field of a composite-type value.
/** `T` is the C++ type of the field we're parsing, and `index` is its
 * zero-based number.
 *
 * Strip off the leading parenthesis or bracket yourself before parsing.
 * However, this function will parse the lcosing parenthesis or bracket.
 *
 * After a successful parse, `pos` will point at `std::end(text)`.
 *
 * For the purposes of parsing, ranges and arrays count as compositve values,
 * so this function supports parsing those.  If you specifically need a closing
 * parenthesis, check afterwards that `text` did not end in a bracket instead.
 *
 * @param index Index of the current field, zero-based.  It will increment for
 *     the next field.
 * @param input Full input text for the entire composite-type value.
 * @param pos Starting position (in `input`) of the field that we're parsing.
 *     After parsing, this will point at the beginning of the next field if
 *     there is one, or one position past the last character otherwise.
 * @param field Destination for the parsed value.
 * @param scan Glyph scanning function for the relevant encoding type.
 * @param last_field Number of the last field in the value (zero-based).  When
 *     parsing the last field, this will equal `index`.
 */
template<typename T>
inline void parse_composite_field(
  std::size_t &index, std::string_view input, std::size_t &pos, T &field,
  glyph_scanner_func *scan, std::size_t last_field)
{
  assert(index <= last_field);
  auto next{scan(std::data(input), std::size(input), pos)};
  if ((next - pos) != 1)
    throw conversion_error{"Non-ASCII character in composite-type syntax."};

  // Expect a field.
  switch (input[pos])
  {
  case ',':
  case ')':
  case ']':
    // The field is empty, i.e, null.
    if constexpr (nullness<T>::has_null)
      field = nullness<T>::null();
    else
      throw conversion_error{
        "Can't read composite field " + to_string(index) + ": C++ type " +
        type_name<T> + " does not support nulls."};
    break;

  case '"': {
    auto const stop{scan_double_quoted_string(
      std::data(input), std::size(input), pos, scan)};
    auto const text{
      parse_double_quoted_string(std::data(input), stop, pos, scan)};
    field = from_string<T>(text);
    pos = stop;
  }
  break;

  default: {
    auto const stop{scan_unquoted_string<',', ')', ']'>(
      std::data(input), std::size(input), pos, scan)};
    auto const text{parse_unquoted_string(std::data(input), stop, pos, scan)};
    field = from_string<T>(text);
    pos = stop;
  }
  break;
  }

  // Expect a comma or a closing parenthesis.
  next = scan(std::data(input), std::size(input), pos);

  if ((next - pos) != 1)
    throw conversion_error{
      "Unexpected non-ASCII character after composite field: " +
      std::string{input}};

  if (index < last_field)
  {
    if (input[pos] != ',')
      throw conversion_error{
        "Found '" + std::string{input[pos]} +
        "' in composite value where comma was expected: " + std::data(input)};
  }
  else
  {
    if (input[pos] == ',')
      throw conversion_error{
        "Composite value contained more fields than the expected " +
        to_string(last_field) + ": " + std::data(input)};
    if (input[pos] != ')' and input[pos] != ']')
      throw conversion_error{
        "Composite value has unexpected characters where closing parenthesis "
        "was expected: " +
        std::string{input}};
    if (next != std::size(input))
      throw conversion_error{
        "Composite value has unexpected text after closing parenthesis: " +
        std::string{input}};
  }

  pos = next;
  ++index;
}


/// Conservatively estimate buffer size needed for a composite field.
template<typename T>
inline std::size_t size_composite_field_buffer(T const &field)
{
  if constexpr (is_unquoted_safe<T>)
  {
    // Safe to copy, without quotes or escaping.  Drop the terminating zero.
    return size_buffer(field) - 1;
  }
  else
  {
    // + Opening quote.
    // + Field budget.
    // - Terminating zero.
    // + Escaping for each byte in the field's string representation.
    // - Escaping for terminating zero.
    // + Closing quote.
    return 1 + 2 * (size_buffer(field) - 1) + 1;
  }
}


template<typename T>
inline void write_composite_field(char *&pos, char *end, T const &field)
{
  if constexpr (is_unquoted_safe<T>)
  {
    // No need for quoting or escaping.  Convert it straight into its final
    // place in the buffer, and "backspace" the trailing zero.
    pos = string_traits<T>::into_buf(pos, end, field) - 1;
  }
  else
  {
    // The field may need escaping, which means we need an intermediate buffer.
    // To avoid allocating that at run time, we use the end of the buffer that
    // we have.
    auto const budget{size_buffer(field)};
    *pos++ = '"';

    // Now escape buf into its final position.
    for (char const c : string_traits<T>::to_buf(end - budget, end, field))
    {
      if ((c == '"') or (c == '\\'))
        *pos++ = '\\';

      *pos++ = c;
    }

    *pos++ = '"';
  }

  *pos++ = ',';
}
} // namespace pqxx::internal
#endif
