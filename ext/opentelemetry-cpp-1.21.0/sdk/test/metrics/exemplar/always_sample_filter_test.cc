// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "opentelemetry/context/context.h"
#include "opentelemetry/sdk/metrics/exemplar/filter.h"

using namespace opentelemetry::sdk::metrics;

TEST(AlwaysSampleFilter, SampleMeasurement)
{
  auto filter = opentelemetry::sdk::metrics::ExemplarFilter::GetAlwaysSampleFilter();
  ASSERT_TRUE(
      filter->ShouldSampleMeasurement(1.0, MetricAttributes{}, opentelemetry::context::Context{}));
  ASSERT_TRUE(filter->ShouldSampleMeasurement(static_cast<int64_t>(1), MetricAttributes{},
                                              opentelemetry::context::Context{}));
}
