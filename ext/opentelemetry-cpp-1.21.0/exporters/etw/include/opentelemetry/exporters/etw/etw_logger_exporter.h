// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <mutex>

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"

#include "opentelemetry/common/key_value_iterable_view.h"

#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_id.h"

#include "opentelemetry/sdk/logs/exporter.h"

#include "opentelemetry/exporters/etw/etw_config.h"
#include "opentelemetry/exporters/etw/etw_logger.h"
#include "opentelemetry/exporters/etw/etw_provider.h"

#include "opentelemetry/exporters/etw/utils.h"
