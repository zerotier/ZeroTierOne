// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/trace/tracer_config.h"
#include <gtest/gtest.h>
#include <array>
#include <string>
#include <utility>
#include <vector>
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"

namespace trace_sdk             = opentelemetry::sdk::trace;
namespace instrumentation_scope = opentelemetry::sdk::instrumentationscope;

/** Tests to verify the basic behavior of trace_sdk::TracerConfig */

TEST(TracerConfig, CheckDisabledWorksAsExpected)
{
  trace_sdk::TracerConfig disabled_config = trace_sdk::TracerConfig::Disabled();
  ASSERT_FALSE(disabled_config.IsEnabled());
}

TEST(TracerConfig, CheckEnabledWorksAsExpected)
{
  trace_sdk::TracerConfig enabled_config = trace_sdk::TracerConfig::Enabled();
  ASSERT_TRUE(enabled_config.IsEnabled());
}

TEST(TracerConfig, CheckDefaultConfigWorksAccToSpec)
{
  trace_sdk::TracerConfig default_config = trace_sdk::TracerConfig::Default();
  ASSERT_TRUE(default_config.IsEnabled());
}

/** Tests to verify the behavior of trace_sdk::TracerConfig::DefaultConfigurator */

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
class DefaultTracerConfiguratorTestFixture
    : public ::testing::TestWithParam<instrumentation_scope::InstrumentationScope *>
{};

// verifies that the default configurator always returns the default tracer config
TEST_P(DefaultTracerConfiguratorTestFixture, VerifyDefaultConfiguratorBehavior)
{
  instrumentation_scope::InstrumentationScope *scope = GetParam();
  instrumentation_scope::ScopeConfigurator<trace_sdk::TracerConfig> default_configurator =
      instrumentation_scope::ScopeConfigurator<trace_sdk::TracerConfig>::Builder(
          trace_sdk::TracerConfig::Default())
          .Build();

  ASSERT_EQ(default_configurator.ComputeConfig(*scope), trace_sdk::TracerConfig::Default());
}

INSTANTIATE_TEST_SUITE_P(InstrumentationScopes,
                         DefaultTracerConfiguratorTestFixture,
                         ::testing::ValuesIn(instrumentation_scopes));
