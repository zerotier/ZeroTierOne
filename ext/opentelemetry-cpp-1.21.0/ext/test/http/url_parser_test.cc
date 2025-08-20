// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdint>
#include <ostream>
#include <string>
#include <tuple>

#include "opentelemetry/ext/http/common/url_parser.h"

namespace http_common = opentelemetry::ext::http::common;

struct ParsedUrl
{
  std::string scheme;
  std::string host;
  std::uint16_t port;
  std::string path;
  std::string query;
  bool success;

  friend void PrintTo(const ParsedUrl &p, std::ostream *os)
  {
    *os << "(valid: " << (p.success ? "yes" : "no") << ", scheme: " << p.scheme
        << ", host: " << p.host << ", port: " << p.port << ", path: " << p.path
        << ", query: " << p.query << ")";
  }
};

class UrlParserTests : public testing::TestWithParam<std::tuple<std::string, ParsedUrl>>
{};

INSTANTIATE_TEST_SUITE_P(
    SampleValues,
    UrlParserTests,
    testing::Values(
        std::make_tuple("www.abc.com", ParsedUrl{"http", "www.abc.com", 80, "/", "", true}),
        std::make_tuple("http://www.abc.com", ParsedUrl{"http", "www.abc.com", 80, "/", "", true}),
        std::make_tuple("https://www.abc.com",
                        ParsedUrl{"https", "www.abc.com", 443, "/", "", true}),
        std::make_tuple("https://www.abc.com:4431",
                        ParsedUrl{"https", "www.abc.com", 4431, "/", "", true}),
        std::make_tuple("https://www.abc.com:4431/path1",
                        ParsedUrl{"https", "www.abc.com", 4431, "/path1", "", true}),
        std::make_tuple("https://www.abc.com:4431/path1/path2",
                        ParsedUrl{"https", "www.abc.com", 4431, "/path1/path2", "", true}),
        std::make_tuple("https://www.abc.com/path1/path2",
                        ParsedUrl{"https", "www.abc.com", 443, "/path1/path2", "", true}),
        std::make_tuple("http://www.abc.com/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "www.abc.com", 80, "/path1/path2", "q1=a1&q2=a2", true}),
        std::make_tuple("http://www.abc.com:8080/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "www.abc.com", 8080, "/path1/path2", "q1=a1&q2=a2",
                                  true}),
        std::make_tuple("www.abc.com:8080/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "www.abc.com", 8080, "/path1/path2", "q1=a1&q2=a2",
                                  true}),
        std::make_tuple("http://user:password@www.abc.com:8080/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "www.abc.com", 8080, "/path1/path2", "q1=a1&q2=a2",
                                  true}),
        std::make_tuple("user:password@www.abc.com:8080/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "www.abc.com", 8080, "/path1/path2", "q1=a1&q2=a2",
                                  true}),
        std::make_tuple("https://user@www.abc.com/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"https", "www.abc.com", 443, "/path1/path2", "q1=a1&q2=a2",
                                  true}),
        std::make_tuple("http://www.abc.com/path1@bbb/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "www.abc.com", 80, "/path1@bbb/path2", "q1=a1&q2=a2",
                                  true}),
        std::make_tuple("http://1.2.3.4/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "1.2.3.4", 80, "/path1/path2", "q1=a1&q2=a2", true}),
        std::make_tuple("user:password@1.2.3.4:8080/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "1.2.3.4", 8080, "/path1/path2", "q1=a1&q2=a2", true}),
        std::make_tuple("https://user@1.2.3.4/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"https", "1.2.3.4", 443, "/path1/path2", "q1=a1&q2=a2", true}),
        std::make_tuple("http://1.2.3.4/path1@bbb/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "1.2.3.4", 80, "/path1@bbb/path2", "q1=a1&q2=a2", true}),
        std::make_tuple("http://[fe80::225:93da:bfde:b5f5]/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "[fe80::225:93da:bfde:b5f5]", 80, "/path1/path2",
                                  "q1=a1&q2=a2", true}),
        std::make_tuple("user:password@[fe80::225:93da:bfde:b5f5]:8080/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "[fe80::225:93da:bfde:b5f5]", 8080, "/path1/path2",
                                  "q1=a1&q2=a2", true}),
        std::make_tuple("https://user@[fe80::225:93da:bfde:b5f5]/path1/path2?q1=a1&q2=a2",
                        ParsedUrl{"https", "[fe80::225:93da:bfde:b5f5]", 443, "/path1/path2",
                                  "q1=a1&q2=a2", true}),
        std::make_tuple("http://[fe80::225:93da:bfde:b5f5]/path1@bbb/path2?q1=a1&q2=a2",
                        ParsedUrl{"http", "[fe80::225:93da:bfde:b5f5]", 80, "/path1@bbb/path2",
                                  "q1=a1&q2=a2", true}),
        std::make_tuple("https://https://example.com/some/path",
                        ParsedUrl{"https", "https", 0, "//example.com/some/path", "", false}),
        std::make_tuple("https://example.com:-1/some/path",
                        ParsedUrl{"https", "example.com", 0, "/some/path", "", false}),
        std::make_tuple("https://example.com:65536/some/path",
                        ParsedUrl{"https", "example.com", 0, "/some/path", "", false}),
        std::make_tuple("https://example.com:80a/some/path",
                        ParsedUrl{"https", "example.com", 0, "/some/path", "", false}),
        std::make_tuple("https://example.com:18446744073709551616/some/path",
                        ParsedUrl{"https", "example.com", 0, "/some/path", "", false})));

TEST_P(UrlParserTests, BasicTests)
{
  const auto &url      = std::get<0>(GetParam());
  const auto &expected = std::get<1>(GetParam());

  const auto actual = http_common::UrlParser(url);

  EXPECT_EQ(actual.success_, expected.success);
  EXPECT_EQ(actual.host_, expected.host);
  EXPECT_EQ(actual.port_, expected.port);
  EXPECT_EQ(actual.scheme_, expected.scheme);
  EXPECT_EQ(actual.path_, expected.path);
  EXPECT_EQ(actual.query_, expected.query);
}

class UrlDecoderTests : public ::testing::TestWithParam<std::tuple<std::string, std::string>>
{};

INSTANTIATE_TEST_SUITE_P(
    SampleValues,
    UrlDecoderTests,
    testing::Values(std::make_tuple("Authentication=Basic xxx", "Authentication=Basic xxx"),
                    std::make_tuple("Authentication=Basic%20xxx", "Authentication=Basic xxx"),
                    std::make_tuple("%C3%B6%C3%A0%C2%A7%C3%96abcd%C3%84",
                                    "\xc3\xb6\xc3\xa0\xc2\xa7\xc3\x96\x61\x62\x63\x64\xc3\x84"),
                    std::make_tuple("%2x", "%2x"),
                    std::make_tuple("%20", " "),
                    std::make_tuple("text%2", "text%2"),
                    std::make_tuple("%20test%zztest", "%20test%zztest"),
                    std::make_tuple("%20test%2", "%20test%2")));

TEST_P(UrlDecoderTests, BasicTests)
{
  const auto &encoded  = std::get<0>(GetParam());
  const auto &expected = std::get<1>(GetParam());
  const auto actual    = http_common::UrlDecoder::Decode(encoded);

  EXPECT_EQ(actual, expected);
}
