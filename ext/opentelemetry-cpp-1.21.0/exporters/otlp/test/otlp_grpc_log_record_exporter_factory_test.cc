// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"

/*
  Make sure OtlpGrpcLogRecordExporterFactory does not require,
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

#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

TEST(OtlpGrpcLogRecordExporterFactoryTest, BuildTest)
{
  OtlpGrpcLogRecordExporterOptions opts;
  opts.endpoint = "localhost:45454";

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter =
      OtlpGrpcLogRecordExporterFactory::Create(opts);

  EXPECT_TRUE(exporter != nullptr);
}

TEST(OtlpGrpcLogRecordExporterFactoryTest, ShareClient)
{
  OtlpGrpcLogRecordExporterOptions opts;
  opts.endpoint = "localhost:45454";

  std::shared_ptr<OtlpGrpcClient> client = OtlpGrpcClientFactory::Create(opts);
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter1 =
      OtlpGrpcLogRecordExporterFactory::Create(opts, client);

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter2 =
      OtlpGrpcLogRecordExporterFactory::Create(opts, client);

  EXPECT_TRUE(exporter1 != nullptr);
  EXPECT_TRUE(exporter2 != nullptr);

  EXPECT_TRUE(static_cast<OtlpGrpcLogRecordExporter *>(exporter1.get())->GetClient().get() ==
              client.get());
  EXPECT_TRUE(static_cast<OtlpGrpcLogRecordExporter *>(exporter2.get())->GetClient().get() ==
              client.get());
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
