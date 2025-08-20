// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/prometheus/exporter_factory.h>
#include <opentelemetry/exporters/prometheus/exporter_options.h>

TEST(ExportersPrometheusInstall, PrometheusExporter)
{
  auto options  = opentelemetry::exporter::metrics::PrometheusExporterOptions();
  auto exporter = opentelemetry::exporter::metrics::PrometheusExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}