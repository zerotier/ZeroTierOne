// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/logs/readable_log_record.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
/**
 * Maintains a representation of a log in a format that can be processed by a recorder.
 *
 * This class is thread-compatible.
 */
class ReadWriteLogRecord final : public ReadableLogRecord
{
public:
  ReadWriteLogRecord();
  ~ReadWriteLogRecord() override;

  /**
   * Set the timestamp for this log.
   * @param timestamp the timestamp to set
   */
  void SetTimestamp(opentelemetry::common::SystemTimestamp timestamp) noexcept override;

  /**
   * Get the timestamp of this log.
   * @return the timestamp of this log
   */
  opentelemetry::common::SystemTimestamp GetTimestamp() const noexcept override;

  /**
   * Set the observed timestamp for this log.
   * @param timestamp the timestamp to set
   */
  void SetObservedTimestamp(opentelemetry::common::SystemTimestamp timestamp) noexcept override;

  /**
   * Get the observed timestamp of this log.
   * @return the observed timestamp of this log
   */
  opentelemetry::common::SystemTimestamp GetObservedTimestamp() const noexcept override;

  /**
   * Set the severity for this log.
   * @param severity the severity of the event
   */
  void SetSeverity(opentelemetry::logs::Severity severity) noexcept override;

  /**
   * Get the severity of this log.
   * @return the severity of this log
   */
  opentelemetry::logs::Severity GetSeverity() const noexcept override;

  /**
   * Set body field for this log.
   * @param message the body to set
   */
  void SetBody(const opentelemetry::common::AttributeValue &message) noexcept override;

  /**
   * Get body field of this log.
   * @return the body field for this log.
   */
  const opentelemetry::sdk::common::OwnedAttributeValue &GetBody() const noexcept override;

  /**
   * Set the Event Id object
   * @param id  the event Id to set
   * @param name the event name to set
   */
  void SetEventId(int64_t id, nostd::string_view name) noexcept override;

  /**
   * Get event Id of this log.
   * @return the event Id of this log.
   */
  int64_t GetEventId() const noexcept override;

  /**
   * Get event name of this log.
   * @return the event name of this log.
   */
  nostd::string_view GetEventName() const noexcept override;

  /**
   * Set the trace id for this log.
   * @param trace_id the trace id to set
   */
  void SetTraceId(const opentelemetry::trace::TraceId &trace_id) noexcept override;

  /**
   * Get the trace id of this log.
   * @return the trace id of this log
   */
  const opentelemetry::trace::TraceId &GetTraceId() const noexcept override;

  /**
   * Set the span id for this log.
   * @param span_id the span id to set
   */
  void SetSpanId(const opentelemetry::trace::SpanId &span_id) noexcept override;

  /**
   * Get the span id of this log.
   * @return the span id of this log
   */
  const opentelemetry::trace::SpanId &GetSpanId() const noexcept override;

  /**
   * Inject trace_flags for this log.
   * @param trace_flags the trace flags to set
   */
  void SetTraceFlags(const opentelemetry::trace::TraceFlags &trace_flags) noexcept override;

  /**
   * Inject trace_flags of this log.
   * @return trace_flags of this log
   */
  const opentelemetry::trace::TraceFlags &GetTraceFlags() const noexcept override;

  /**
   * Set an attribute of a log.
   * @param key the name of the attribute
   * @param value the attribute value
   */
  void SetAttribute(nostd::string_view key,
                    const opentelemetry::common::AttributeValue &value) noexcept override;

  /**
   * Get attributes of this log.
   * @return the body field of this log
   */
  const std::unordered_map<std::string, opentelemetry::sdk::common::OwnedAttributeValue> &
  GetAttributes() const noexcept override;

  /**
   * Get resource of this log
   * @return the resource of this log
   */
  const opentelemetry::sdk::resource::Resource &GetResource() const noexcept override;

  /**
   * Set Resource of this log
   * @param Resource the resource to set
   */
  void SetResource(const opentelemetry::sdk::resource::Resource &resource) noexcept override;

  /**
   * Get instrumentation_scope of this log.
   * @return  the instrumentation_scope of this log
   */
  const opentelemetry::sdk::instrumentationscope::InstrumentationScope &GetInstrumentationScope()
      const noexcept override;

  /**
   * Set instrumentation_scope for this log.
   * @param instrumentation_scope the instrumentation scope to set
   */
  void SetInstrumentationScope(const opentelemetry::sdk::instrumentationscope::InstrumentationScope
                                   &instrumentation_scope) noexcept override;

private:
  // Default values are set by the respective data structures' constructors for all fields,
  // except the severity field, which must be set manually (an enum with no default value).
  opentelemetry::logs::Severity severity_;
  const opentelemetry::sdk::resource::Resource *resource_;
  const opentelemetry::sdk::instrumentationscope::InstrumentationScope *instrumentation_scope_;

  std::unordered_map<std::string, opentelemetry::sdk::common::OwnedAttributeValue> attributes_map_;
  opentelemetry::sdk::common::OwnedAttributeValue body_;
  opentelemetry::common::SystemTimestamp timestamp_;
  opentelemetry::common::SystemTimestamp observed_timestamp_;

  int64_t event_id_;
  std::string event_name_;

  // We do not pay for trace state when not necessary
  struct TraceState
  {
    opentelemetry::trace::TraceId trace_id;
    opentelemetry::trace::SpanId span_id;
    opentelemetry::trace::TraceFlags trace_flags;
  };
  std::unique_ptr<TraceState> trace_state_;
};
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
