// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/state/metric_storage.h"
#include "opentelemetry/sdk/metrics/state/multi_metric_storage.h"
#include "opentelemetry/sdk/metrics/sync_instruments.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

auto instrumentation_scope = InstrumentationScope::Create("opentelemetry-cpp", "0.1.0");

using M = std::map<std::string, std::string>;

TEST(SyncInstruments, LongCounter)
{
  InstrumentDescriptor instrument_descriptor = {
      "long_counter", "description", "1", InstrumentType::kCounter, InstrumentValueType::kLong};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  LongCounter counter(instrument_descriptor, std::move(metric_storage));
  counter.Add(10);
  counter.Add(10, opentelemetry::context::Context{});

  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}));
  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
              opentelemetry::context::Context{});
  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({}));
  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({}),
              opentelemetry::context::Context{});
}

TEST(SyncInstruments, DoubleCounter)
{
  InstrumentDescriptor instrument_descriptor = {
      "double_counter", "description", "1", InstrumentType::kCounter, InstrumentValueType::kDouble};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  DoubleCounter counter(instrument_descriptor, std::move(metric_storage));
  counter.Add(10.10);
  counter.Add(10.10, opentelemetry::context::Context{});

  counter.Add(10.10,
              opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}));
  counter.Add(10.10,
              opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
              opentelemetry::context::Context{});
  counter.Add(10.10, opentelemetry::common::KeyValueIterableView<M>({}));
  counter.Add(10.10, opentelemetry::common::KeyValueIterableView<M>({}),
              opentelemetry::context::Context{});
}

TEST(SyncInstruments, LongUpDownCounter)
{
  InstrumentDescriptor instrument_descriptor = {"long_updowncounter", "description", "1",
                                                InstrumentType::kUpDownCounter,
                                                InstrumentValueType::kLong};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  LongUpDownCounter counter(instrument_descriptor, std::move(metric_storage));
  counter.Add(10);
  counter.Add(10, opentelemetry::context::Context{});

  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}));
  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
              opentelemetry::context::Context{});
  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({}));
  counter.Add(10, opentelemetry::common::KeyValueIterableView<M>({}),
              opentelemetry::context::Context{});

  // negative values
  counter.Add(-10);
  counter.Add(-10, opentelemetry::context::Context{});

  counter.Add(-10,
              opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}));
  counter.Add(-10, opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
              opentelemetry::context::Context{});
  counter.Add(-10, opentelemetry::common::KeyValueIterableView<M>({}));
  counter.Add(-10, opentelemetry::common::KeyValueIterableView<M>({}),
              opentelemetry::context::Context{});
}

TEST(SyncInstruments, DoubleUpDownCounter)
{
  InstrumentDescriptor instrument_descriptor = {"double_updowncounter", "description", "1",
                                                InstrumentType::kUpDownCounter,
                                                InstrumentValueType::kDouble};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  DoubleUpDownCounter counter(instrument_descriptor, std::move(metric_storage));
  counter.Add(10.10);
  counter.Add(10.10, opentelemetry::context::Context{});

  counter.Add(10.10,
              opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
              opentelemetry::context::Context{});
  counter.Add(10.10,
              opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}));
  counter.Add(10.10, opentelemetry::common::KeyValueIterableView<M>({}),
              opentelemetry::context::Context{});
  counter.Add(10.10, opentelemetry::common::KeyValueIterableView<M>({}));
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
TEST(SyncInstruments, LongGauge)
{
  InstrumentDescriptor instrument_descriptor = {"long_gauge", "description", "1",
                                                InstrumentType::kGauge, InstrumentValueType::kLong};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  LongGauge gauge(instrument_descriptor, std::move(metric_storage));
  gauge.Record(10);
  gauge.Record(10, opentelemetry::context::Context{});

  gauge.Record(10, opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
               opentelemetry::context::Context{});
  gauge.Record(10,
               opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}));
  gauge.Record(10, opentelemetry::common::KeyValueIterableView<M>({}),
               opentelemetry::context::Context{});
  gauge.Record(10, opentelemetry::common::KeyValueIterableView<M>({}));
}

TEST(SyncInstruments, DoubleGauge)
{
  InstrumentDescriptor instrument_descriptor = {
      "double_gauge", "description", "1", InstrumentType::kGauge, InstrumentValueType::kDouble};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  DoubleGauge gauge(instrument_descriptor, std::move(metric_storage));
  gauge.Record(10.10);
  gauge.Record(10.10, opentelemetry::context::Context{});

  gauge.Record(10.10,
               opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
               opentelemetry::context::Context{});
  gauge.Record(10.10,
               opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}));
  gauge.Record(10.10, opentelemetry::common::KeyValueIterableView<M>({}),
               opentelemetry::context::Context{});
  gauge.Record(10.10, opentelemetry::common::KeyValueIterableView<M>({}));
}
#endif

TEST(SyncInstruments, LongHistogram)
{
  InstrumentDescriptor instrument_descriptor = {
      "long_histogram", "description", "1", InstrumentType::kHistogram, InstrumentValueType::kLong};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  LongHistogram histogram(instrument_descriptor, std::move(metric_storage));
  histogram.Record(10, opentelemetry::context::Context{});

  histogram.Record(10,
                   opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
                   opentelemetry::context::Context{});
  histogram.Record(10, opentelemetry::common::KeyValueIterableView<M>({}),
                   opentelemetry::context::Context{});
}

TEST(SyncInstruments, DoubleHistogram)
{
  InstrumentDescriptor instrument_descriptor = {"double_histogram", "description", "1",
                                                InstrumentType::kHistogram,
                                                InstrumentValueType::kDouble};
  std::unique_ptr<SyncWritableMetricStorage> metric_storage(new SyncMultiMetricStorage());
  DoubleHistogram histogram(instrument_descriptor, std::move(metric_storage));
  histogram.Record(10.10, opentelemetry::context::Context{});
  histogram.Record(-10.10, opentelemetry::context::Context{});  // This is ignored.
  histogram.Record(std::numeric_limits<double>::quiet_NaN(),
                   opentelemetry::context::Context{});  // This is ignored too
  histogram.Record(std::numeric_limits<double>::infinity(),
                   opentelemetry::context::Context{});  // This is ignored too

  histogram.Record(10.10,
                   opentelemetry::common::KeyValueIterableView<M>({{"abc", "123"}, {"xyz", "456"}}),
                   opentelemetry::context::Context{});
  histogram.Record(10.10, opentelemetry::common::KeyValueIterableView<M>({}),
                   opentelemetry::context::Context{});
}
