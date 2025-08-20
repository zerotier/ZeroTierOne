// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter_options.h"

#include <chrono>
#include <string>

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

OtlpFileLogRecordExporterOptions::OtlpFileLogRecordExporterOptions()
{
  console_debug = false;

  OtlpFileClientFileSystemOptions fs_options;
  fs_options.file_pattern   = "logs-%N.jsonl";
  fs_options.alias_pattern  = "logs-latest.jsonl";
  fs_options.flush_interval = std::chrono::seconds(30);
  fs_options.flush_count    = 256;
  fs_options.file_size      = 1024 * 1024 * 20;
  fs_options.rotate_size    = 10;

  backend_options = fs_options;
}

OtlpFileLogRecordExporterOptions::~OtlpFileLogRecordExporterOptions() {}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
