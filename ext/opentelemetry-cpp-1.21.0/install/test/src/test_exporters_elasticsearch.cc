// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/elasticsearch/es_log_record_exporter.h>
#include <opentelemetry/sdk/logs/exporter.h>

namespace logs_exporter = opentelemetry::exporter::logs;
namespace sdklogs       = opentelemetry::sdk::logs;

TEST(ExportersElasticSearchInstall, ElasticsearchLogRecordExporter)
{
  logs_exporter::ElasticsearchExporterOptions options;

  auto exporter = std::unique_ptr<sdklogs::LogRecordExporter>(
      new logs_exporter::ElasticsearchLogRecordExporter(options));

  ASSERT_TRUE(exporter != nullptr);
}