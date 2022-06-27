#include "../test_helpers.hxx"

#include "pqxx/internal/encodings.hxx"


namespace
{
void test_scan_ascii()
{
  auto const scan{pqxx::internal::get_glyph_scanner(
    pqxx::internal::encoding_group::MONOBYTE)};
  std::string const text{"hello"};

  PQXX_CHECK_EQUAL(
    scan(text.c_str(), std::size(text), 0), 1ul,
    "Monobyte scanner acting up.");
  PQXX_CHECK_EQUAL(
    scan(text.c_str(), std::size(text), 1), 2ul,
    "Monobyte scanner is inconsistent.");
}


void test_scan_utf8()
{
  auto const scan{
    pqxx::internal::get_glyph_scanner(pqxx::internal::encoding_group::UTF8)};

  // Thai: "Khrab".
  std::string const text{"\xe0\xb8\x95\xe0\xb8\xa3\xe0\xb8\xb1\xe0\xb8\x9a"};
  PQXX_CHECK_EQUAL(
    scan(text.c_str(), std::size(text), 0), 3ul,
    "UTF-8 scanner mis-scanned Thai khor khwai.");
  PQXX_CHECK_EQUAL(
    scan(text.c_str(), std::size(text), 3), 6ul,
    "UTF-8 scanner mis-scanned Thai ror reua.");
}


void test_for_glyphs_empty()
{
  bool iterated{false};
  pqxx::internal::for_glyphs(
    pqxx::internal::encoding_group::MONOBYTE,
    [&iterated](char const *, char const *) { iterated = true; }, "", 0);
  PQXX_CHECK(!iterated, "Empty string went through an iteration.");
}


void test_for_glyphs_ascii()
{
  std::string const text{"hi"};
  std::vector<std::ptrdiff_t> points;

  pqxx::internal::for_glyphs(
    pqxx::internal::encoding_group::UTF8,
    [&points](char const *gbegin, char const *gend) {
      points.push_back(gend - gbegin);
    },
    text.c_str(), std::size(text));

  PQXX_CHECK_EQUAL(std::size(points), 2u, "Wrong number of ASCII iterations.");
  PQXX_CHECK_EQUAL(points[0], 1u, "ASCII iteration started off wrong.");
  PQXX_CHECK_EQUAL(points[1], 1u, "ASCII iteration was inconsistent.");
}


void test_for_glyphs_utf8()
{
  // Greek: alpha omega.
  std::string const text{"\xce\x91\xce\xa9"};
  std::vector<std::ptrdiff_t> points;

  pqxx::internal::for_glyphs(
    pqxx::internal::encoding_group::UTF8,
    [&points](char const *gbegin, char const *gend) {
      points.push_back(gend - gbegin);
    },
    text.c_str(), std::size(text));

  PQXX_CHECK_EQUAL(std::size(points), 2u, "Wrong number of UTF-8 iterations.");
  PQXX_CHECK_EQUAL(points[0], 2u, "UTF-8 iteration started off wrong.");
  PQXX_CHECK_EQUAL(points[1], 2u, "ASCII iteration was inconsistent.");

  // Greek lambda, ASCII plus sign, Old Persian Gu.
  std::string const mix{"\xce\xbb+\xf0\x90\x8e\xa6"};
  points.clear();

  pqxx::internal::for_glyphs(
    pqxx::internal::encoding_group::UTF8,
    [&points](char const *gbegin, char const *gend) {
      points.push_back(gend - gbegin);
    },
    mix.c_str(), std::size(mix));

  PQXX_CHECK_EQUAL(std::size(points), 3u, "Mixed UTF-8 iteration is broken.");
  PQXX_CHECK_EQUAL(points[0], 2u, "Mixed UTF-8 iteration started off wrong.");
  PQXX_CHECK_EQUAL(points[1], 1u, "Mixed UTF-8 iteration got ASCII wrong.");
  PQXX_CHECK_EQUAL(
    points[2], 4u, "Mixed UTF-8 iteration got long char wrong.");
}


void test_encodings()
{
  test_scan_ascii();
  test_scan_utf8();
  test_for_glyphs_empty();
  test_for_glyphs_ascii();
  test_for_glyphs_utf8();
}


PQXX_REGISTER_TEST(test_encodings);
} // namespace
