// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/logs/event_logger.h"
#include "opentelemetry/logs/event_logger_provider.h"
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/noop.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/logs/event_logger_provider.h"
#include "opentelemetry/sdk/logs/logger.h"
#include "opentelemetry/sdk/logs/logger_config.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/tracer.h"

using namespace opentelemetry::sdk::logs;
using namespace opentelemetry::sdk::instrumentationscope;
namespace logs_api = opentelemetry::logs;
namespace nostd    = opentelemetry::nostd;

TEST(LoggerSDK, LogToNullProcessor)
{
  // Confirm Logger::EmitLogRecord() does not have undefined behavior
  // even when there is no processor set
  // since it calls Processor::OnEmit()

  auto lp = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = lp->GetLogger("logger", "opentelelemtry_library", "", schema_url);

  auto sdk_logger = static_cast<opentelemetry::sdk::logs::Logger *>(logger.get());
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetName(), "opentelelemtry_library");
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetVersion(), "");
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetSchemaURL(), schema_url);
  // Log a sample log record to a nullptr processor
  logger->Debug("Test log");
}

class MockLogRecordable final : public opentelemetry::sdk::logs::Recordable
{
public:
  void SetTimestamp(opentelemetry::common::SystemTimestamp) noexcept override {}

  void SetObservedTimestamp(
      opentelemetry::common::SystemTimestamp observed_timestamp) noexcept override
  {
    observed_timestamp_ = observed_timestamp;
  }

  const opentelemetry::common::SystemTimestamp &GetObservedTimestamp() const noexcept
  {
    return observed_timestamp_;
  }

  opentelemetry::logs::Severity GetSeverity() const noexcept { return severity_; }

  void SetSeverity(opentelemetry::logs::Severity severity) noexcept override
  {
    severity_ = severity;
  }

  nostd::string_view GetBody() const noexcept { return body_; }

  void SetBody(const opentelemetry::common::AttributeValue &message) noexcept override
  {
    if (nostd::holds_alternative<const char *>(message))
    {
      body_ = nostd::get<const char *>(message);
    }
    else if (nostd::holds_alternative<nostd::string_view>(message))
    {
      body_ = static_cast<std::string>(nostd::get<nostd::string_view>(message));
    }
  }

  void SetBody(const std::string &message) noexcept { body_ = message; }

  void SetEventId(int64_t id, nostd::string_view name) noexcept override
  {
    event_id_              = id;
    log_record_event_name_ = static_cast<std::string>(name);
  }

  void SetTraceId(const opentelemetry::trace::TraceId &trace_id) noexcept override
  {
    trace_id_ = trace_id;
  }
  inline const opentelemetry::trace::TraceId &GetTraceId() const noexcept { return trace_id_; }

  void SetSpanId(const opentelemetry::trace::SpanId &span_id) noexcept override
  {
    span_id_ = span_id;
  }

  inline const opentelemetry::trace::SpanId &GetSpanId() const noexcept { return span_id_; }

  void SetTraceFlags(const opentelemetry::trace::TraceFlags &trace_flags) noexcept override
  {
    trace_flags_ = trace_flags;
  }

  inline const opentelemetry::trace::TraceFlags &GetTraceFlags() const noexcept
  {
    return trace_flags_;
  }

  void SetAttribute(nostd::string_view key,
                    const opentelemetry::common::AttributeValue &value) noexcept override
  {
    if (!nostd::holds_alternative<nostd::string_view>(value))
    {
      return;
    }

    if (key == "event.domain")
    {
      event_domain_ = static_cast<std::string>(nostd::get<nostd::string_view>(value));
    }
    else if (key == "event.name")
    {
      event_name_ = static_cast<std::string>(nostd::get<nostd::string_view>(value));
    }
  }

  inline const std::string &GetEventName() const noexcept { return event_name_; }

  inline const std::string &GetEventDomain() const noexcept { return event_domain_; }

  void SetResource(const opentelemetry::sdk::resource::Resource &) noexcept override {}

  void SetInstrumentationScope(
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope &) noexcept override
  {}

  void CopyFrom(const MockLogRecordable &other)
  {
    severity_           = other.severity_;
    body_               = other.body_;
    trace_id_           = other.trace_id_;
    span_id_            = other.span_id_;
    trace_flags_        = other.trace_flags_;
    event_name_         = other.event_name_;
    event_domain_       = other.event_domain_;
    observed_timestamp_ = other.observed_timestamp_;
  }

private:
  opentelemetry::logs::Severity severity_ = opentelemetry::logs::Severity::kInvalid;
  std::string body_;
  int64_t event_id_;
  std::string log_record_event_name_;
  opentelemetry::trace::TraceId trace_id_;
  opentelemetry::trace::SpanId span_id_;
  opentelemetry::trace::TraceFlags trace_flags_;
  std::string event_name_;
  std::string event_domain_;
  opentelemetry::common::SystemTimestamp observed_timestamp_ =
      std::chrono::system_clock::from_time_t(0);
};

