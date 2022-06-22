/** Handling of SQL arrays.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <utility>

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/array.hxx"
#include "pqxx/except.hxx"
#include "pqxx/internal/array-composite.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/strconv.hxx"
#include "pqxx/util.hxx"

#include "pqxx/internal/header-post.hxx"


namespace pqxx
{
/// Scan to next glyph in the buffer.  Assumes there is one.
[[nodiscard]] std::string::size_type
array_parser::scan_glyph(std::string::size_type pos) const
{
  return m_scan(std::data(m_input), std::size(m_input), pos);
}


/// Scan to next glyph in a substring.  Assumes there is one.
std::string::size_type array_parser::scan_glyph(
  std::string::size_type pos, std::string::size_type end) const
{
  return m_scan(std::data(m_input), end, pos);
}


/// Find the end of a single-quoted SQL string in an SQL array.
/** Call this while pointed at the opening quote.
 *
 * Returns the offset of the first character after the closing quote.
 */
std::string::size_type array_parser::scan_single_quoted_string() const
{
  assert(m_input[m_pos] == '\'');
  auto const sz{std::size(m_input)};
  auto here{pqxx::internal::find_char<'\\', '\''>(m_scan, m_input, m_pos + 1)};
  while (here < sz)
  {
    char const c{m_input[here]};
    // Consume the slash or quote that we found.
    ++here;
    if (c == '\'')
    {
      // Single quote.

      // At end?
      if (here >= sz)
        return here;

      // SQL escapes single quotes by doubling them.  Terrible idea, but it's
      // what we have.  Inspect the next character to find out whether this
      // is the closing quote, or an escaped one inside the string.
      if (m_input[here] != '\'')
        return here;
      // Check against embedded "'" byte in a multichar byte.  If we do have a
      // multibyte char, then we're still out of the string.
      if (scan_glyph(here, sz) > here + 1)
        PQXX_UNLIKELY return here;

      // We have a second quote.  Consume it as well.
      ++here;
    }
    else
    {
      assert(c == '\\');
      // Backslash escape.  Skip ahead by one more character.
      here = scan_glyph(here, sz);
    }
    // Race on to the next quote or backslash.
    here = pqxx::internal::find_char<'\\', '\''>(m_scan, m_input, here);
  }
  throw argument_error{internal::concat("Null byte in SQL string: ", m_input)};
}


/// Parse a single-quoted SQL string: un-quote it and un-escape it.
std::string
array_parser::parse_single_quoted_string(std::string::size_type end) const
{
  std::string output;
  // Maximum output size is same as the input size, minus the opening and
  // closing quotes.  In the worst case, the real number could be half that.
  // Usually it'll be a pretty close estimate.
  output.reserve(end - m_pos - 2);
  // XXX: find_char<'\\', '\''>().
  for (auto here = m_pos + 1, next = scan_glyph(here, end); here < end - 1;
       here = next, next = scan_glyph(here, end))
  {
    if (next - here == 1 and (m_input[here] == '\'' or m_input[here] == '\\'))
    {
      // Skip escape.  (Performance-wise, we bet that these are relatively
      // rare.)
      PQXX_UNLIKELY
      here = next;
      next = scan_glyph(here, end);
    }

    output.append(std::data(m_input) + here, std::data(m_input) + next);
  }

  return output;
}


/// Find the end of a double-quoted SQL string in an SQL array.
std::string::size_type array_parser::scan_double_quoted_string() const
{
  return pqxx::internal::scan_double_quoted_string(
    std::data(m_input), std::size(m_input), m_pos, m_scan);
}


/// Parse a double-quoted SQL string: un-quote it and un-escape it.
std::string
array_parser::parse_double_quoted_string(std::string::size_type end) const
{
  return pqxx::internal::parse_double_quoted_string(
    std::data(m_input), end, m_pos, m_scan);
}


/// Find the end of an unquoted string in an SQL array.
/** Assumes UTF-8 or an ASCII-superset single-byte encoding.
 */
std::string::size_type array_parser::scan_unquoted_string() const
{
  return pqxx::internal::scan_unquoted_string<',', ';', '}'>(
    std::data(m_input), std::size(m_input), m_pos, m_scan);
}


/// Parse an unquoted SQL string.
/** Here, the special unquoted value NULL means a null value, not a string
 * that happens to spell "NULL".
 */
std::string
array_parser::parse_unquoted_string(std::string::size_type end) const
{
  return pqxx::internal::parse_unquoted_string(
    std::data(m_input), end, m_pos, m_scan);
}


array_parser::array_parser(
  std::string_view input, internal::encoding_group enc) :
        m_input(input), m_scan(internal::get_glyph_scanner(enc))
{}


std::pair<array_parser::juncture, std::string> array_parser::get_next()
{
  std::string value;

  if (m_pos >= std::size(m_input))
    return std::make_pair(juncture::done, value);

  juncture found;
  std::string::size_type end;

  if (scan_glyph(m_pos) - m_pos > 1)
  {
    // Non-ASCII unquoted string.
    end = scan_unquoted_string();
    value = parse_unquoted_string(end);
    found = juncture::string_value;
  }
  else
    switch (m_input[m_pos])
    {
    case '\0': throw failure{"Unexpected zero byte in array."};
    case '{':
      found = juncture::row_start;
      end = scan_glyph(m_pos);
      break;
    case '}':
      found = juncture::row_end;
      end = scan_glyph(m_pos);
      break;
    case '\'':
      found = juncture::string_value;
      end = scan_single_quoted_string();
      value = parse_single_quoted_string(end);
      break;
    case '"':
      found = juncture::string_value;
      end = scan_double_quoted_string();
      value = parse_double_quoted_string(end);
      break;
    default:
      end = scan_unquoted_string();
      value = parse_unquoted_string(end);
      if (value == "NULL")
      {
        // In this one situation, as a special case, NULL means a null field,
        // not a string that happens to spell "NULL".
        value.clear();
        found = juncture::null_value;
      }
      else
      {
        // The normal case: we just parsed an unquoted string.  The value is
        // what we need.
        PQXX_LIKELY
        found = juncture::string_value;
      }
      break;
    }

  // Skip a trailing field separator, if present.
  if (end < std::size(m_input))
  {
    auto next{scan_glyph(end)};
    if (next - end == 1 and (m_input[end] == ',' or m_input[end] == ';'))
      PQXX_UNLIKELY
    end = next;
  }

  m_pos = end;
  return std::make_pair(found, value);
}
} // namespace pqxx
