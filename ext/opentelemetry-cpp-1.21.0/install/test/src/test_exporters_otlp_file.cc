// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/otlp/otlp_file_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_file_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_file_metric_exporter_options.h>

#include <opentelemetry/exporters/otlp/otlp_file_client.h>
#include <opentelemetry/exporters/otlp/otlp_file_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_file_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_file_metric_exporter_factory.h>

TEST(ExportersOtlpFileInstall, OtlpFileExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpFileExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpFileExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOtlpFileInstall, OtlpFileLogRecordExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpFileLogRecordExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpFileLogRecordExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOtlpFileInstall, OtlpFileMetricExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpFileMetricExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpFileMetricExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}
