// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/ext/http/client/http_client_factory.h"
#include "opentelemetry/ext/http/common/url_parser.h"
#include "opentelemetry/sdk/common/env_variables.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace zipkin
{

inline const std::string GetDefaultZipkinEndpoint()
{
  const char *otel_exporter_zipkin_endpoint_env = "OTEL_EXPORTER_ZIPKIN_ENDPOINT";
  const char *kZipkinEndpointDefault            = "http://localhost:9411/api/v2/spans";

  std::string endpoint;

  auto exists = opentelemetry::sdk::common::GetStringEnvironmentVariable(
      otel_exporter_zipkin_endpoint_env, endpoint);
  return exists ? endpoint : kZipkinEndpointDefault;
}

enum class TransportFormat
{
  kJson,
  kProtobuf
};

/**
 * Struct to hold Zipkin  exporter options.
 */
struct ZipkinExporterOptions
{
  // The endpoint to export to. By default the OpenTelemetry Collector's default endpoint.
  std::string endpoint     = GetDefaultZipkinEndpoint();
  TransportFormat format   = TransportFormat::kJson;
  std::string service_name = "default-service";
  std::string ipv4;
  std::string ipv6;
  ext::http::client::Headers headers = {{"content-type", "application/json"}};
};

}  // namespace zipkin
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