class MockProcessor final : public LogRecordProcessor
{
private:
  std::shared_ptr<MockLogRecordable> record_received_;

public:
  // A processor used for testing that keeps a track of the recordable it received
  explicit MockProcessor(std::shared_ptr<MockLogRecordable> record_received) noexcept
      : record_received_(std::move(record_received))
  {}

  std::unique_ptr<opentelemetry::sdk::logs::Recordable> MakeRecordable() noexcept override
  {
    return std::unique_ptr<opentelemetry::sdk::logs::Recordable>(new MockLogRecordable());
  }

  // OnEmit stores the record it receives into the shared_ptr recordable passed into its
  // constructor
  void OnEmit(std::unique_ptr<opentelemetry::sdk::logs::Recordable> &&record) noexcept override
  {
    // Cast the recordable received into a concrete MockLogRecordable type
    auto copy =
        std::shared_ptr<MockLogRecordable>(static_cast<MockLogRecordable *>(record.release()));

    // Copy over the received log record's severity, name, and body fields over to the recordable
    // passed in the constructor
    record_received_->CopyFrom(*copy);
  }

  bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override { return true; }
  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override { return true; }
};

TEST(LoggerSDK, LogToAProcessor)
{
  // Create an API LoggerProvider and logger
  auto api_lp = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = api_lp->GetLogger("logger", "opentelelemtry_library", "", schema_url);

  // Cast the API LoggerProvider to an SDK Logger Provider and assert that it is still the same
  // LoggerProvider by checking that getting a logger with the same name as the previously defined
  // logger is the same instance
  auto lp      = static_cast<LoggerProvider *>(api_lp.get());
  auto logger2 = lp->GetLogger("logger", "opentelelemtry_library", "", schema_url);
  ASSERT_EQ(logger, logger2);

  nostd::shared_ptr<opentelemetry::trace::Span> include_span;
  {
    std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>> span_processors;
    auto trace_provider =
        opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(span_processors));
    include_span = trace_provider->GetTracer("logger")->StartSpan("test_logger");
  }
  opentelemetry::trace::Scope trace_scope{include_span};

  auto now = std::chrono::system_clock::now();

  auto sdk_logger = static_cast<opentelemetry::sdk::logs::Logger *>(logger.get());
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetName(), "opentelelemtry_library");
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetVersion(), "");
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetSchemaURL(), schema_url);
  // Set a processor for the LoggerProvider
  auto shared_recordable = std::shared_ptr<MockLogRecordable>(new MockLogRecordable());
  lp->AddProcessor(std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>(
      new MockProcessor(shared_recordable)));

  // Check that the recordable created by the EmitLogRecord() statement is set properly
  logger->EmitLogRecord(logs_api::Severity::kWarn, "Log Message");

  ASSERT_EQ(shared_recordable->GetSeverity(), logs_api::Severity::kWarn);
  ASSERT_EQ(shared_recordable->GetBody(), "Log Message");
  ASSERT_EQ(shared_recordable->GetTraceFlags().flags(),
            include_span->GetContext().trace_flags().flags());
  char trace_id_in_logger[opentelemetry::trace::TraceId::kSize * 2];
  char trace_id_in_span[opentelemetry::trace::TraceId::kSize * 2];
  char span_id_in_logger[opentelemetry::trace::SpanId::kSize * 2];
  char span_id_in_span[opentelemetry::trace::SpanId::kSize * 2];
  shared_recordable->GetTraceId().ToLowerBase16(trace_id_in_logger);
  include_span->GetContext().trace_id().ToLowerBase16(trace_id_in_span);
  shared_recordable->GetSpanId().ToLowerBase16(span_id_in_logger);
  include_span->GetContext().span_id().ToLowerBase16(span_id_in_span);
  std::string trace_id_text_in_logger{trace_id_in_logger, sizeof(trace_id_in_logger)};
  std::string trace_id_text_in_span{trace_id_in_span, sizeof(trace_id_in_span)};
  std::string span_id_text_in_logger{span_id_in_logger, sizeof(span_id_in_logger)};
  std::string span_id_text_in_span{span_id_in_span, sizeof(span_id_in_span)};
  ASSERT_EQ(trace_id_text_in_logger, trace_id_text_in_span);
  ASSERT_EQ(span_id_text_in_logger, span_id_text_in_span);

  ASSERT_GE(
      static_cast<std::chrono::system_clock::time_point>(shared_recordable->GetObservedTimestamp()),
      now);
}

