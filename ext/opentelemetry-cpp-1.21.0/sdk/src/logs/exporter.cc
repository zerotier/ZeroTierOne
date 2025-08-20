// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/logs/exporter.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

OPENTELEMETRY_EXPORT LogRecordExporter::LogRecordExporter() {}

OPENTELEMETRY_EXPORT LogRecordExporter::~LogRecordExporter() {}

OPENTELEMETRY_EXPORT bool LogRecordExporter::ForceFlush(
    std::chrono::microseconds /*timeout*/) noexcept
{
  return true;
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
