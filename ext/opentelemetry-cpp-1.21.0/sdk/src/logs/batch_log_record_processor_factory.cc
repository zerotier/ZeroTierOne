// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <utility>

#include "opentelemetry/sdk/logs/batch_log_record_processor.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_options.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_runtime_options.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

std::unique_ptr<LogRecordProcessor> BatchLogRecordProcessorFactory::Create(
    std::unique_ptr<LogRecordExporter> &&exporter,
    const BatchLogRecordProcessorOptions &options)
{
  BatchLogRecordProcessorRuntimeOptions runtime_options;
  return Create(std::move(exporter), options, runtime_options);
}

std::unique_ptr<LogRecordProcessor> BatchLogRecordProcessorFactory::Create(
    std::unique_ptr<LogRecordExporter> &&exporter,
    const BatchLogRecordProcessorOptions &options,
    const BatchLogRecordProcessorRuntimeOptions &runtime_options)
{
  std::unique_ptr<LogRecordProcessor> processor(
      new BatchLogRecordProcessor(std::move(exporter), options, runtime_options));
  return processor;
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