TEST(LoggerSDK, LoggerWithDisabledConfig)
{
  ScopeConfigurator<LoggerConfig> disabled_all_scopes =
      ScopeConfigurator<LoggerConfig>::Builder(LoggerConfig::Disabled()).Build();
  // Set a processor for the LoggerProvider
  auto shared_recordable = std::shared_ptr<MockLogRecordable>(new MockLogRecordable());
  auto log_processor = std::unique_ptr<LogRecordProcessor>(new MockProcessor(shared_recordable));

  // Create an API LoggerProvider and logger
  const auto resource = opentelemetry::sdk::resource::Resource::Create({});
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto api_lp = std::shared_ptr<logs_api::LoggerProvider>(
      new LoggerProvider(std::move(log_processor), resource,
                         std::make_unique<ScopeConfigurator<LoggerConfig>>(disabled_all_scopes)));
  auto logger = api_lp->GetLogger("logger", "opentelelemtry_library", "", schema_url);

  auto noop_logger = logs_api::NoopLogger();

  // Test Logger functions for the constructed logger
  // This logger should behave like a noop logger
  ASSERT_EQ(logger->GetName(), noop_logger.GetName());

  // Since the logger is disabled, when creating a LogRecord, the observed timestamp will not be
  // set in the underlying LogRecordable
  auto log_record = logger->CreateLogRecord();
  logger->EmitLogRecord(std::move(log_record));
  ASSERT_EQ(shared_recordable->GetObservedTimestamp(), std::chrono::system_clock::from_time_t(0));

  // Since this logger should behave like a noop logger, no values within the recordable would be
  // set.
  logger->EmitLogRecord(logs_api::Severity::kWarn, "Log Message");
  ASSERT_EQ(shared_recordable->GetBody(), "");
  ASSERT_EQ(shared_recordable->GetSeverity(), opentelemetry::logs::Severity::kInvalid);
  ASSERT_EQ(shared_recordable->GetObservedTimestamp(), std::chrono::system_clock::from_time_t(0));
}

TEST(LoggerSDK, LoggerWithEnabledConfig)
{
  ScopeConfigurator<LoggerConfig> enabled_all_scopes =
      ScopeConfigurator<LoggerConfig>::Builder(LoggerConfig::Enabled()).Build();
  // Set a processor for the LoggerProvider
  auto shared_recordable = std::shared_ptr<MockLogRecordable>(new MockLogRecordable());
  auto log_processor = std::unique_ptr<LogRecordProcessor>(new MockProcessor(shared_recordable));

  // Create an API LoggerProvider and logger
  const auto resource = opentelemetry::sdk::resource::Resource::Create({});
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto api_lp = std::shared_ptr<logs_api::LoggerProvider>(
      new LoggerProvider(std::move(log_processor), resource,
                         std::make_unique<ScopeConfigurator<LoggerConfig>>(enabled_all_scopes)));
  auto logger = api_lp->GetLogger("test-logger", "opentelemetry_library", "", schema_url);

  // Test Logger functions for the constructed logger
  ASSERT_EQ(logger->GetName(), "test-logger");

  // Since the logger is enabled, when creating a LogRecord, the observed timestamp will be set
  // in the underlying LogRecordable.
  auto reference_ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  auto log_record = logger->CreateLogRecord();
  logger->EmitLogRecord(std::move(log_record));
  // Since log_record was created after recording reference timestamp, expect that observed
  // timestamp is greater
  ASSERT_GE(shared_recordable->GetObservedTimestamp().time_since_epoch().count(),
            reference_ts.count());

  // Since this logger should behave like a valid logger, values within the recordable would be set.
  logger->EmitLogRecord(logs_api::Severity::kWarn, "Log Message");
  ASSERT_EQ(shared_recordable->GetBody(), "Log Message");
  ASSERT_EQ(shared_recordable->GetSeverity(), opentelemetry::logs::Severity::kWarn);
  ASSERT_GE(shared_recordable->GetObservedTimestamp().time_since_epoch().count(),
            reference_ts.count());
}

static std::unique_ptr<MockLogRecordable> create_mock_log_recordable(
    const std::string &body,
    opentelemetry::logs::Severity severity)
{
  auto mock_log_recordable = std::make_unique<MockLogRecordable>();
  mock_log_recordable->SetBody(body);
  mock_log_recordable->SetSeverity(severity);
  return mock_log_recordable;
}

