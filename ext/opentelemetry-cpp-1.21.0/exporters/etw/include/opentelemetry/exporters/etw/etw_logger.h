// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <type_traits>

#include <fstream>

#include <map>
#include <unordered_map>

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"

#include "opentelemetry/common/key_value_iterable_view.h"

#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_id.h"

#include "opentelemetry/exporters/etw/etw_config.h"
#include "opentelemetry/exporters/etw/etw_fields.h"
#include "opentelemetry/exporters/etw/etw_properties.h"
#include "opentelemetry/exporters/etw/etw_provider.h"
#include "opentelemetry/exporters/etw/utils.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace etw
{

class LoggerProvider;

class LogRecord : public opentelemetry::logs::LogRecord
{
public:
  ~LogRecord() override = default;

  void SetTimestamp(opentelemetry::common::SystemTimestamp timestamp) noexcept override
  {
    timestamp_ = timestamp;
  }

  opentelemetry::common::SystemTimestamp GetTimestamp() const noexcept { return timestamp_; }

  void SetObservedTimestamp(opentelemetry::common::SystemTimestamp timestamp) noexcept override
  {
    observed_timestamp_ = timestamp;
  }

  opentelemetry::common::SystemTimestamp GetObservedTimestamp() const noexcept
  {
    return observed_timestamp_;
  }

  void SetSeverity(opentelemetry::logs::Severity severity) noexcept override
  {
    severity_ = severity;
  }

  opentelemetry::logs::Severity GetSeverity() const noexcept { return severity_; }

  void SetBody(const opentelemetry::common::AttributeValue &message) noexcept override
  {
    body_ = message;
  }

  const opentelemetry::common::AttributeValue &GetBody() const noexcept { return body_; }

  void SetAttribute(nostd::string_view key,
                    const opentelemetry::common::AttributeValue &value) noexcept override
  {
    attributes_map_[static_cast<std::string>(key)] = value;
  }

  void SetEventId(int64_t /* id */, nostd::string_view /* name */) noexcept override {}

  const std::unordered_map<std::string, opentelemetry::common::AttributeValue> &GetAttributes()
      const noexcept
  {
    return attributes_map_;
  }

  void SetTraceId(const opentelemetry::trace::TraceId &trace_id) noexcept override
  {
    trace_id_ = trace_id;
  }

  const opentelemetry::trace::TraceId &GetTraceId() const noexcept { return trace_id_; }

  void SetSpanId(const opentelemetry::trace::SpanId &span_id) noexcept override
  {
    span_id_ = span_id;
  }

  const opentelemetry::trace::SpanId &GetSpanId() const noexcept { return span_id_; }

  void SetTraceFlags(const opentelemetry::trace::TraceFlags &trace_flags) noexcept override
  {
    trace_flags_ = trace_flags;
  }

  const opentelemetry::trace::TraceFlags &GetTraceFlags() const noexcept { return trace_flags_; }

private:
  opentelemetry::logs::Severity severity_ = opentelemetry::logs::Severity::kInvalid;

  std::unordered_map<std::string, opentelemetry::common::AttributeValue> attributes_map_;
  opentelemetry::common::AttributeValue body_ = opentelemetry::nostd::string_view();
  opentelemetry::common::SystemTimestamp timestamp_;
  opentelemetry::common::SystemTimestamp observed_timestamp_ = std::chrono::system_clock::now();

  opentelemetry::trace::TraceId trace_id_;
  opentelemetry::trace::SpanId span_id_;
  opentelemetry::trace::TraceFlags trace_flags_;
};

/**
 * @brief Logger  class that allows to send logs to ETW Provider.
 */
class Logger : public opentelemetry::logs::Logger
{

  /**
   * @brief Parent provider of this Tracer
   */
  etw::LoggerProvider &loggerProvider_;

  /**
   * @brief ProviderId (Name or GUID)
   */
  std::string provId;

  std::string eventName_;

  /**
   * @brief Encoding (Manifest, MessagePack or XML)
   */
  ETWProvider::EventFormat encoding;

  /**
   * @brief Provider Handle
   */
  ETWProvider::Handle &provHandle;

  /**
   * @brief ETWProvider is a singleton that aggregates all ETW writes.
   * @return
   */
  static ETWProvider &etwProvider()
  {
    static ETWProvider instance;  // C++11 magic static
    return instance;
  }

  /**
   * @brief Init a reference to etw::ProviderHandle
   * @return Provider Handle
   */
  ETWProvider::Handle &initProvHandle() { return etwProvider().open(provId, encoding); }

public:
  /**
   * @brief Tracer constructor
   * @param parent Parent LoggerProvider
   * @param providerId ProviderId - Name or GUID
   * @param encoding ETW encoding format to use.
   */
  Logger(etw::LoggerProvider &parent,
         nostd::string_view eventName,
         nostd::string_view providerId     = "",
         ETWProvider::EventFormat encoding = ETWProvider::EventFormat::ETW_MANIFEST)
      : opentelemetry::logs::Logger(),
        loggerProvider_(parent),
        eventName_(eventName),
        provId(providerId.data(), providerId.size()),
        encoding(encoding),
        provHandle(initProvHandle())
  {}

  nostd::unique_ptr<opentelemetry::logs::LogRecord> CreateLogRecord() noexcept
  {
    return nostd::unique_ptr<opentelemetry::logs::LogRecord>(new LogRecord());
  }

  using opentelemetry::logs::Logger::EmitLogRecord;

  void EmitLogRecord(
      nostd::unique_ptr<opentelemetry::logs::LogRecord> &&log_record) noexcept override
  {
    if (!log_record)
    {
      return;
    }
    LogRecord *readable_record = static_cast<LogRecord *>(log_record.get());

    nostd::string_view body;
    if (nostd::holds_alternative<const char *>(readable_record->GetBody()))
    {
      body = nostd::get<const char *>(readable_record->GetBody());
    }
    else if (nostd::holds_alternative<nostd::string_view>(readable_record->GetBody()))
    {
      body = nostd::get<nostd::string_view>(readable_record->GetBody());
    }
    // TODO: More body types?

    Properties evtCopy{
        opentelemetry::common::MakeKeyValueIterableView(readable_record->GetAttributes())};
    return Log(readable_record->GetSeverity(), provId, body, evtCopy, readable_record->GetTraceId(),
               readable_record->GetSpanId(), readable_record->GetTraceFlags(),
               readable_record->GetTimestamp());
  }

  virtual void Log(opentelemetry::logs::Severity severity,
                   nostd::string_view name,
                   nostd::string_view body,
                   Properties &input_evt,
                   opentelemetry::trace::TraceId trace_id,
                   opentelemetry::trace::SpanId span_id,
                   opentelemetry::trace::TraceFlags trace_flags,
                   opentelemetry::common::SystemTimestamp timestamp) noexcept
  {
    UNREFERENCED_PARAMETER(trace_flags);

#if defined(ENABLE_ENV_PROPERTIES)

    Properties env_properties_env = {};
    bool has_customer_attribute   = false;
    if (input_evt.size() > 0)
    {
      nlohmann::json env_properties_json = nlohmann::json::object();
      for (auto &kv : input_evt)
      {
        nostd::string_view key = kv.first.data();

        // don't serialize fields propagated from span data.
        if (key == ETW_FIELD_NAME || key == ETW_FIELD_SPAN_ID || key == ETW_FIELD_TRACE_ID ||
            key == ETW_FIELD_SPAN_PARENTID)
        {
          env_properties_env[key.data()] = kv.second;
        }
        else
        {
          utils::PopulateAttribute(env_properties_json, key, kv.second);
          has_customer_attribute = true;
        }
      }
      if (has_customer_attribute)
      {
        env_properties_env[ETW_FIELD_ENV_PROPERTIES] = env_properties_json.dump();
      }
    }

    Properties &evt = has_customer_attribute ? env_properties_env : input_evt;

#else

    Properties &evt = input_evt;

#endif  // defined(ENABLE_ENV_PROPERTIES)

    // Populate Etw.EventName attribute at envelope level
    evt[ETW_FIELD_NAME] = eventName_.data();

#ifdef HAVE_FIELD_TIME
    {
      auto timeNow        = std::chrono::system_clock::now().time_since_epoch();
      auto nanos          = std::chrono::duration_cast<std::chrono::nanoseconds>(timeNow).count();
      evt[ETW_FIELD_TIME] = utils::formatUtcTimestampNsAsISO8601(nanos);
    }
#endif
    const auto &cfg = GetConfiguration(loggerProvider_);
    if (cfg.enableSpanId)
    {
      evt[ETW_FIELD_SPAN_ID] = ToLowerBase16(span_id);
    }
    if (cfg.enableTraceId)
    {
      evt[ETW_FIELD_TRACE_ID] = ToLowerBase16(trace_id);
    }
    // Populate ActivityId if enabled
    GUID ActivityId;
    LPGUID ActivityIdPtr = nullptr;
    if (cfg.enableActivityId)
    {
      if (CopySpanIdToActivityId(span_id, ActivityId))
      {
        ActivityIdPtr = &ActivityId;
      }
    }
    evt[ETW_FIELD_PAYLOAD_NAME]              = std::string(name.data(), name.size());
    std::chrono::system_clock::time_point ts = timestamp;
    int64_t tsNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(ts.time_since_epoch()).count();
    evt[ETW_FIELD_TIMESTAMP] = utils::formatUtcTimestampNsAsISO8601(tsNs);
    int severity_index       = static_cast<int>(severity);
    if (severity_index < 0 ||
        severity_index >= std::extent<decltype(opentelemetry::logs::SeverityNumToText)>::value)
    {
      std::stringstream sout;
      sout << "Invalid severity(" << severity_index << ")";
      evt[ETW_FIELD_LOG_SEVERITY_TEXT] = sout.str();
    }
    else
    {
      evt[ETW_FIELD_LOG_SEVERITY_TEXT] =
          opentelemetry::logs::SeverityNumToText[severity_index].data();
    }
    evt[ETW_FIELD_LOG_SEVERITY_NUM] = static_cast<uint32_t>(severity);
    evt[ETW_FIELD_LOG_BODY]         = std::string(body.data(), body.length());

#if defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

    for (const auto &attr : cfg.timestampAttributes)
    {
      auto it = evt.find(attr);
      if (it != evt.end())
      {
        auto value_index = it->second.index();
        if (value_index != exporter_etw::PropertyType::kTypeInt64 &&
            value_index != exporter_etw::PropertyType::kTypeUInt64)
        {
          continue;
        }
        int64_t filetime = value_index == exporter_etw::PropertyType::kTypeUInt64
                               ? nostd::get<uint64_t>(it->second)
                               : nostd::get<int64_t>(it->second);
        constexpr int64_t FILETIME_EPOCH_DIFF = 11644473600LL;  // Seconds from 1601 to 1970
        constexpr int64_t HUNDRED_NANOSECONDS_PER_SECOND = 10000000LL;
        int64_t unix_time_seconds =
            (filetime / HUNDRED_NANOSECONDS_PER_SECOND) - FILETIME_EPOCH_DIFF;
        int64_t unix_time_nanos =
            unix_time_seconds * 1'000'000'000 + (filetime % HUNDRED_NANOSECONDS_PER_SECOND) * 100;
        it->second = utils::formatUtcTimestampNsAsISO8601(unix_time_nanos);
      }
    }

#endif  // defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

    etwProvider().write(provHandle, evt, nullptr, nullptr, 0, encoding);
  }

  const nostd::string_view GetName() noexcept override { return std::string(); }
  // TODO : Flush and Shutdown method in main Logger API
  ~Logger() { etwProvider().close(provHandle); }
};

/**
 * @brief ETW LoggerProvider
 */
class LoggerProvider : public opentelemetry::logs::LoggerProvider
{
public:
  /**
   * @brief LoggerProvider options supplied during initialization.
   */
  TelemetryProviderConfiguration config_;

  /**
   * @brief Construct instance of LoggerProvider with given options
   * @param options Configuration options
   */
  LoggerProvider(TelemetryProviderOptions options) : opentelemetry::logs::LoggerProvider()
  {
    GetOption(options, "enableTraceId", config_.enableTraceId, true);
    GetOption(options, "enableSpanId", config_.enableSpanId, true);
    GetOption(options, "enableActivityId", config_.enableActivityId, false);
    GetOption(options, "enableTableNameMappings", config_.enableTableNameMappings, false);
    GetOption(options, "tableNameMappings", config_.tableNameMappings, {});

#if defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

    GetOption(options, "timestampAttributes", config_.timestampAttributes, {});

#endif  // defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

    // Determines what encoding to use for ETW events: TraceLogging Dynamic, MsgPack, XML, etc.
    config_.encoding = GetEncoding(options);
  }

  LoggerProvider() : opentelemetry::logs::LoggerProvider()
  {
    config_.encoding = ETWProvider::EventFormat::ETW_MANIFEST;
  }

  nostd::shared_ptr<opentelemetry::logs::Logger> GetLogger(
      opentelemetry::nostd::string_view logger_name,
      opentelemetry::nostd::string_view library_name = "",
      opentelemetry::nostd::string_view version      = "",
      opentelemetry::nostd::string_view schema_url   = "",
      const opentelemetry::common::KeyValueIterable &attributes =
          opentelemetry::common::NoopKeyValueIterable()) override
  {
    UNREFERENCED_PARAMETER(version);
    UNREFERENCED_PARAMETER(schema_url);
    UNREFERENCED_PARAMETER(attributes);

    std::string event_name{ETW_VALUE_LOG};
    if (config_.enableTableNameMappings)
    {
      auto it =
          config_.tableNameMappings.find(std::string(library_name.data(), library_name.size()));
      if (it != config_.tableNameMappings.end())
      {
        event_name = it->second;
      }
    }

    ETWProvider::EventFormat evtFmt = config_.encoding;
    return nostd::shared_ptr<opentelemetry::logs::Logger>{
        new (std::nothrow) etw::Logger(*this, event_name, logger_name, evtFmt)};
  }
};

}  // namespace etw
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
