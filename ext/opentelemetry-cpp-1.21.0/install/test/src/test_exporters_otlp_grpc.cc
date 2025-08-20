// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/otlp/otlp_grpc_client_options.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>

#include <opentelemetry/exporters/otlp/otlp_grpc_client_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>

TEST(ExportersOtlpGrpcInstall, OtlpGrpcClient)
{
  auto options = opentelemetry::exporter::otlp::OtlpGrpcExporterOptions();
  auto client  = opentelemetry::exporter::otlp::OtlpGrpcClientFactory::Create(options);
  ASSERT_TRUE(client != nullptr);
}

TEST(ExportersOtlpGrpcInstall, OtlpGrpcExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpGrpcExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOtlpGrpcInstall, OtlpGrpcLogRecordExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}

TEST(ExportersOtlpGrpcInstall, OtlpGrpcMetricExporter)
{
  auto options  = opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions();
  auto exporter = opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}