class CustomLogConfiguratorTestData
{
public:
  InstrumentationScope instrumentation_scope_;
  MockLogRecordable test_log_recordable_;
  MockLogRecordable expected_log_recordable_;
  bool expected_disabled_for_scope_;

  CustomLogConfiguratorTestData(const InstrumentationScope &instrumentation_scope,
                                const MockLogRecordable &test_log_recordable,
                                const MockLogRecordable &expected_log_recordable,
                                const bool expected_disabled_for_scope)
      : instrumentation_scope_(instrumentation_scope),
        test_log_recordable_(test_log_recordable),
        expected_log_recordable_(expected_log_recordable),
        expected_disabled_for_scope_(expected_disabled_for_scope)
  {}
};

// constants used in VerifyCustomConfiguratorBehavior test
static auto noop_logger = logs_api::NoopLogger();
const std::string schema{"https://opentelemetry.io/schemas/1.11.0"};

// Generate test case data
// Test Case 1
static auto instrumentation_scope_1 =
    *InstrumentationScope::Create("opentelemetry_library", "1.0.0", schema);
static auto test_log_recordable_1 =
    create_mock_log_recordable("Log Message", opentelemetry::logs::Severity::kWarn);
static auto expected_log_recordable_1 =
    create_mock_log_recordable("Log Message", opentelemetry::logs::Severity::kWarn);
static auto custom_log_configurator_test_data_1 =
    CustomLogConfiguratorTestData(instrumentation_scope_1,
                                  *test_log_recordable_1,
                                  *expected_log_recordable_1,
                                  false);
// Test Case 2
static auto instrumentation_scope_2 = *InstrumentationScope::Create("bar_library", "1.0.0", schema);
static auto test_log_recordable_2 =
    create_mock_log_recordable("", opentelemetry::logs::Severity::kDebug);
static auto expected_log_recordable_2 =
    create_mock_log_recordable("", opentelemetry::logs::Severity::kDebug);
static auto custom_log_configurator_test_data_2 =
    CustomLogConfiguratorTestData(instrumentation_scope_2,
                                  *test_log_recordable_2,
                                  *expected_log_recordable_2,
                                  false);
// Test Case 3
static auto instrumentation_scope_3 = *InstrumentationScope::Create("foo_library", "", schema);
static auto test_log_recordable_3 =
    create_mock_log_recordable("Info message", opentelemetry::logs::Severity::kInfo);
static auto expected_log_recordable_3 =
    create_mock_log_recordable("", opentelemetry::logs::Severity::kInvalid);
static auto custom_log_configurator_test_data_3 =
    CustomLogConfiguratorTestData(instrumentation_scope_3,
                                  *test_log_recordable_3,
                                  *expected_log_recordable_3,
                                  true);
// Test Case 4
static auto instrumentation_scope_4 = *InstrumentationScope::Create("allowed_library", "", schema);
static auto test_log_recordable_4 =
    create_mock_log_recordable("Scope version missing", opentelemetry::logs::Severity::kInfo);
static auto expected_log_recordable_4 =
    create_mock_log_recordable("", opentelemetry::logs::Severity::kInvalid);
static auto custom_log_configurator_test_data_4 =
    CustomLogConfiguratorTestData(instrumentation_scope_4,
                                  *test_log_recordable_4,
                                  *expected_log_recordable_4,
                                  true);

// This array could also directly contain the reference types, but that  leads to 'uninitialized
// value was created by heap allocation' errors in Valgrind memcheck. This is a bug in Googletest
// library, see https://github.com/google/googletest/issues/3805#issuecomment-1397301790 for more
// details. Using pointers is a workaround to prevent the Valgrind warnings.
constexpr std::array<CustomLogConfiguratorTestData *, 4> log_configurator_test_cases = {
    &custom_log_configurator_test_data_1, &custom_log_configurator_test_data_2,
    &custom_log_configurator_test_data_3, &custom_log_configurator_test_data_4};

// Test fixture for VerifyCustomConfiguratorBehavior
class CustomLoggerConfiguratorTestFixture
    : public ::testing::TestWithParam<CustomLogConfiguratorTestData *>
{};

