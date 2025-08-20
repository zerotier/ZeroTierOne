// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/exporters/zipkin/zipkin_exporter_factory.h>
#include <opentelemetry/exporters/zipkin/zipkin_exporter_options.h>

TEST(ExportersZipkinInstall, ZipkinExporter)
{
  auto options  = opentelemetry::exporter::zipkin::ZipkinExporterOptions();
  auto exporter = opentelemetry::exporter::zipkin::ZipkinExporterFactory::Create(options);
  ASSERT_TRUE(exporter != nullptr);
}