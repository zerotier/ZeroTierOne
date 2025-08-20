// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <string>

#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/exporters/prometheus/exporter_options.h"
#include "opentelemetry/sdk/metrics/instruments.h"

/**
 * PrometheusExporterTest is a friend class of PrometheusExporter.
 * It has access to a private constructor that does not take in
 * an exposer as an argument, and instead takes no arguments; this
 * private constructor is only to be used here for testing
 */

using opentelemetry::exporter::metrics::PrometheusExporter;
using opentelemetry::exporter::metrics::PrometheusExporterOptions;
using opentelemetry::sdk::metrics::AggregationTemporality;
using opentelemetry::sdk::metrics::InstrumentType;
/**
 * When a PrometheusExporter is initialized,
 * isShutdown should be false.
 */
TEST(PrometheusExporter, InitializeConstructorIsNotShutdown)
{
  PrometheusExporterOptions options;
  options.url = "localhost:8081";
  PrometheusExporter exporter(options);
  // // Asserts that the exporter is not shutdown.
  // ASSERT_TRUE(!exporter.IsShutdown());
  exporter.Shutdown();
}

/**
 * The shutdown() function should set the isShutdown field to true.
 */
TEST(PrometheusExporter, ShutdownSetsIsShutdownToTrue)
{
  PrometheusExporterOptions options;
  PrometheusExporter exporter(options);

  // exporter shuold not be shutdown by default
  ASSERT_TRUE(!exporter.IsShutdown());

  exporter.Shutdown();

  // the exporter shuold be shutdown
  ASSERT_TRUE(exporter.IsShutdown());

  // shutdown function should be idempotent
  exporter.Shutdown();
  ASSERT_TRUE(exporter.IsShutdown());
}

/**
 * The Aggregation temporality should be correctly set
 */
TEST(PrometheusExporter, CheckAggregationTemporality)
{
  PrometheusExporterOptions options;
  PrometheusExporter exporter(options);

  ASSERT_EQ(exporter.GetAggregationTemporality(InstrumentType::kCounter),
            AggregationTemporality::kCumulative);
  ASSERT_EQ(exporter.GetAggregationTemporality(InstrumentType::kHistogram),
            AggregationTemporality::kCumulative);
  ASSERT_EQ(exporter.GetAggregationTemporality(InstrumentType::kObservableCounter),
            AggregationTemporality::kCumulative);
  ASSERT_EQ(exporter.GetAggregationTemporality(InstrumentType::kObservableGauge),
            AggregationTemporality::kCumulative);
  ASSERT_EQ(exporter.GetAggregationTemporality(InstrumentType::kObservableUpDownCounter),
            AggregationTemporality::kCumulative);
  ASSERT_EQ(exporter.GetAggregationTemporality(InstrumentType::kUpDownCounter),
            AggregationTemporality::kCumulative);
}
