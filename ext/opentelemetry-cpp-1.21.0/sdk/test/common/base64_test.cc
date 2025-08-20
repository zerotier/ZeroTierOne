// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/common/base64.h"

#include <string>

#include <gtest/gtest.h>

static const unsigned char base64_test_dec[64] = {
    0x24, 0x48, 0x6E, 0x56, 0x87, 0x62, 0x5A, 0xBD, 0xBF, 0x17, 0xD9, 0xA2, 0xC4, 0x17, 0x1A, 0x01,
    0x94, 0xED, 0x8F, 0x1E, 0x11, 0xB3, 0xD7, 0x09, 0x0C, 0xB6, 0xE9, 0x10, 0x6F, 0x22, 0xEE, 0x13,
    0xCA, 0xB3, 0x07, 0x05, 0x76, 0xC9, 0xFA, 0x31, 0x6C, 0x08, 0x34, 0xFF, 0x8D, 0xC2, 0x6C, 0x38,
    0x00, 0x43, 0xE9, 0x54, 0x97, 0xAF, 0x50, 0x4B, 0xD1, 0x41, 0xBA, 0x95, 0x31, 0x5A, 0x0B, 0x97};

static const unsigned char base64_test_enc_rfc_2045[] =
    "JEhuVodiWr2/F9mixBcaAZTtjx4Rs9cJDLbpEG8i7hPK"
    "swcFdsn6MWwINP+Nwmw4AEPpVJevUEvRQbqVMVoLlw==";

TEST(Base64Test, EscapeRfc2045)
{
  std::string encoded = opentelemetry::sdk::common::Base64Escape(opentelemetry::nostd::string_view{
      reinterpret_cast<const char *>(base64_test_dec), sizeof(base64_test_dec)});
  opentelemetry::nostd::string_view expected{
      reinterpret_cast<const char *>(base64_test_enc_rfc_2045), 88};

  EXPECT_EQ(encoded, expected);
}

TEST(Base64Test, UnescapeRfc2045)
{
  std::string decoded;
  EXPECT_TRUE(opentelemetry::sdk::common::Base64Unescape(
      opentelemetry::nostd::string_view{reinterpret_cast<const char *>(base64_test_enc_rfc_2045),
                                        88},
      &decoded));
  opentelemetry::nostd::string_view expected{reinterpret_cast<const char *>(base64_test_dec),
                                             sizeof(base64_test_dec)};

  EXPECT_EQ(decoded, expected);
}

TEST(Base64Test, UnescapeRfc2045InvalidInput)
{
  std::string std_str_out;
  std::string std_str_in = "=";

  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));

  std_str_in = "J=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "JEhu=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "JEhuV=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "JEhuVodi=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "JEhuVodiW=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));

  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "  J=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "  JEhu=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "  JEhuV=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "  JEhuVodi=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "  JEhuVodiW=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));

  std_str_in = "JE==";
  EXPECT_TRUE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "JE==huVo";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = "JE=huVo=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));

  std_str_in = " JE==";
  EXPECT_TRUE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = " JE==huVo";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
  std_str_in = " JE=huVo=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));

  std_str_in = "JE==\r\n";
  EXPECT_TRUE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));

  std_str_in = "J E=";
  EXPECT_FALSE(opentelemetry::sdk::common::Base64Unescape(std_str_in, &std_str_out));
}
