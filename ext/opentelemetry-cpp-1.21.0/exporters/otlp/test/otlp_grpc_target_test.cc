// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#include "opentelemetry/exporters/otlp/otlp_grpc_client.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

TEST(OtlpGrpcClientEndpointTest, GrpcClientTest)
{
  OtlpGrpcClientOptions opts1;
  opts1.endpoint = "unix:///tmp/otel1.sock";

  OtlpGrpcClientOptions opts2;
  opts2.endpoint = "unix:tmp/otel2.sock";

  OtlpGrpcClientOptions opts3;
  opts3.endpoint = "localhost:4317";

  auto target1 = OtlpGrpcClient::GetGrpcTarget(opts1.endpoint);
  auto target2 = OtlpGrpcClient::GetGrpcTarget(opts2.endpoint);
  auto target3 = OtlpGrpcClient::GetGrpcTarget(opts3.endpoint);

  EXPECT_EQ(target1, "unix:/tmp/otel1.sock");
  EXPECT_EQ(target2, "");
  EXPECT_EQ(target3, "localhost:4317");
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
