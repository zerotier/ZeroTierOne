// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>

#include <opentelemetry/exporters/otlp/otlp_http_client.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>

TEST(ExportersOtlpHttpInstall, OtlpHttpExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpHttpExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOtlpHttpInstall, OtlpHttpLogRecordExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOtlpHttpInstall, OtlpHttpMetricExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}
