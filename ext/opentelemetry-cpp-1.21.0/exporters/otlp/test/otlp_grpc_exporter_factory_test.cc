// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"

/*
  Make sure OtlpGrpcExporterFactory does not require,
  even indirectly, protobuf headers.
*/
#ifdef GOOGLE_PROTOBUF_VERSION
#  error "protobuf should not be included"
#endif

/*
  Implementation, this requires protobuf.
*/
#include "opentelemetry/exporters/otlp/otlp_grpc_client_factory.h"

/*
  Make sure OtlpGrpcClientFactory does not require,
  even indirectly, gRPC headers.
*/
#if defined(GRPC_CPP_VERSION_MAJOR) || defined(GRPC_CPP_VERSION_STRING)
#  error "gRPC should not be included"
#endif

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

TEST(OtlpGrpcExporterFactoryTest, BuildTest)
{
  OtlpGrpcExporterOptions opts;
  opts.endpoint = "localhost:45454";

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter =
      OtlpGrpcExporterFactory::Create(opts);

  EXPECT_TRUE(exporter != nullptr);
}

TEST(OtlpGrpcExporterFactoryTest, ShareClient)
{
  OtlpGrpcExporterOptions opts;
  opts.endpoint = "localhost:45454";

  std::shared_ptr<OtlpGrpcClient> client = OtlpGrpcClientFactory::Create(opts);
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter1 =
      OtlpGrpcExporterFactory::Create(opts, client);

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter2 =
      OtlpGrpcExporterFactory::Create(opts, client);

  EXPECT_TRUE(exporter1 != nullptr);
  EXPECT_TRUE(exporter2 != nullptr);

  EXPECT_TRUE(static_cast<OtlpGrpcExporter *>(exporter1.get())->GetClient().get() == client.get());
  EXPECT_TRUE(static_cast<OtlpGrpcExporter *>(exporter2.get())->GetClient().get() == client.get());
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
