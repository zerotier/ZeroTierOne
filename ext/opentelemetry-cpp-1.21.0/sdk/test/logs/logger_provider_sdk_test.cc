// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/event_logger_provider.h"
#include "opentelemetry/sdk/logs/event_logger_provider_factory.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger.h"
#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/provider.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"

using namespace opentelemetry::sdk::logs;
namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace nostd    = opentelemetry::nostd;

TEST(LoggerProviderSDK, PushToAPI)
{
  auto lp =
      nostd::shared_ptr<logs_api::LoggerProvider>(new opentelemetry::sdk::logs::LoggerProvider());
  logs_sdk::Provider::SetLoggerProvider(lp);

  // Check that the loggerprovider was correctly pushed into the API
  ASSERT_EQ(lp, logs_api::Provider::GetLoggerProvider());
}

TEST(LoggerProviderSDK, LoggerProviderGetLoggerSimple)
{
  auto lp = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());

  nostd::string_view schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger1 = lp->GetLogger("logger1", "opentelelemtry_library", "", schema_url);
  auto logger2 = lp->GetLogger("logger2", "", "", schema_url);

  // Check that the logger is not nullptr
  ASSERT_NE(logger1, nullptr);
  ASSERT_NE(logger2, nullptr);

  auto sdk_logger1 = static_cast<opentelemetry::sdk::logs::Logger *>(logger1.get());
  auto sdk_logger2 = static_cast<opentelemetry::sdk::logs::Logger *>(logger2.get());
  ASSERT_EQ(sdk_logger1->GetInstrumentationScope().GetName(), "opentelelemtry_library");
  ASSERT_EQ(sdk_logger1->GetInstrumentationScope().GetVersion(), "");
  ASSERT_EQ(sdk_logger1->GetInstrumentationScope().GetSchemaURL(), schema_url);

  ASSERT_EQ(sdk_logger2->GetInstrumentationScope().GetName(), "logger2");
  ASSERT_EQ(sdk_logger2->GetInstrumentationScope().GetVersion(), "");
  ASSERT_EQ(sdk_logger2->GetInstrumentationScope().GetSchemaURL(), schema_url);

  // Check that two loggers with different names aren't the same instance
  ASSERT_NE(logger1, logger2);

  // Check that two loggers with the same name are the same instance
  auto logger3 = lp->GetLogger("logger1", "opentelelemtry_library", "", schema_url);
  ASSERT_EQ(logger1, logger3);
  auto sdk_logger3 = static_cast<opentelemetry::sdk::logs::Logger *>(logger3.get());
  ASSERT_EQ(sdk_logger3->GetInstrumentationScope(), sdk_logger1->GetInstrumentationScope());
}

TEST(LoggerProviderSDK, LoggerProviderLoggerArguments)
{
  auto lp = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());

  nostd::string_view schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger1 = lp->GetLogger("logger1", "opentelelemtry_library", "", schema_url);

  auto logger2     = lp->GetLogger("logger2", "opentelelemtry_library", "", schema_url);
  auto sdk_logger1 = static_cast<opentelemetry::sdk::logs::Logger *>(logger1.get());
  auto sdk_logger2 = static_cast<opentelemetry::sdk::logs::Logger *>(logger2.get());
  ASSERT_EQ(sdk_logger2->GetInstrumentationScope(), sdk_logger1->GetInstrumentationScope());

  auto logger3 = lp->GetLogger("logger3", "opentelelemtry_library", "", schema_url,
                               {{"scope_key1", "scope_value"}, {"scope_key2", 2}});

  auto sdk_logger3 = static_cast<opentelemetry::sdk::logs::Logger *>(logger3.get());
  EXPECT_EQ(sdk_logger3->GetInstrumentationScope().GetAttributes().size(), 2);
  {
    auto attibute = sdk_logger3->GetInstrumentationScope().GetAttributes().find("scope_key1");
    ASSERT_FALSE(attibute == sdk_logger3->GetInstrumentationScope().GetAttributes().end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::string>(attibute->second));
    EXPECT_EQ(opentelemetry::nostd::get<std::string>(attibute->second), "scope_value");
  }

  std::unordered_map<std::string, std::string> scope_attributes = {{"scope_key", "scope_value"}};
  auto logger4 =
      lp->GetLogger("logger4", "opentelelemtry_library", "", schema_url, scope_attributes);
  auto sdk_logger4 = static_cast<opentelemetry::sdk::logs::Logger *>(logger4.get());

  EXPECT_EQ(sdk_logger4->GetInstrumentationScope().GetAttributes().size(), 1);
  {
    auto attibute = sdk_logger4->GetInstrumentationScope().GetAttributes().find("scope_key");
    ASSERT_FALSE(attibute == sdk_logger4->GetInstrumentationScope().GetAttributes().end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::string>(attibute->second));
    EXPECT_EQ(opentelemetry::nostd::get<std::string>(attibute->second), "scope_value");
  }
}

#if OPENTELEMETRY_ABI_VERSION_NO < 2
TEST(LoggerProviderSDK, EventLoggerProviderFactory)
{
  auto elp = opentelemetry::sdk::logs::EventLoggerProviderFactory::Create();
  auto lp  = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());

  nostd::string_view schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger1 = lp->GetLogger("logger1", "opentelelemtry_library", "", schema_url);

  auto event_logger = elp->CreateEventLogger(logger1, "otel-cpp.test");
}
#endif

