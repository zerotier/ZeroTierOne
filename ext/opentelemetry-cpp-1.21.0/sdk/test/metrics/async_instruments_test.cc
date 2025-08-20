// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/sdk/metrics/async_instruments.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/state/metric_storage.h"
#include "opentelemetry/sdk/metrics/state/multi_metric_storage.h"
#include "opentelemetry/sdk/metrics/state/observable_registry.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::metrics;

using M = std::map<std::string, std::string>;

namespace
{
// NOLINTNEXTLINE
void asyc_generate_measurements(opentelemetry::metrics::ObserverResult /* observer */,
                                void * /* state */)
{}
}  // namespace

TEST(AsyncInstruments, ObservableInstrument)
{
  InstrumentDescriptor instrument_descriptor = {"long_counter", "description", "1",
                                                InstrumentType::kObservableCounter,
                                                InstrumentValueType::kLong};
  std::shared_ptr<ObservableRegistry> observable_registry(new ObservableRegistry());
  std::unique_ptr<AsyncWritableMetricStorage> metric_storage(new AsyncMultiMetricStorage());
  ObservableInstrument observable_counter_long(instrument_descriptor, std::move(metric_storage),
                                               observable_registry);
  observable_counter_long.AddCallback(asyc_generate_measurements, nullptr);
}
