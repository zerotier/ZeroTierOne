// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW

#  include "opentelemetry/sdk/metrics/exemplar/no_exemplar_reservoir.h"
#  include <gtest/gtest.h>

using namespace opentelemetry::sdk::metrics;

TEST(NoExemplarReservoir, OfferMeasurement)
{
  auto reservoir = opentelemetry::sdk::metrics::ExemplarReservoir::GetNoExemplarReservoir();
  reservoir->OfferMeasurement(1.0, MetricAttributes{}, opentelemetry::context::Context{},
                              std::chrono::system_clock::now());
  reservoir->OfferMeasurement(static_cast<int64_t>(1), MetricAttributes{},
                              opentelemetry::context::Context{}, std::chrono::system_clock::now());
  auto exemplar_data = reservoir->CollectAndReset(MetricAttributes{});
  ASSERT_TRUE(exemplar_data.empty());
}

#endif  // ENABLE_METRICS_EXEMPLAR_PREVIEW
