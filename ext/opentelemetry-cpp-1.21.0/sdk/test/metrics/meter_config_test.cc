// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/metrics/meter_config.h"
#include <gtest/gtest.h>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"

namespace metrics_sdk           = opentelemetry::sdk::metrics;
namespace instrumentation_scope = opentelemetry::sdk::instrumentationscope;

/** Test to verify the basic behavior of metrics_sdk::MeterConfig */

TEST(MeterConfig, CheckDisabledWorksAsExpected)
{
  metrics_sdk::MeterConfig disabled_config = metrics_sdk::MeterConfig::Disabled();
  ASSERT_FALSE(disabled_config.IsEnabled());
}

TEST(MeterConfig, CheckEnabledWorksAsExpected)
{
  metrics_sdk::MeterConfig enabled_config = metrics_sdk::MeterConfig::Enabled();
  ASSERT_TRUE(enabled_config.IsEnabled());
}

TEST(MeterConfig, CheckDefaultConfigWorksAccToSpec)
{
  metrics_sdk::MeterConfig enabled_config = metrics_sdk::MeterConfig::Default();
  ASSERT_TRUE(enabled_config.IsEnabled());
}

/** Tests to verify the behavior of metrics_sdk::MeterConfig::Default */

static std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> attr1 = {
    "accept_single_attr", true};
static std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> attr2 = {
    "accept_second_attr", "some other attr"};
static std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> attr3 = {
    "accept_third_attr", 3};

static instrumentation_scope::InstrumentationScope test_scope_1 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_1");
static instrumentation_scope::InstrumentationScope test_scope_2 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_2", "1.0");
static instrumentation_scope::InstrumentationScope test_scope_3 =
    *instrumentation_scope::InstrumentationScope::Create(
        "test_scope_3",
        "0",
        "https://opentelemetry.io/schemas/v1.18.0");
static instrumentation_scope::InstrumentationScope test_scope_4 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_4",
                                                         "0",
                                                         "https://opentelemetry.io/schemas/v1.18.0",
                                                         {attr1});
static instrumentation_scope::InstrumentationScope test_scope_5 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_5",
                                                         "0",
                                                         "https://opentelemetry.io/schemas/v1.18.0",
                                                         {attr1, attr2, attr3});

// This array could also directly contain the reference types, but that  leads to 'uninitialized
// value was created by heap allocation' errors in Valgrind memcheck. This is a bug in Googletest
// library, see https://github.com/google/googletest/issues/3805#issuecomment-1397301790 for more
// details. Using pointers is a workaround to prevent the Valgrind warnings.
const std::array<instrumentation_scope::InstrumentationScope *, 5> instrumentation_scopes = {
    &test_scope_1, &test_scope_2, &test_scope_3, &test_scope_4, &test_scope_5,
};

// Test fixture for VerifyDefaultConfiguratorBehavior
class DefaultMeterConfiguratorTestFixture
    : public ::testing::TestWithParam<instrumentation_scope::InstrumentationScope *>
{};

// verifies that the default configurator always returns the default meter config
TEST_P(DefaultMeterConfiguratorTestFixture, VerifyDefaultConfiguratorBehavior)
{
  instrumentation_scope::InstrumentationScope *scope = GetParam();
  instrumentation_scope::ScopeConfigurator<metrics_sdk::MeterConfig> default_configurator =
      instrumentation_scope::ScopeConfigurator<metrics_sdk::MeterConfig>::Builder(
          metrics_sdk::MeterConfig::Default())
          .Build();

  ASSERT_EQ(default_configurator.ComputeConfig(*scope), metrics_sdk::MeterConfig::Default());
}

INSTANTIATE_TEST_SUITE_P(InstrumentationScopes,
                         DefaultMeterConfiguratorTestFixture,
                         ::testing::ValuesIn(instrumentation_scopes));
