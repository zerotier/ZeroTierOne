// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/version.h"

/*
  Make sure OtlpHttpExporterFactory does not require,
  even indirectly, nlohmann/json headers.
*/
#ifdef NLOHMANN_JSON_VERSION_MAJOR
#  error "nlohmann/json should not be included"
#endif /* NLOHMANN_JSON_VERSION_MAJOR */

/*
  Make sure OtlpHttpExporterFactory does not require,
  even indirectly, protobuf headers.
*/
#ifdef GOOGLE_PROTOBUF_VERSION
#  error "protobuf should not be included"
#endif

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

TEST(OtlpHttpExporterFactoryTest, BuildTest)
{
  OtlpHttpExporterOptions opts;
  opts.url = "localhost:45454";

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter =
      OtlpHttpExporterFactory::Create(opts);

  EXPECT_TRUE(exporter != nullptr);
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
