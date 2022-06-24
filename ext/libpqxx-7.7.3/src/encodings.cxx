/** Implementation of string encodings support
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>

extern "C"
{
#include <libpq-fe.h>
}


#include "pqxx/internal/header-pre.hxx"

#include "pqxx/except.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/internal/encodings.hxx"
#include "pqxx/strconv.hxx"

#include "pqxx/internal/header-post.hxx"


namespace pqxx
{
PQXX_DECLARE_ENUM_CONVERSION(pqxx::internal::encoding_group);
}


// Internal helper functions
namespace
{
/// Extract byte from buffer, return as unsigned char.
constexpr PQXX_PURE unsigned char
get_byte(char const buffer[], std::size_t offset) noexcept
{
  return static_cast<unsigned char>(buffer[offset]);
}


[[noreturn]] void throw_for_encoding_error(
  char const *encoding_name, char const buffer[], std::size_t start,
  std::size_t count)
{
  std::stringstream s;
  s << "Invalid byte sequence for encoding " << encoding_name << " at byte "
    << start << ": " << std::hex << std::setw(2) << std::setfill('0');
  for (std::size_t i{0}; i < count; ++i)
  {
    s << "0x" << static_cast<unsigned int>(get_byte(buffer, start + i));
    if (i + 1 < count)
      s << " ";
  }
  throw pqxx::argument_error{s.str()};
}


/// Does value lie between bottom and top, inclusive?
constexpr PQXX_PURE bool
between_inc(unsigned char value, unsigned bottom, unsigned top)
{
  return value >= bottom and value <= top;
}


/*
EUC-JP and EUC-JIS-2004 represent slightly different code points but iterate
the same:
 * https://en.wikipedia.org/wiki/Extended_Unix_Code#EUC-JP
 * http://x0213.org/codetable/index.en.html
*/
PQXX_PURE std::size_t next_seq_for_euc_jplike(
  char const buffer[], std::size_t buffer_len, std::size_t start,
  char const encoding_name[])
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error(encoding_name, buffer, start, 1);

  auto const byte2{get_byte(buffer, start + 1)};
  if (byte1 == 0x8e)
  {
    if (not between_inc(byte2, 0xa1, 0xfe))
      PQXX_UNLIKELY
    throw_for_encoding_error(encoding_name, buffer, start, 2);

    return start + 2;
  }

  if (between_inc(byte1, 0xa1, 0xfe))
  {
    if (not between_inc(byte2, 0xa1, 0xfe))
      PQXX_UNLIKELY
    throw_for_encoding_error(encoding_name, buffer, start, 2);

    return start + 2;
  }

  if (byte1 == 0x8f and start + 3 <= buffer_len)
  {
    auto const byte3{get_byte(buffer, start + 2)};
    if (
      not between_inc(byte2, 0xa1, 0xfe) or not between_inc(byte3, 0xa1, 0xfe))
      PQXX_UNLIKELY
    throw_for_encoding_error(encoding_name, buffer, start, 3);

    return start + 3;
  }

  throw_for_encoding_error(encoding_name, buffer, start, 1);
}

/*
As far as I can tell, for the purposes of iterating the only difference between
SJIS and SJIS-2004 is increased range in the first byte of two-byte sequences
(0xEF increased to 0xFC).  Officially, that is; apparently the version of SJIS
used by Postgres has the same range as SJIS-2004.  They both have increased
range over the documented versions, not having the even/odd restriction for the
first byte in 2-byte sequences.
*/
// https://en.wikipedia.org/wiki/Shift_JIS#Shift_JIS_byte_map
// http://x0213.org/codetable/index.en.html
PQXX_PURE std::size_t next_seq_for_sjislike(
  char const buffer[], std::size_t buffer_len, std::size_t start,
  char const *encoding_name)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80 or between_inc(byte1, 0xa1, 0xdf))
    return start + 1;

  if (
    not between_inc(byte1, 0x81, 0x9f) and not between_inc(byte1, 0xe0, 0xfc))
    PQXX_UNLIKELY
  throw_for_encoding_error(encoding_name, buffer, start, 1);

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error(encoding_name, buffer, start, buffer_len - start);

  auto const byte2{get_byte(buffer, start + 1)};
  if (byte2 == 0x7f)
    PQXX_UNLIKELY
  throw_for_encoding_error(encoding_name, buffer, start, 2);

  if (between_inc(byte2, 0x40, 0x9e) or between_inc(byte2, 0x9f, 0xfc))
    return start + 2;

  PQXX_UNLIKELY
  throw_for_encoding_error(encoding_name, buffer, start, 2);
}
} // namespace


