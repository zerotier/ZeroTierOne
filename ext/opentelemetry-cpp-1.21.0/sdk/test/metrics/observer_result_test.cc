// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/sdk/metrics/observer_result.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"

using namespace opentelemetry::sdk::metrics;
TEST(ObserverResult, BasicTests)
{
  const AttributesProcessor *attributes_processor = new DefaultAttributesProcessor();

  ObserverResultT<int64_t> observer_result(attributes_processor);

  observer_result.Observe(10);
  observer_result.Observe(20);
  EXPECT_EQ(observer_result.GetMeasurements().size(), 1);

  std::map<std::string, int64_t> m1 = {{"k2", 12}};
  observer_result.Observe(
      30, opentelemetry::common::KeyValueIterableView<std::map<std::string, int64_t>>(m1));
  EXPECT_EQ(observer_result.GetMeasurements().size(), 2);

  observer_result.Observe(
      40, opentelemetry::common::KeyValueIterableView<std::map<std::string, int64_t>>(m1));
  EXPECT_EQ(observer_result.GetMeasurements().size(), 2);

  std::map<std::string, int64_t> m2 = {{"k2", 12}, {"k4", 12}};
  observer_result.Observe(
      40, opentelemetry::common::KeyValueIterableView<std::map<std::string, int64_t>>(m2));
  EXPECT_EQ(observer_result.GetMeasurements().size(), 3);

  delete attributes_processor;
}