TEST_P(CustomLoggerConfiguratorTestFixture, VerifyCustomConfiguratorBehavior)
{
  // lambda checks if version is present in scope information
  auto check_if_version_present = [](const InstrumentationScope &scope_info) {
    return !scope_info.GetVersion().empty();
  };
  // custom scope configurator that only disables loggers for library name "foo_library" or do not
  // have version information
  auto test_scope_configurator = ScopeConfigurator<LoggerConfig>(
      ScopeConfigurator<LoggerConfig>::Builder(LoggerConfig::Disabled())
          .AddConditionNameEquals("foo_library", LoggerConfig::Disabled())
          .AddCondition(check_if_version_present, LoggerConfig::Enabled())
          .Build());

  // Get the test case data from fixture
  CustomLogConfiguratorTestData *test_case = GetParam();
  auto test_instrumentation_scope          = test_case->instrumentation_scope_;
  auto test_log_recordable                 = test_case->test_log_recordable_;

  // Set a processor for the LoggerProvider
  auto shared_recordable_under_test = std::shared_ptr<MockLogRecordable>(new MockLogRecordable());
  auto log_processor_test =
      std::unique_ptr<LogRecordProcessor>(new MockProcessor(shared_recordable_under_test));

  // Create an API LoggerProvider and logger
  const auto resource = opentelemetry::sdk::resource::Resource::Create({});
  auto api_lp         = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider(
      std::move(log_processor_test), resource,
      std::make_unique<ScopeConfigurator<LoggerConfig>>(test_scope_configurator)));

  // Create logger and make assertions
  auto logger_under_test = api_lp->GetLogger("test-logger", test_instrumentation_scope.GetName(),
                                             test_instrumentation_scope.GetVersion(),
                                             test_instrumentation_scope.GetSchemaURL());
  auto reference_ts      = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  auto log_record = logger_under_test->CreateLogRecord();
  logger_under_test->EmitLogRecord(std::move(log_record));

  // Test Logger functions for the constructed logger
  if (test_case->expected_disabled_for_scope_)
  {
    ASSERT_EQ(logger_under_test->GetName(), noop_logger.GetName());
    ASSERT_EQ(shared_recordable_under_test->GetObservedTimestamp(),
              std::chrono::system_clock::from_time_t(0));
  }
  else
  {
    ASSERT_EQ(logger_under_test->GetName(), "test-logger");
    ASSERT_GE(shared_recordable_under_test->GetObservedTimestamp().time_since_epoch().count(),
              reference_ts.count());
  }

  logger_under_test->EmitLogRecord(test_log_recordable.GetBody(),
                                   test_log_recordable.GetSeverity());
  ASSERT_EQ(shared_recordable_under_test->GetBody(), test_case->expected_log_recordable_.GetBody());
  ASSERT_EQ(shared_recordable_under_test->GetSeverity(),
            test_case->expected_log_recordable_.GetSeverity());
}

INSTANTIATE_TEST_SUITE_P(CustomLogConfiguratorTestData,
                         CustomLoggerConfiguratorTestFixture,
                         ::testing::ValuesIn(log_configurator_test_cases));

#if OPENTELEMETRY_ABI_VERSION_NO < 2
TEST(LoggerSDK, EventLog)
{
  // Create an API LoggerProvider and logger
  auto api_lp = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = api_lp->GetLogger("logger", "opentelelemtry_library", "", schema_url);

  auto api_elp      = std::shared_ptr<logs_api::EventLoggerProvider>(new EventLoggerProvider());
  auto event_logger = api_elp->CreateEventLogger(logger, "otel-cpp.event_domain");

  auto sdk_logger = static_cast<opentelemetry::sdk::logs::Logger *>(logger.get());
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetName(), "opentelelemtry_library");
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetVersion(), "");
  ASSERT_EQ(sdk_logger->GetInstrumentationScope().GetSchemaURL(), schema_url);
  // Set a processor for the LoggerProvider
  auto shared_recordable = std::shared_ptr<MockLogRecordable>(new MockLogRecordable());
  auto sdk_lp            = static_cast<LoggerProvider *>(api_lp.get());
  sdk_lp->AddProcessor(std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>(
      new MockProcessor(shared_recordable)));

  // Check that the recordable created by the EmitEvent() statement is set properly
  event_logger->EmitEvent("otel-cpp.event_name", logs_api::Severity::kWarn, "Event Log Message");

  ASSERT_EQ(shared_recordable->GetSeverity(), logs_api::Severity::kWarn);
  ASSERT_EQ(shared_recordable->GetBody(), "Event Log Message");
  ASSERT_EQ(shared_recordable->GetEventName(), "otel-cpp.event_name");
  ASSERT_EQ(shared_recordable->GetEventDomain(), "otel-cpp.event_domain");
}
#endif