// Implement template specializations first.
namespace pqxx::internal
{
template<encoding_group> struct glyph_scanner
{
  PQXX_PURE static std::size_t
  call(char const buffer[], std::size_t buffer_len, std::size_t start);
};

template<> struct glyph_scanner<encoding_group::MONOBYTE>
{
  static PQXX_PURE constexpr std::size_t
  call(char const /* buffer */[], std::size_t buffer_len, std::size_t start)
  {
    if (start >= buffer_len)
      return std::string::npos;
    else
      return start + 1;
  }
};

// https://en.wikipedia.org/wiki/Big5#Organization
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::BIG5>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (not between_inc(byte1, 0x81, 0xfe) or (start + 2 > buffer_len))
    PQXX_UNLIKELY
  throw_for_encoding_error("BIG5", buffer, start, 1);

  auto const byte2{get_byte(buffer, start + 1)};
  if (
    not between_inc(byte2, 0x40, 0x7e) and not between_inc(byte2, 0xa1, 0xfe))
    PQXX_UNLIKELY
  throw_for_encoding_error("BIG5", buffer, start, 2);

  return start + 2;
}

/*
The PostgreSQL documentation claims that the EUC_* encodings are 1-3 bytes
each, but other documents explain that the EUC sets can contain 1-(2,3,4) bytes
depending on the specific extension:
    EUC_CN      : 1-2
    EUC_JP      : 1-3
    EUC_JIS_2004: 1-2
    EUC_KR      : 1-2
    EUC_TW      : 1-4
*/

// https://en.wikipedia.org/wiki/GB_2312#EUC-CN
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::EUC_CN>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (not between_inc(byte1, 0xa1, 0xf7) or start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("EUC_CN", buffer, start, 1);

  auto const byte2{get_byte(buffer, start + 1)};
  if (not between_inc(byte2, 0xa1, 0xfe))
    PQXX_UNLIKELY
  throw_for_encoding_error("EUC_CN", buffer, start, 2);

  return start + 2;
}


template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::EUC_JP>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  return next_seq_for_euc_jplike(buffer, buffer_len, start, "EUC_JP");
}


template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::EUC_JIS_2004>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  return next_seq_for_euc_jplike(buffer, buffer_len, start, "EUC_JIS_2004");
}


// https://en.wikipedia.org/wiki/Extended_Unix_Code#EUC-KR
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::EUC_KR>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (not between_inc(byte1, 0xa1, 0xfe) or start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("EUC_KR", buffer, start, 1);

  auto const byte2{get_byte(buffer, start + 1)};
  if (not between_inc(byte2, 0xa1, 0xfe))
    PQXX_UNLIKELY
  throw_for_encoding_error("EUC_KR", buffer, start, 1);

  return start + 2;
}

// https://en.wikipedia.org/wiki/Extended_Unix_Code#EUC-TW
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::EUC_TW>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    PQXX_UNLIKELY
  return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("EUC_KR", buffer, start, 1);

  auto const byte2{get_byte(buffer, start + 1)};
  if (between_inc(byte1, 0xa1, 0xfe))
  {
    if (not between_inc(byte2, 0xa1, 0xfe))
      PQXX_UNLIKELY
    throw_for_encoding_error("EUC_KR", buffer, start, 2);

    return start + 2;
  }

  if (byte1 != 0x8e or start + 4 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("EUC_KR", buffer, start, 1);

  if (
    between_inc(byte2, 0xa1, 0xb0) and
    between_inc(get_byte(buffer, start + 2), 0xa1, 0xfe) and
    between_inc(get_byte(buffer, start + 3), 0xa1, 0xfe))
    return start + 4;

  PQXX_UNLIKELY
  throw_for_encoding_error("EUC_KR", buffer, start, 4);
}

