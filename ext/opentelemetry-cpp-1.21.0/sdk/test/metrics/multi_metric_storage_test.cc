// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/metrics/state/metric_storage.h"
#include "opentelemetry/sdk/metrics/state/multi_metric_storage.h"

#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
#  include "opentelemetry/sdk/metrics/exemplar/no_exemplar_reservoir.h"
#endif  // ENABLE_METRICS_EXEMPLAR_PREVIEW

using namespace opentelemetry;
using namespace opentelemetry::sdk::metrics;

class TestMetricStorage : public SyncWritableMetricStorage
{
public:
  void RecordLong(int64_t /* value */,
                  const opentelemetry::context::Context & /* context */) noexcept override
  {
    num_calls_long++;
  }

  void RecordLong(int64_t /* value */,
                  const opentelemetry::common::KeyValueIterable & /* attributes */,
                  const opentelemetry::context::Context & /* context */) noexcept override
  {
    num_calls_long++;
  }

  void RecordDouble(double /* value */,
                    const opentelemetry::context::Context & /* context */) noexcept override
  {
    num_calls_double++;
  }

  void RecordDouble(double /* value */,
                    const opentelemetry::common::KeyValueIterable & /* attributes */,
                    const opentelemetry::context::Context & /* context */) noexcept override
  {
    num_calls_double++;
  }

  size_t num_calls_long;
  size_t num_calls_double;
};

TEST(MultiMetricStorageTest, BasicTests)
{
  std::shared_ptr<opentelemetry::sdk::metrics::SyncWritableMetricStorage> storage(
      new TestMetricStorage());
  SyncMultiMetricStorage storages{};
  storages.AddStorage(storage);
  storages.RecordLong(10, opentelemetry::context::Context{});
  storages.RecordLong(20, opentelemetry::context::Context{});

  storages.RecordDouble(10.0, opentelemetry::context::Context{});
  storages.RecordLong(30, opentelemetry::context::Context{});

  EXPECT_EQ(static_cast<TestMetricStorage *>(storage.get())->num_calls_long, 3);
  EXPECT_EQ(static_cast<TestMetricStorage *>(storage.get())->num_calls_double, 1);
}
