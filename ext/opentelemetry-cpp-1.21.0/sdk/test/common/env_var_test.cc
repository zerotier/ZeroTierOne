// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "opentelemetry/sdk/common/env_variables.h"

#if defined(_MSC_VER)
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#endif

using opentelemetry::sdk::common::GetBoolEnvironmentVariable;
using opentelemetry::sdk::common::GetDurationEnvironmentVariable;
using opentelemetry::sdk::common::GetFloatEnvironmentVariable;
using opentelemetry::sdk::common::GetStringEnvironmentVariable;
using opentelemetry::sdk::common::GetUintEnvironmentVariable;

#ifndef NO_GETENV
TEST(EnvVarTest, BoolEnvVar)
{
  unsetenv("BOOL_ENV_VAR_NONE");
  setenv("BOOL_ENV_VAR_EMPTY", "", 1);
  setenv("BOOL_ENV_VAR_T1", "true", 1);
  setenv("BOOL_ENV_VAR_T2", "TRUE", 1);
  setenv("BOOL_ENV_VAR_T3", "TrUe", 1);
  setenv("BOOL_ENV_VAR_F1", "false", 1);
  setenv("BOOL_ENV_VAR_F2", "FALSE", 1);
  setenv("BOOL_ENV_VAR_F3", "FaLsE", 1);
  setenv("BOOL_ENV_VAR_BROKEN", "Maybe ?", 1);

  bool exists;
  bool value = true;

  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_NONE", value);
  EXPECT_FALSE(exists);

  value  = true;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_EMPTY", value);
  EXPECT_FALSE(exists);
  EXPECT_FALSE(value);

  value  = false;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_T1", value);
  EXPECT_TRUE(exists);
  EXPECT_TRUE(value);

  value  = false;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_T2", value);
  EXPECT_TRUE(exists);
  EXPECT_TRUE(value);

  value  = false;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_T3", value);
  EXPECT_TRUE(exists);
  EXPECT_TRUE(value);

  value  = true;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_F1", value);
  EXPECT_TRUE(exists);
  EXPECT_FALSE(value);

  value  = true;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_F2", value);
  EXPECT_TRUE(exists);
  EXPECT_FALSE(value);

  value  = true;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_F3", value);
  EXPECT_TRUE(exists);
  EXPECT_FALSE(value);

  // This raises a warning, not verifying the warning text.
  value  = true;
  exists = GetBoolEnvironmentVariable("BOOL_ENV_VAR_BROKEN", value);
  EXPECT_TRUE(exists);
  EXPECT_FALSE(value);

  unsetenv("BOOL_ENV_VAR_EMPTY");
  unsetenv("BOOL_ENV_VAR_T1");
  unsetenv("BOOL_ENV_VAR_T2");
  unsetenv("BOOL_ENV_VAR_T3");
  unsetenv("BOOL_ENV_VAR_F1");
  unsetenv("BOOL_ENV_VAR_F2");
  unsetenv("BOOL_ENV_VAR_F3");
  unsetenv("BOOL_ENV_VAR_BROKEN");
}

TEST(EnvVarTest, StringEnvVar)
{
  unsetenv("STRING_ENV_VAR_NONE");
  setenv("STRING_ENV_VAR_EMPTY", "", 1);
  setenv("STRING_ENV_VAR", "my string", 1);

  bool exists;
  std::string value;

  exists = GetStringEnvironmentVariable("STRING_ENV_VAR_NONE", value);
  EXPECT_FALSE(exists);

  exists = GetStringEnvironmentVariable("STRING_ENV_VAR_EMPTY", value);
  EXPECT_FALSE(exists);
  EXPECT_EQ(value, "");

  value  = "poison";
  exists = GetStringEnvironmentVariable("STRING_ENV_VAR", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, "my string");

  unsetenv("STRING_ENV_VAR_EMPTY");
  unsetenv("STRING_ENV_VAR");
}