// https://en.wikipedia.org/wiki/GB_18030#Mapping
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::GB18030>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;
  if (byte1 == 0x80)
    throw_for_encoding_error("GB18030", buffer, start, buffer_len - start);

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("GB18030", buffer, start, buffer_len - start);

  auto const byte2{get_byte(buffer, start + 1)};
  if (between_inc(byte2, 0x40, 0xfe))
  {
    if (byte2 == 0x7f)
      PQXX_UNLIKELY
    throw_for_encoding_error("GB18030", buffer, start, 2);

    return start + 2;
  }

  if (start + 4 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("GB18030", buffer, start, buffer_len - start);

  if (
    between_inc(byte2, 0x30, 0x39) and
    between_inc(get_byte(buffer, start + 2), 0x81, 0xfe) and
    between_inc(get_byte(buffer, start + 3), 0x30, 0x39))
    return start + 4;

  PQXX_UNLIKELY
  throw_for_encoding_error("GB18030", buffer, start, 4);
}

// https://en.wikipedia.org/wiki/GBK_(character_encoding)#Encoding
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::GBK>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("GBK", buffer, start, 1);

  auto const byte2{get_byte(buffer, start + 1)};
  if (
    (between_inc(byte1, 0xa1, 0xa9) and between_inc(byte2, 0xa1, 0xfe)) or
    (between_inc(byte1, 0xb0, 0xf7) and between_inc(byte2, 0xa1, 0xfe)) or
    (between_inc(byte1, 0x81, 0xa0) and between_inc(byte2, 0x40, 0xfe) and
     byte2 != 0x7f) or
    (between_inc(byte1, 0xaa, 0xfe) and between_inc(byte2, 0x40, 0xa0) and
     byte2 != 0x7f) or
    (between_inc(byte1, 0xa8, 0xa9) and between_inc(byte2, 0x40, 0xa0) and
     byte2 != 0x7f) or
    (between_inc(byte1, 0xaa, 0xaf) and between_inc(byte2, 0xa1, 0xfe)) or
    (between_inc(byte1, 0xf8, 0xfe) and between_inc(byte2, 0xa1, 0xfe)) or
    (between_inc(byte1, 0xa1, 0xa7) and between_inc(byte2, 0x40, 0xa0) and
     byte2 != 0x7f))
    return start + 2;

  PQXX_UNLIKELY
  throw_for_encoding_error("GBK", buffer, start, 2);
}

/*
The PostgreSQL documentation claims that the JOHAB encoding is 1-3 bytes, but
"CJKV Information Processing" describes it (actually just the Hangul portion)
as "three five-bit segments" that reside inside 16 bits (2 bytes).

CJKV Information Processing by Ken Lunde, pg. 269:

  https://bit.ly/2BEOu5V
*/
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::JOHAB>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("JOHAB", buffer, start, 1);

  auto const byte2{get_byte(buffer, start)};
  if (
    (between_inc(byte1, 0x84, 0xd3) and
     (between_inc(byte2, 0x41, 0x7e) or between_inc(byte2, 0x81, 0xfe))) or
    ((between_inc(byte1, 0xd8, 0xde) or between_inc(byte1, 0xe0, 0xf9)) and
     (between_inc(byte2, 0x31, 0x7e) or between_inc(byte2, 0x91, 0xfe))))
    return start + 2;

  PQXX_UNLIKELY
  throw_for_encoding_error("JOHAB", buffer, start, 2);
}

