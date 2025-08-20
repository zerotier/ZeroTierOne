// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/memory/in_memory_metric_data.h>
#include <opentelemetry/exporters/memory/in_memory_metric_exporter_factory.h>
#include <opentelemetry/exporters/memory/in_memory_span_data.h>
#include <opentelemetry/exporters/memory/in_memory_span_exporter_factory.h>

using namespace opentelemetry::exporter::memory;

TEST(ExportersInMemoryInstall, InMemorySpanExporter)
{
  auto data     = std::shared_ptr<InMemorySpanData>{new InMemorySpanData(10)};
  auto exporter = InMemorySpanExporterFactory::Create(data);
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOstreamInstall, InMemoryMetricExporter)
{
  auto data     = std::shared_ptr<InMemoryMetricData>{new SimpleAggregateInMemoryMetricData()};
  auto exporter = InMemoryMetricExporterFactory::Create(data);
  ASSERT_TRUE(exporter != nullptr);
}
