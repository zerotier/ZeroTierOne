// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "opentelemetry/sdk/metrics/instrument_metadata_validator.h"

static std::string CreateVeryLargeString(size_t multiple)
{
  std::string result   = "START";
  std::string repeater = "0123456789";
  for (size_t i = 0; i < multiple; i++)
  {
    result += repeater;
  }
  return result;
}

TEST(InstrumentMetadataValidator, TestName)
{
  opentelemetry::sdk::metrics::InstrumentMetaDataValidator validator;
  std::vector<std::string> invalid_names = {
      "",      // empty string
      "1sdf",  // string starting with number
      "\x31\x32\x33\xe2\x82\xac\x41\x41\x41\xe2\x82\xac\x42\x42\x42",  // unicode characters
      "/\\sdsd",                        // string starting with special character
      "***sSSs",                        // string starting with special character
      "a\\broken\\path",                // contains backward slash
      CreateVeryLargeString(25) + "X",  // total 256 characters
      CreateVeryLargeString(26),        // string much bigger than 255 character
  };
  for (auto const &str : invalid_names)
  {
    EXPECT_FALSE(validator.ValidateName(str));
  }

  std::vector<std::string> valid_names = {
      "T",                                      // single char string
      "s123",                                   // starting with char, followed by numbers
      "dsdsdsd_-.",                             // string , and valid nonalphanumeric
      "d1234_-sDSDs.sdsd344",                   // combination of all valid characters
      "a/path/to/some/metric",                  // contains forward slash
      CreateVeryLargeString(5) + "ABCERTYG",    // total 63 characters
      CreateVeryLargeString(5) + "ABCERTYGJ",   // total 64 characters
      CreateVeryLargeString(24) + "ABCDEFGHI",  // total 254 characters
      CreateVeryLargeString(25),                // total 255 characters
  };
  for (auto const &str : valid_names)
  {
    EXPECT_TRUE(validator.ValidateName(str));
  }
}

TEST(InstrumentMetadataValidator, TestUnit)
{
  opentelemetry::sdk::metrics::InstrumentMetaDataValidator validator;
  std::vector<std::string> invalid_units = {
      CreateVeryLargeString(5) + "ABCERTYGJ",  // total 64 charactes
      CreateVeryLargeString(7),                // string bigger than 63 chars
      "\x31\x32\x33\xe2\x82\xac\x41\x41\x41\xe2\x82\xac\x42\x42\x42",  // unicode string
  };
  for (auto const &str : invalid_units)
  {
    EXPECT_FALSE(validator.ValidateUnit(str));
  }

  std::vector<std::string> valid_units = {
      "T",                                    // single char
      "s123",                                 // starting with char, followed by numbers
      "dsdsdsd_-.",                           // string , and valid nonalphanumeric
      "d1234_-sdsds.sdsd344",                 // combination of all valid characters
      CreateVeryLargeString(5) + "ABCERTYG",  // total 63 charactes
      "ASDDSDF",                              // uppercase
  };
  for (auto const &str : valid_units)
  {
    EXPECT_TRUE(validator.ValidateName(str));
  }
}
