/*
 * Custom types for testing & libpqxx support those types
 */

#include <pqxx/strconv>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <vector>


namespace pqxx
{
template<> struct nullness<std::byte> : no_null<std::byte>
{};


constexpr static auto hex_digit{"0123456789abcdef"};


template<> struct string_traits<std::byte>
{
  static std::size_t size_buffer(std::byte const &) { return 3; }

  static zview to_buf(char *begin, char *end, std::byte const &value)
  {
    if (pqxx::internal::cmp_less(end - begin, size_buffer(value)))
      throw pqxx::conversion_overrun{
        "Not enough buffer to convert std::byte."};
    auto uc{static_cast<unsigned char>(value)};
    begin[0] = hex_digit[uc >> 4];
    begin[1] = hex_digit[uc & 0x0f];
    return zview{begin, 2u};
  }

  static char *into_buf(char *begin, char *end, std::byte const &value)
  {
    auto view{to_buf(begin, end, value)};
    return begin + std::size(view);
  }
};
} // namespace pqxx


class ipv4
{
public:
  ipv4() : m_as_int{0u} {}
  ipv4(ipv4 const &) = default;
  ipv4(ipv4 &&) = default;
  explicit ipv4(uint32_t i) : m_as_int{i} {}
  ipv4(
    unsigned char b1, unsigned char b2, unsigned char b3, unsigned char b4) :
          ipv4()
  {
    set_byte(0, b1);
    set_byte(1, b2);
    set_byte(2, b3);
    set_byte(3, b4);
  }

  bool operator==(ipv4 const &o) const { return m_as_int == o.m_as_int; }
  ipv4 &operator=(ipv4 const &) = default;

  /// Index bytes, from 0 to 3, in network (i.e. Big-Endian) byte order.
  unsigned int operator[](int byte) const
  {
    if (byte < 0 or byte > 3)
      throw pqxx::usage_error("Byte out of range.");
    auto const shift = compute_shift(byte);
    return static_cast<unsigned int>((m_as_int >> shift) & 0xff);
  }

  /// Set individual byte, in network byte order.
  void set_byte(int byte, uint32_t value)
  {
    auto const shift = compute_shift(byte);
    auto const blanked = (m_as_int & ~uint32_t(0xff << shift));
    m_as_int = (blanked | ((value & 0xff) << shift));
  }

private:
  static unsigned compute_shift(int byte)
  {
    if (byte < 0 or byte > 3)
      throw pqxx::usage_error("Byte out of range.");
    return static_cast<unsigned>((3 - byte) * 8);
  }

  uint32_t m_as_int;
};


using bytea = std::vector<unsigned char>;


namespace pqxx
{
template<> struct nullness<ipv4> : no_null<ipv4>
{};


template<> struct string_traits<ipv4>
{
  static ipv4 from_string(std::string_view text)
  {
    ipv4 ts;
    if (std::data(text) == nullptr)
      internal::throw_null_conversion(type_name<ipv4>);
    std::regex ipv4_regex{R"--((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))--"};
    std::smatch match;
    // Need non-temporary for `std::regex_match()`
    std::string sstr{text};
    if (not std::regex_match(sstr, match, ipv4_regex) or std::size(match) != 5)
      throw std::runtime_error{"Invalid ipv4 format: " + std::string{text}};
    try
    {
      for (std::size_t i{0}; i < 4; ++i)
        ts.set_byte(int(i), uint32_t(std::stoi(match[i + 1])));
    }
    catch (std::invalid_argument const &)
    {
      throw std::runtime_error{"Invalid ipv4 format: " + std::string{text}};
    }
    catch (std::out_of_range const &)
    {
      throw std::runtime_error{"Invalid ipv4 format: " + std::string{text}};
    }
    return ts;
  }

  static char *into_buf(char *begin, char *end, ipv4 const &value)
  {
    if (pqxx::internal::cmp_less(end - begin, size_buffer(value)))
      throw conversion_error{"Buffer too small for ipv4."};
    char *here = begin;
    for (int i = 0; i < 4; ++i)
    {
      here = string_traits<unsigned>::into_buf(here, end, value[i]);
      *(here - 1) = '.';
    }
    *(here - 1) = '\0';
    return here;
  }

  static zview to_buf(char *begin, char *end, ipv4 const &value)
  {
    return zview{
      begin,
      static_cast<std::size_t>(into_buf(begin, end, value) - begin - 1)};
  }

  static constexpr std::size_t size_buffer(ipv4 const &) noexcept
  {
    return 20;
  }
};


namespace
{
inline char nibble_to_hex(unsigned nibble)
{
  if (nibble < 10)
    return char('0' + nibble);
  else if (nibble < 16)
    return char('a' + (nibble - 10));
  else
    throw std::runtime_error{"Invalid digit going into bytea."};
}


inline unsigned hex_to_digit(char hex)
{
  auto x = static_cast<unsigned char>(hex);
  if (x >= '0' and x <= '9')
    return x - '0';
  else if (x >= 'a' and x <= 'f')
    return 10 + x - 'a';
  else if (x >= 'A' and x <= 'F')
    return 10 + x - 'A';
  else
    throw std::runtime_error{"Invalid hex in bytea."};
}
} // namespace


template<> struct nullness<bytea> : no_null<bytea>
{};


template<> struct string_traits<bytea>
{
  static bytea from_string(std::string_view text)
  {
    if ((std::size(text) & 1) != 0)
      throw std::runtime_error{"Odd hex size."};
    bytea value;
    value.reserve((std::size(text) - 2) / 2);
    for (std::size_t i = 2; i < std::size(text); i += 2)
    {
      auto hi = hex_to_digit(text[i]), lo = hex_to_digit(text[i + 1]);
      value.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return value;
  }

  static zview to_buf(char *begin, char *end, bytea const &value)
  {
    auto const need = size_buffer(value);
    auto const have = end - begin;
    if (std::size_t(have) < need)
      throw pqxx::conversion_overrun{"Not enough space in buffer for bytea."};
    char *pos = begin;
    *pos++ = '\\';
    *pos++ = 'x';
    for (unsigned char const u : value)
    {
      *pos++ = nibble_to_hex(unsigned(u) >> 4);
      *pos++ = nibble_to_hex(unsigned(u) & 0x0f);
    }
    *pos++ = '\0';
    return {begin, pos - begin - 1};
  }

  static char *into_buf(char *begin, char *end, bytea const &value)
  {
    return begin + std::size(to_buf(begin, end, value)) + 1;
  }

  static std::size_t size_buffer(bytea const &value)
  {
    return 2 + 2 * std::size(value) + 1;
  }
};
} // namespace pqxx