/*
PostgreSQL's MULE_INTERNAL is the emacs rather than Xemacs implementation;
see the server/mb/pg_wchar.h PostgreSQL header file.
This is implemented according to the description in said header file, but I was
unable to get it to successfully iterate a MULE-encoded test CSV generated
using PostgreSQL 9.2.23.  Use this at your own risk.
*/
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::MULE_INTERNAL>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("MULE_INTERNAL", buffer, start, 1);

  auto const byte2{get_byte(buffer, start + 1)};
  if (between_inc(byte1, 0x81, 0x8d) and byte2 >= 0xa0)
    return start + 2;

  if (start + 3 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("MULE_INTERNAL", buffer, start, 2);

  if (
    ((byte1 == 0x9a and between_inc(byte2, 0xa0, 0xdf)) or
     (byte1 == 0x9b and between_inc(byte2, 0xe0, 0xef)) or
     (between_inc(byte1, 0x90, 0x99) and byte2 >= 0xa0)) and
    (byte2 >= 0xa0))
    return start + 3;

  if (start + 4 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("MULE_INTERNAL", buffer, start, 3);

  if (
    ((byte1 == 0x9c and between_inc(byte2, 0xf0, 0xf4)) or
     (byte1 == 0x9d and between_inc(byte2, 0xf5, 0xfe))) and
    get_byte(buffer, start + 2) >= 0xa0 and
    get_byte(buffer, start + 4) >= 0xa0)
    return start + 4;

  PQXX_UNLIKELY
  throw_for_encoding_error("MULE_INTERNAL", buffer, start, 4);
}

template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::SJIS>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  return next_seq_for_sjislike(buffer, buffer_len, start, "SJIS");
}

template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::SHIFT_JIS_2004>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  return next_seq_for_sjislike(buffer, buffer_len, start, "SHIFT_JIS_2004");
}

// https://en.wikipedia.org/wiki/Unified_Hangul_Code
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::UHC>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("UHC", buffer, start, buffer_len - start);

  auto const byte2{get_byte(buffer, start + 1)};
  if (between_inc(byte1, 0x80, 0xc6))
  {
    if (
      between_inc(byte2, 0x41, 0x5a) or between_inc(byte2, 0x61, 0x7a) or
      between_inc(byte2, 0x80, 0xfe))
      return start + 2;

    PQXX_UNLIKELY
    throw_for_encoding_error("UHC", buffer, start, 2);
  }

  if (between_inc(byte1, 0xa1, 0xfe))
  {
    if (not between_inc(byte2, 0xa1, 0xfe))
      PQXX_UNLIKELY
    throw_for_encoding_error("UHC", buffer, start, 2);

    return start + 2;
  }

  throw_for_encoding_error("UHC", buffer, start, 1);
}