TEST(EnvVarTest, DurationEnvVar)
{
  unsetenv("DURATION_ENV_VAR_NONE");
  setenv("DURATION_ENV_VAR_EMPTY", "", 1);
  setenv("DURATION_ENV_VAR_1", "10ns", 1);
  setenv("DURATION_ENV_VAR_2", "20us", 1);
  setenv("DURATION_ENV_VAR_3", "30ms", 1);
  setenv("DURATION_ENV_VAR_4", "40s", 1);
  setenv("DURATION_ENV_VAR_5", "50m", 1);
  setenv("DURATION_ENV_VAR_6", "60h", 1);
  setenv("DURATION_ENV_VAR_7", "123", 1);
  setenv("DURATION_ENV_VAR_BROKEN_1", "123 ms", 1);
  setenv("DURATION_ENV_VAR_BROKEN_2", "1mississippi", 1);

  bool exists;
  std::chrono::system_clock::duration poison;
  std::chrono::system_clock::duration value;
  std::chrono::system_clock::duration expected;

  poison =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{666});

  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_NONE", value);
  EXPECT_FALSE(exists);

  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_EMPTY", value);
  EXPECT_FALSE(exists);

  value = poison;
  expected =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds{10});
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_1", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, expected);

  value    = poison;
  expected = std::chrono::duration_cast<std::chrono::system_clock::duration>(
      std::chrono::microseconds{20});
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_2", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, expected);

  value    = poison;
  expected = std::chrono::duration_cast<std::chrono::system_clock::duration>(
      std::chrono::milliseconds{30});
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_3", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, expected);

  value = poison;
  expected =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{40});
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_4", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, expected);

  value = poison;
  expected =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::minutes{50});
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_5", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, expected);

  value = poison;
  expected =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::hours{60});
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_6", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, expected);

  value = poison;
  // Deviation from the spec: 123 seconds instead of 123 milliseconds
  expected =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{123});
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_7", value);
  EXPECT_TRUE(exists);
  EXPECT_EQ(value, expected);

  // This raises a warning, not verifying the warning text.
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_BROKEN_1", value);
  EXPECT_FALSE(exists);

  // This raises a warning, not verifying the warning text.
  exists = GetDurationEnvironmentVariable("DURATION_ENV_VAR_BROKEN_2", value);
  EXPECT_FALSE(exists);

  unsetenv("STRING_ENV_VAR_EMPTY");
  unsetenv("STRING_ENV_VAR_1");
  unsetenv("STRING_ENV_VAR_2");
  unsetenv("STRING_ENV_VAR_3");
  unsetenv("STRING_ENV_VAR_4");
  unsetenv("STRING_ENV_VAR_5");
  unsetenv("STRING_ENV_VAR_6");
  unsetenv("STRING_ENV_VAR_7");
  unsetenv("STRING_ENV_VAR_BROKEN_1");
  unsetenv("STRING_ENV_VAR_BROKEN_2");
}

TEST(EnvVarTest, UintEnvVar)
{
  unsetenv("UINT_ENV_VAR_NONE");
  setenv("UINT_ENV_VAR_EMPTY", "", 1);
  setenv("UINT_ENV_VAR_POSITIVE_INT", "42", 1);
  setenv("UINT_ENV_VAR_NEGATIVE_INT", "-42", 1);
  setenv("UINT_ENV_VAR_POSITIVE_DEC", "12.34", 1);
  setenv("UINT_ENV_VAR_NEGATIVE_DEC", "-12.34", 1);
  setenv("UINT_ENV_VAR_POSITIVE_INT_MAX", "4294967295", 1);
  setenv("UINT_ENV_VAR_POSITIVE_OVERFLOW", "4294967296", 1);
  setenv("UINT_ENV_VAR_NEGATIVE_INT_MIN", "-2147483648", 1);
  setenv("UINT_ENV_VAR_NEGATIVE_OVERFLOW", "-4294967296", 1);
  setenv("UINT_ENV_VAR_TOO_LARGE_INT", "99999999999999999999", 1);
  setenv("UINT_ENV_VAR_TOO_LARGE_DEC", "3.9999e+99", 1);
  setenv("UINT_ENV_VAR_WITH_NOISE", "   \t \n 9.12345678.9", 1);
  setenv("UINT_ENV_VAR_ONLY_SPACES", "   ", 1);

  std::uint32_t value;

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_NONE", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_EMPTY", value));

  ASSERT_TRUE(GetUintEnvironmentVariable("UINT_ENV_VAR_POSITIVE_INT", value));
  ASSERT_EQ(42, value);

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_NEGATIVE_INT", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_POSITIVE_DEC", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_NEGATIVE_DEC", value));

  ASSERT_TRUE(GetUintEnvironmentVariable("UINT_ENV_VAR_POSITIVE_INT_MAX", value));
  ASSERT_EQ(4294967295, value);

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_POSITIVE_OVERFLOW", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_NEGATIVE_INT_MIN", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_NEGATIVE_OVERFLOW", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_TOO_LARGE_INT", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_TOO_LARGE_DEC", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_WITH_NOISE", value));

  ASSERT_FALSE(GetUintEnvironmentVariable("UINT_ENV_VAR_ONLY_SPACES", value));

  unsetenv("UINT_ENV_VAR_EMPTY");
  unsetenv("UINT_ENV_VAR_POSITIVE_INT");
  unsetenv("UINT_ENV_VAR_NEGATIVE_INT");
  unsetenv("UINT_ENV_VAR_POSITIVE_DEC");
  unsetenv("UINT_ENV_VAR_NEGATIVE_DEC");
  unsetenv("UINT_ENV_VAR_POSITIVE_INT_MAX");
  unsetenv("UINT_ENV_VAR_POSITIVE_OVERFLOW");
  unsetenv("UINT_ENV_VAR_NEGATIVE_INT_MIN");
  unsetenv("UINT_ENV_VAR_NEGATIVE_OVERFLOW");
  unsetenv("UINT_ENV_VAR_TOO_LARGE_INT");
  unsetenv("UINT_ENV_VAR_TOO_LARGE_DEC");
  unsetenv("UINT_ENV_VAR_WITH_NOISE");
  unsetenv("UINT_ENV_VAR_ONLY_SPACES");
}

