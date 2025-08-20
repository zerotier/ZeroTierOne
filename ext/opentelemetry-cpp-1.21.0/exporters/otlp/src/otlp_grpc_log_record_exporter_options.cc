// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

OtlpGrpcLogRecordExporterOptions::OtlpGrpcLogRecordExporterOptions()
{
  endpoint                         = GetOtlpDefaultGrpcLogsEndpoint();
  use_ssl_credentials              = !GetOtlpDefaultGrpcLogsIsInsecure(); /* negation intended. */
  ssl_credentials_cacert_path      = GetOtlpDefaultLogsSslCertificatePath();
  ssl_credentials_cacert_as_string = GetOtlpDefaultLogsSslCertificateString();

#ifdef ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
  ssl_client_key_path    = GetOtlpDefaultLogsSslClientKeyPath();
  ssl_client_key_string  = GetOtlpDefaultLogsSslClientKeyString();
  ssl_client_cert_path   = GetOtlpDefaultLogsSslClientCertificatePath();
  ssl_client_cert_string = GetOtlpDefaultLogsSslClientCertificateString();
#endif

  timeout    = GetOtlpDefaultLogsTimeout();
  metadata   = GetOtlpDefaultLogsHeaders();
  user_agent = GetOtlpDefaultUserAgent();

  max_threads = 0;

  compression = GetOtlpDefaultLogsCompression();
#ifdef ENABLE_ASYNC_EXPORT
  max_concurrent_requests = 64;
#endif

  retry_policy_max_attempts       = GetOtlpDefaultLogsRetryMaxAttempts();
  retry_policy_initial_backoff    = GetOtlpDefaultLogsRetryInitialBackoff();
  retry_policy_max_backoff        = GetOtlpDefaultLogsRetryMaxBackoff();
  retry_policy_backoff_multiplier = GetOtlpDefaultLogsRetryBackoffMultiplier();
}

OtlpGrpcLogRecordExporterOptions::~OtlpGrpcLogRecordExporterOptions() {}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
