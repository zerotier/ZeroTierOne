// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

#include "opentelemetry/exporters/otlp/otlp_file_client_options.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

/**
 * Struct to hold OTLP File log record exporter options.
 *
 * See
 * https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/protocol/file-exporter.md
 */
struct OPENTELEMETRY_EXPORT OtlpFileLogRecordExporterOptions : public OtlpFileClientOptions
{
  OtlpFileLogRecordExporterOptions();
  ~OtlpFileLogRecordExporterOptions();
};

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