TEST(EnvVarTest, FloatEnvVar)
{
  unsetenv("FLOAT_ENV_VAR_NONE");
  setenv("FLOAT_ENV_VAR_EMPTY", "", 1);
  setenv("FLOAT_ENV_VAR_POSITIVE_INT", "42", 1);
  setenv("FLOAT_ENV_VAR_NEGATIVE_INT", "-42", 1);
  setenv("FLOAT_ENV_VAR_POSITIVE_DEC", "12.34", 1);
  setenv("FLOAT_ENV_VAR_NEGATIVE_DEC", "-12.34", 1);
  setenv("FLOAT_ENV_VAR_POSITIVE_INT_MAX", "4294967295", 1);
  setenv("FLOAT_ENV_VAR_POSITIVE_OVERFLOW", "4294967296", 1);
  setenv("FLOAT_ENV_VAR_NEGATIVE_INT_MIN", "-2147483648", 1);
  setenv("FLOAT_ENV_VAR_NEGATIVE_OVERFLOW", "-4294967296", 1);
  setenv("FLOAT_ENV_VAR_TOO_LARGE_INT", "99999999999999999999", 1);
  setenv("FLOAT_ENV_VAR_TOO_LARGE_DEC", "3.9999e+99", 1);
  setenv("FLOAT_ENV_VAR_WITH_NOISE", "   \t \n 9.12345678.9", 1);
  setenv("FLOAT_ENV_VAR_ONLY_SPACES", "   ", 1);

  float value;

  ASSERT_FALSE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_NONE", value));

  ASSERT_FALSE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_EMPTY", value));

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_POSITIVE_INT", value));
  ASSERT_FLOAT_EQ(42.f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_NEGATIVE_INT", value));
  ASSERT_FLOAT_EQ(-42.f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_POSITIVE_DEC", value));
  ASSERT_FLOAT_EQ(12.34f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_NEGATIVE_DEC", value));
  ASSERT_FLOAT_EQ(-12.34f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_POSITIVE_INT_MAX", value));
  ASSERT_FLOAT_EQ(4294967295.f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_POSITIVE_OVERFLOW", value));
  ASSERT_FLOAT_EQ(4294967296.f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_NEGATIVE_INT_MIN", value));
  ASSERT_FLOAT_EQ(-2147483648.f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_NEGATIVE_OVERFLOW", value));
  ASSERT_FLOAT_EQ(-4294967296.f, value);

  ASSERT_TRUE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_TOO_LARGE_INT", value));
  ASSERT_FLOAT_EQ(99999999999999999999.f, value);

  ASSERT_FALSE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_TOO_LARGE_DEC", value));

  ASSERT_FALSE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_WITH_NOISE", value));

  ASSERT_FALSE(GetFloatEnvironmentVariable("FLOAT_ENV_VAR_ONLY_SPACES", value));

  unsetenv("FLOAT_ENV_VAR_EMPTY");
  unsetenv("FLOAT_ENV_VAR_POSITIVE_INT");
  unsetenv("FLOAT_ENV_VAR_NEGATIVE_INT");
  unsetenv("FLOAT_ENV_VAR_POSITIVE_DEC");
  unsetenv("FLOAT_ENV_VAR_NEGATIVE_DEC");
  unsetenv("FLOAT_ENV_VAR_POSITIVE_INT_MAX");
  unsetenv("FLOAT_ENV_VAR_POSITIVE_OVERFLOW");
  unsetenv("FLOAT_ENV_VAR_NEGATIVE_INT_MIN");
  unsetenv("FLOAT_ENV_VAR_NEGATIVE_OVERFLOW");
  unsetenv("FLOAT_ENV_VAR_TOO_LARGE_INT");
  unsetenv("FLOAT_ENV_VAR_TOO_LARGE_DEC");
  unsetenv("FLOAT_ENV_VAR_WITH_NOISE");
  unsetenv("FLOAT_ENV_VAR_ONLY_SPACES");
}

#endif  // NO_GETENV
