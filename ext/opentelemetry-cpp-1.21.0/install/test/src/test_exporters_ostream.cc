// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/ostream/log_record_exporter_factory.h>
#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>

TEST(ExportersOstreamInstall, OStreamSpanExporter)
{
  auto exporter = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOstreamInstall, OStreamMetricExporter)
{
  auto exporter = opentelemetry::exporter::metrics::OStreamMetricExporterFactory::Create();
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOstreamInstall, OStreamLogRecordExporter)
{
  auto exporter = opentelemetry::exporter::logs::OStreamLogRecordExporterFactory::Create();
  ASSERT_TRUE(exporter != nullptr);
}