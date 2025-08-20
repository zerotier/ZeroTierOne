// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <string>

#include "opentelemetry/sdk/metrics/instruments.h"

using namespace opentelemetry::sdk::metrics;

InstrumentDescriptor CreateInstrumentDescriptor(
    const std::string &name        = "counter",
    const std::string &description = "description",
    const std::string &unit        = "unit",
    InstrumentType type            = InstrumentType::kCounter,
    InstrumentValueType value_type = InstrumentValueType::kLong)
{
  return {name, description, unit, type, value_type};
}

TEST(InstrumentDescriptorUtilTest, CaseInsensitiveAsciiEquals)
{
  // same name
  EXPECT_TRUE(InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals("counter", "counter"));

  // same case-insensitive name
  EXPECT_TRUE(InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals("counter", "COUNTer"));
  EXPECT_TRUE(InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals("CountER", "counter"));

  // different case-insensitive name same string length
  EXPECT_FALSE(InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals("Counter_1", "counter_2"));
  EXPECT_FALSE(InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals("counter_1", "counter_2"));

  // different case-sensitive name different string length
  EXPECT_FALSE(InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals("counter", "Counter1"));
  EXPECT_FALSE(InstrumentDescriptorUtil::CaseInsensitiveAsciiEquals("Counter1", "counter"));
}

// The following tests cover the spec requirements on detecting identical and duplicate instruments
// https://github.com/open-telemetry/opentelemetry-specification/blob/9c8c30631b0e288de93df7452f91ed47f6fba330/specification/metrics/sdk.md?plain=1#L869
TEST(InstrumentDescriptorUtilTest, IsDuplicate)
{
  auto instrument_existing = CreateInstrumentDescriptor("counter");

  // not a duplicate - different name
  auto instrument_different_name  = instrument_existing;
  instrument_different_name.name_ = "another_name";
  EXPECT_FALSE(
      InstrumentDescriptorUtil::IsDuplicate(instrument_existing, instrument_different_name));

  // not a duplicate - identical instrument
  auto instrument_identical = instrument_existing;
  EXPECT_FALSE(InstrumentDescriptorUtil::IsDuplicate(instrument_existing, instrument_identical));

  // not a duplicate - instrument with same case-insensitive name
  auto instrument_same_name_case_insensitive  = instrument_existing;
  instrument_same_name_case_insensitive.name_ = "COUNTER";
  EXPECT_FALSE(InstrumentDescriptorUtil::IsDuplicate(instrument_existing,
                                                     instrument_same_name_case_insensitive));

  // is duplicate by description
  auto instrument_different_desc         = instrument_existing;
  instrument_different_desc.description_ = "another_description";
  EXPECT_TRUE(
      InstrumentDescriptorUtil::IsDuplicate(instrument_existing, instrument_different_desc));

  // is duplicate by unit
  auto instrument_different_unit  = instrument_existing;
  instrument_different_unit.unit_ = "another_unit";
  EXPECT_TRUE(
      InstrumentDescriptorUtil::IsDuplicate(instrument_existing, instrument_different_unit));

  // is duplicate by kind - instrument type
  auto instrument_different_type  = instrument_existing;
  instrument_different_type.type_ = InstrumentType::kHistogram;
  EXPECT_TRUE(
      InstrumentDescriptorUtil::IsDuplicate(instrument_existing, instrument_different_type));

  // is duplicate by kind - instrument value_type
  auto instrument_different_valuetype        = instrument_existing;
  instrument_different_valuetype.value_type_ = InstrumentValueType::kDouble;
  EXPECT_TRUE(
      InstrumentDescriptorUtil::IsDuplicate(instrument_existing, instrument_different_valuetype));
}

TEST(InstrumentDescriptorTest, EqualNameCaseInsensitiveOperator)
{
  // equal by name, description, unit, type and value type
  InstrumentEqualNameCaseInsensitive equal_operator{};
  auto instrument_existing  = CreateInstrumentDescriptor("counter");
  auto instrument_identical = instrument_existing;
  EXPECT_TRUE(equal_operator(instrument_existing, instrument_identical));

  // equal by name with different case
  auto instrument_name_case_conflict  = instrument_existing;
  instrument_name_case_conflict.name_ = "COUNTER";
  EXPECT_TRUE(equal_operator(instrument_existing, instrument_name_case_conflict));

  // not equal by name
  auto instrument_different_name  = instrument_existing;
  instrument_different_name.name_ = "another_counter";
  EXPECT_FALSE(equal_operator(instrument_existing, instrument_different_name));

  // not equal by instrument value type
  auto instrument_different_valuetype        = instrument_existing;
  instrument_different_valuetype.value_type_ = InstrumentValueType::kDouble;
  EXPECT_FALSE(equal_operator(instrument_existing, instrument_different_valuetype));

  // not equal by instrument type
  auto instrument_different_type  = instrument_existing;
  instrument_different_type.type_ = InstrumentType::kObservableCounter;
  EXPECT_FALSE(equal_operator(instrument_existing, instrument_different_type));

  // not equal by description
  auto instrument_different_desc         = instrument_existing;
  instrument_different_desc.description_ = "another description";
  EXPECT_FALSE(equal_operator(instrument_existing, instrument_different_desc));

  // not equal by unit
  auto instrument_different_unit  = instrument_existing;
  instrument_different_unit.unit_ = "another unit";
  EXPECT_FALSE(equal_operator(instrument_existing, instrument_different_unit));
}

TEST(InstrumentDescriptorTest, HashOperator)
{
  InstrumentDescriptorHash hash_operator{};

  // identical instrument - hash must match
  auto instrument_existing  = CreateInstrumentDescriptor("counter");
  auto instrument_identical = instrument_existing;
  EXPECT_EQ(hash_operator(instrument_existing), hash_operator(instrument_identical));

  // name case conflict - hash must match
  auto instrument_name_case_conflict  = instrument_existing;
  instrument_name_case_conflict.name_ = "COUNTER";
  EXPECT_EQ(hash_operator(instrument_existing), hash_operator(instrument_name_case_conflict));

  // different name
  auto instrument_different_name  = instrument_existing;
  instrument_different_name.name_ = "another_counter";
  EXPECT_NE(hash_operator(instrument_existing), hash_operator(instrument_different_name));

  // different kind - instrument value type
  auto instrument_different_valuetype        = instrument_existing;
  instrument_different_valuetype.value_type_ = InstrumentValueType::kFloat;
  EXPECT_NE(hash_operator(instrument_existing), hash_operator(instrument_different_valuetype));

  // different kind - instrument type
  auto instrument_different_type  = instrument_existing;
  instrument_different_type.type_ = InstrumentType::kObservableUpDownCounter;
  EXPECT_NE(hash_operator(instrument_existing), hash_operator(instrument_different_type));

  // different description
  auto instrument_different_desc         = instrument_existing;
  instrument_different_desc.description_ = "another description";
  EXPECT_NE(hash_operator(instrument_existing), hash_operator(instrument_different_desc));

  // different unit
  auto instrument_different_unit  = instrument_existing;
  instrument_different_unit.unit_ = "another unit";
  EXPECT_NE(hash_operator(instrument_existing), hash_operator(instrument_different_unit));
}
