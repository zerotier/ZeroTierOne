// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_environment.h"
#include "opentelemetry/exporters/otlp/otlp_http.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

OtlpHttpLogRecordExporterOptions::OtlpHttpLogRecordExporterOptions()
    : url(GetOtlpDefaultHttpLogsEndpoint()),
      content_type(GetOtlpHttpProtocolFromString(GetOtlpDefaultHttpLogsProtocol())),
      json_bytes_mapping(JsonBytesMappingKind::kHexId),
      use_json_name(false),
      console_debug(false),
      timeout(GetOtlpDefaultLogsTimeout()),
      http_headers(GetOtlpDefaultLogsHeaders()),
      ssl_insecure_skip_verify(false),
      ssl_ca_cert_path(GetOtlpDefaultLogsSslCertificatePath()),
      ssl_ca_cert_string(GetOtlpDefaultLogsSslCertificateString()),
      ssl_client_key_path(GetOtlpDefaultLogsSslClientKeyPath()),
      ssl_client_key_string(GetOtlpDefaultLogsSslClientKeyString()),
      ssl_client_cert_path(GetOtlpDefaultLogsSslClientCertificatePath()),
      ssl_client_cert_string(GetOtlpDefaultLogsSslClientCertificateString()),
      ssl_min_tls(GetOtlpDefaultLogsSslTlsMinVersion()),
      ssl_max_tls(GetOtlpDefaultLogsSslTlsMaxVersion()),
      ssl_cipher(GetOtlpDefaultLogsSslTlsCipher()),
      ssl_cipher_suite(GetOtlpDefaultLogsSslTlsCipherSuite()),
      compression(GetOtlpDefaultLogsCompression()),
      retry_policy_max_attempts(GetOtlpDefaultLogsRetryMaxAttempts()),
      retry_policy_initial_backoff(GetOtlpDefaultLogsRetryInitialBackoff()),
      retry_policy_max_backoff(GetOtlpDefaultLogsRetryMaxBackoff()),
      retry_policy_backoff_multiplier(GetOtlpDefaultLogsRetryBackoffMultiplier())
{
#ifdef ENABLE_ASYNC_EXPORT
  max_concurrent_requests     = 64;
  max_requests_per_connection = 8;
#endif
}

OtlpHttpLogRecordExporterOptions::~OtlpHttpLogRecordExporterOptions() {}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
