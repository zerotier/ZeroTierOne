// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <utility>
#include <vector>

#include "opentelemetry/sdk/logs/multi_log_record_processor.h"
#include "opentelemetry/sdk/logs/multi_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

std::unique_ptr<LogRecordProcessor> MultiLogRecordProcessorFactory::Create(
    std::vector<std::unique_ptr<LogRecordProcessor>> &&processors)
{
  std::unique_ptr<LogRecordProcessor> processor(new MultiLogRecordProcessor(std::move(processors)));
  return processor;
}

}  // namespace logs
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