TEST(LoggerProviderSDK, LoggerEqualityCheck)
{
  auto lp = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());
  nostd::string_view schema_url{"https://opentelemetry.io/schemas/1.11.0"};

  auto logger1 = lp->GetLogger("logger1", "opentelelemtry_library", "", schema_url);
  auto logger2 = lp->GetLogger("logger1", "opentelelemtry_library", "", schema_url);
  EXPECT_EQ(logger1, logger2);

  auto logger3         = lp->GetLogger("logger3");
  auto another_logger3 = lp->GetLogger("logger3");
  EXPECT_EQ(logger3, another_logger3);

  auto logger4         = lp->GetLogger("logger4", "opentelelemtry_library", "1.0.0", schema_url);
  auto another_logger4 = lp->GetLogger("logger4", "opentelelemtry_library", "1.0.0", schema_url);
  EXPECT_EQ(logger4, another_logger4);

  auto logger5 =
      lp->GetLogger("logger5", "opentelelemtry_library", "1.0.0", schema_url, {{"key", "value"}});
  auto another_logger5 =
      lp->GetLogger("logger5", "opentelelemtry_library", "1.0.0", schema_url, {{"key", "value"}});
  EXPECT_EQ(logger5, another_logger5);
}

TEST(LoggerProviderSDK, GetLoggerInequalityCheck)
{
  auto lp               = std::shared_ptr<logs_api::LoggerProvider>(new LoggerProvider());
  auto logger_library_1 = lp->GetLogger("logger1", "library_1");
  auto logger_library_2 = lp->GetLogger("logger1", "library_2");
  auto logger_version_1 = lp->GetLogger("logger1", "library_1", "1.0.0");
  auto logger_version_2 = lp->GetLogger("logger1", "library_1", "2.0.0");
  auto logger_url_1     = lp->GetLogger("logger1", "library_1", "1.0.0", "url_1");
  auto logger_url_2     = lp->GetLogger("logger1", "library_1", "1.0.0", "url_2");
  auto logger_attribute_1 =
      lp->GetLogger("logger1", "library_1", "1.0.0", "url_1", {{"key", "one"}});
  auto logger_attribute_2 =
      lp->GetLogger("logger1", "library_1", "1.0.0", "url_1", {{"key", "two"}});

  // different scope names should return distinct loggers
  EXPECT_NE(logger_library_1, logger_library_2);

  // different scope versions should return distinct loggers
  EXPECT_NE(logger_version_1, logger_library_1);
  EXPECT_NE(logger_version_1, logger_version_2);

  // different scope schema urls should return distinct loggers
  EXPECT_NE(logger_url_1, logger_library_1);
  EXPECT_NE(logger_url_1, logger_version_1);
  EXPECT_NE(logger_url_1, logger_url_2);

  // different scope attributes should return distinct loggers
  EXPECT_NE(logger_attribute_1, logger_library_1);
  EXPECT_NE(logger_attribute_1, logger_url_1);
  EXPECT_NE(logger_attribute_1, logger_attribute_2);
}

class DummyLogRecordable final : public opentelemetry::sdk::logs::Recordable
{
public:
  void SetTimestamp(opentelemetry::common::SystemTimestamp) noexcept override {}

  void SetObservedTimestamp(opentelemetry::common::SystemTimestamp) noexcept override {}

  void SetSeverity(opentelemetry::logs::Severity) noexcept override {}

  void SetBody(const opentelemetry::common::AttributeValue &) noexcept override {}

  void SetEventId(int64_t, nostd::string_view) noexcept override {}

  void SetTraceId(const opentelemetry::trace::TraceId &) noexcept override {}

  void SetSpanId(const opentelemetry::trace::SpanId &) noexcept override {}

  void SetTraceFlags(const opentelemetry::trace::TraceFlags &) noexcept override {}

  void SetAttribute(nostd::string_view,
                    const opentelemetry::common::AttributeValue &) noexcept override
  {}

  void SetResource(const opentelemetry::sdk::resource::Resource &) noexcept override {}

  void SetInstrumentationScope(
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope &) noexcept override
  {}
};

class DummyProcessor : public LogRecordProcessor
{
  std::unique_ptr<Recordable> MakeRecordable() noexcept override
  {
    return std::unique_ptr<Recordable>(new DummyLogRecordable());
  }

  void OnEmit(std::unique_ptr<Recordable> && /* record */) noexcept override {}
  bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override { return true; }
  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override { return true; }
};

TEST(LoggerProviderSDK, GetResource)
{
  // Create a LoggerProvider without a processor
  auto resource = opentelemetry::sdk::resource::Resource::Create({{"key", "value"}});
  LoggerProvider lp{nullptr, resource};
  ASSERT_EQ(nostd::get<std::string>(lp.GetResource().GetAttributes().at("key")), "value");
}

TEST(LoggerProviderSDK, Shutdown)
{
  std::unique_ptr<SimpleLogRecordProcessor> processor(new SimpleLogRecordProcessor(nullptr));
  SimpleLogRecordProcessor *processor_ptr = processor.get();
  std::vector<std::unique_ptr<LogRecordProcessor>> processors;
  processors.push_back(std::move(processor));

  std::unique_ptr<LoggerContext> context(new LoggerContext(std::move(processors)));
  LoggerProvider lp(std::move(context));

  EXPECT_TRUE(lp.Shutdown());
  EXPECT_TRUE(processor_ptr->IsShutdown());

  // It's safe to shutdown again
  EXPECT_TRUE(lp.Shutdown());
}

TEST(LoggerProviderSDK, ForceFlush)
{
  std::unique_ptr<SimpleLogRecordProcessor> processor(new SimpleLogRecordProcessor(nullptr));
  std::vector<std::unique_ptr<LogRecordProcessor>> processors;
  processors.push_back(std::move(processor));

  std::unique_ptr<LoggerContext> context(new LoggerContext(std::move(processors)));
  LoggerProvider lp(std::move(context));

  EXPECT_TRUE(lp.ForceFlush());
}