// https://en.wikipedia.org/wiki/UTF-8#Description
template<>
PQXX_PURE std::size_t glyph_scanner<encoding_group::UTF8>::call(
  char const buffer[], std::size_t buffer_len, std::size_t start)
{
  if (start >= buffer_len)
    return std::string::npos;

  auto const byte1{get_byte(buffer, start)};
  if (byte1 < 0x80)
    return start + 1;

  if (start + 2 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("UTF8", buffer, start, buffer_len - start);

  auto const byte2{get_byte(buffer, start + 1)};
  if (between_inc(byte1, 0xc0, 0xdf))
  {
    if (not between_inc(byte2, 0x80, 0xbf))
      PQXX_UNLIKELY
    throw_for_encoding_error("UTF8", buffer, start, 2);

    return start + 2;
  }

  if (start + 3 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("UTF8", buffer, start, buffer_len - start);

  auto const byte3{get_byte(buffer, start + 2)};
  if (between_inc(byte1, 0xe0, 0xef))
  {
    if (between_inc(byte2, 0x80, 0xbf) and between_inc(byte3, 0x80, 0xbf))
      return start + 3;

    PQXX_UNLIKELY
    throw_for_encoding_error("UTF8", buffer, start, 3);
  }

  if (start + 4 > buffer_len)
    PQXX_UNLIKELY
  throw_for_encoding_error("UTF8", buffer, start, buffer_len - start);

  if (between_inc(byte1, 0xf0, 0xf7))
  {
    if (
      between_inc(byte2, 0x80, 0xbf) and between_inc(byte3, 0x80, 0xbf) and
      between_inc(get_byte(buffer, start + 3), 0x80, 0xbf))
      return start + 4;

    PQXX_UNLIKELY
    throw_for_encoding_error("UTF8", buffer, start, 4);
  }

  PQXX_UNLIKELY
  throw_for_encoding_error("UTF8", buffer, start, 1);
}


PQXX_PURE char const *name_encoding(int encoding_id)
{
  return pg_encoding_to_char(encoding_id);
}


encoding_group enc_group(int libpq_enc_id)
{
  return enc_group(name_encoding(libpq_enc_id));
}


encoding_group enc_group(std::string_view encoding_name)
{
  struct mapping
  {
    std::string_view const name;
    encoding_group const group;
    constexpr mapping(std::string_view n, encoding_group g) : name{n}, group{g}
    {}
    constexpr bool operator<(mapping const &rhs) const
    {
      return name < rhs.name;
    }
  };

  // C++20: Once compilers are ready, go full constexpr, leave to the compiler.
  auto const sz{std::size(encoding_name)};
  if (sz > 0u)
    switch (encoding_name[0])
    {
    case 'B':
      if (encoding_name == "BIG5"sv)
        return encoding_group::BIG5;
      PQXX_UNLIKELY
      break;
    case 'E':
      // C++20: Use string_view::starts_with().
      if ((sz >= 6u) and (encoding_name.substr(0, 4) == "EUC_"sv))
      {
        auto const subtype{encoding_name.substr(4)};
        static constexpr std::array<mapping, 5> subtypes{
          mapping{"CN"sv, encoding_group::EUC_CN},
          mapping{"JIS_2004"sv, encoding_group::EUC_JIS_2004},
          mapping{"JP"sv, encoding_group::EUC_JP},
          mapping{"KR"sv, encoding_group::EUC_KR},
          mapping{"TW"sv, encoding_group::EUC_TW},
        };
        for (auto const &m : subtypes)
          if (m.name == subtype)
            return m.group;
      }
      PQXX_UNLIKELY
      break;
    case 'G':
      if (encoding_name == "GB18030"sv)
        return encoding_group::GB18030;
      else if (encoding_name == "GBK"sv)
        return encoding_group::GBK;
      PQXX_UNLIKELY
      break;
    case 'I':
      // We know iso-8859-X, where 5 <= X < 9.  They're all monobyte encodings.
      if ((sz == 10) and (encoding_name.substr(0, 9) == "ISO_8859_"sv))
      {
        char const subtype{encoding_name[9]};
        if (('5' <= subtype) and (subtype < '9'))
          return encoding_group::MONOBYTE;
      }
      PQXX_UNLIKELY
      break;
    case 'J':
      if (encoding_name == "JOHAB"sv)
        return encoding_group::JOHAB;
      PQXX_UNLIKELY
      break;
    case 'K':
      if ((encoding_name == "KOI8R"sv) or (encoding_name == "KOI8U"sv))
        return encoding_group::MONOBYTE;
      PQXX_UNLIKELY
      break;
    case 'L':
      // We know LATIN1 through LATIN10.
      if (encoding_name.substr(0, 5) == "LATIN"sv)
      {
        auto const subtype{encoding_name.substr(5)};
        if (subtype.size() == 1)
        {
          char const n{subtype[0]};
          if (('1' <= n) and (n <= '9'))
            return encoding_group::MONOBYTE;
        }
        else if (subtype == "10"sv)
        {
          return encoding_group::MONOBYTE;
        }
      }
      PQXX_UNLIKELY
      break;
    case 'M':
      if (encoding_name == "MULE_INTERNAL"sv)
        return encoding_group::MULE_INTERNAL;
      PQXX_UNLIKELY
      break;
    case 'S':
      if (encoding_name == "SHIFT_JIS_2004"sv)
        return encoding_group::SHIFT_JIS_2004;
      else if (encoding_name == "SJIS"sv)
        return encoding_group::SJIS;
      else if (encoding_name == "SQL_ASCII"sv)
        return encoding_group::MONOBYTE;
      PQXX_UNLIKELY
      break;
    case 'U':
      if (encoding_name == "UHC"sv)
        return encoding_group::UHC;
      else if (encoding_name == "UTF8"sv)
        return encoding_group::UTF8;
      PQXX_UNLIKELY
      break;
    case 'W':
      if (encoding_name.substr(0, 3) == "WIN"sv)
      {
        auto const subtype{encoding_name.substr(3)};
        static constexpr std::array<std::string_view, 11u> subtypes{
          "866"sv,  "874"sv,  "1250"sv, "1251"sv, "1252"sv, "1253"sv,
          "1254"sv, "1255"sv, "1256"sv, "1257"sv, "1258"sv,
        };
        for (auto const n : subtypes)
          if (n == subtype)
            return encoding_group::MONOBYTE;
      }
      PQXX_UNLIKELY
      break;
    default: PQXX_UNLIKELY break;
    }
  PQXX_UNLIKELY
  throw std::invalid_argument{
    internal::concat("Unrecognized encoding: '", encoding_name, "'.")};
}


/// Look up instantiation @c T<enc>::call at runtime.
/** Here, "T" is a struct template with a static member function "call", whose
 * type is "F".
 *
 * The return value is a pointer to the "call" member function for the
 * instantiation of T for encoding group enc.
 */
template<template<encoding_group> class T, typename F>
constexpr inline F *for_encoding(encoding_group enc)
{
#define CASE_GROUP(ENC)                                                       \
  case encoding_group::ENC: return T<encoding_group::ENC>::call

  switch (enc)
  {
    PQXX_LIKELY CASE_GROUP(MONOBYTE);
    CASE_GROUP(BIG5);
    CASE_GROUP(EUC_CN);
    CASE_GROUP(EUC_JP);
    CASE_GROUP(EUC_JIS_2004);
    CASE_GROUP(EUC_KR);
    CASE_GROUP(EUC_TW);
    CASE_GROUP(GB18030);
    CASE_GROUP(GBK);
    CASE_GROUP(JOHAB);
    CASE_GROUP(MULE_INTERNAL);
    CASE_GROUP(SJIS);
    CASE_GROUP(SHIFT_JIS_2004);
    CASE_GROUP(UHC);
    PQXX_LIKELY CASE_GROUP(UTF8);
  }
  PQXX_UNLIKELY
  throw pqxx::usage_error{
    internal::concat("Unsupported encoding group code ", enc, ".")};

#undef CASE_GROUP
}


PQXX_PURE glyph_scanner_func *get_glyph_scanner(encoding_group enc)
{
  return for_encoding<glyph_scanner, glyph_scanner_func>(enc);
}


template<encoding_group E> struct char_finder
{
  constexpr static PQXX_PURE std::size_t
  call(std::string_view haystack, char needle, std::size_t start)
  {
    auto const buffer{std::data(haystack)};
    auto const size{std::size(haystack)};
    for (auto here{start}; here + 1 <= size;
         here = glyph_scanner<E>::call(buffer, size, here))
    {
      if (haystack[here] == needle)
        return here;
    }
    return std::string::npos;
  }
};


template<encoding_group E> struct string_finder
{
  PQXX_PURE constexpr static PQXX_PURE std::size_t
  call(std::string_view haystack, std::string_view needle, std::size_t start)
  {
    auto const buffer{std::data(haystack)};
    auto const size{std::size(haystack)};
    auto const needle_size{std::size(needle)};
    for (auto here{start}; here + needle_size <= size;
         here = glyph_scanner<E>::call(buffer, size, here))
    {
      if (std::memcmp(buffer + here, std::data(needle), needle_size) == 0)
        return here;
    }
    return std::string::npos;
  }
};

#undef DISPATCH_ENCODING_OPERATION
} // namespace pqxx::internal
