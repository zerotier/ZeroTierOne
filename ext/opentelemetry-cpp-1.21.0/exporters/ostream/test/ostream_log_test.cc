// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/ostream/log_record_exporter.h"
#include "opentelemetry/exporters/ostream/log_record_exporter_factory.h"
#include "opentelemetry/logs/event_id.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/utility.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/provider.h"
#include "opentelemetry/sdk/logs/read_write_log_record.h"
#include "opentelemetry/sdk/logs/readable_log_record.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/version.h"

namespace sdklogs      = opentelemetry::sdk::logs;
namespace logs_api     = opentelemetry::logs;
namespace nostd        = opentelemetry::nostd;
namespace exporterlogs = opentelemetry::exporter::logs;
namespace common       = opentelemetry::common;

using Attributes = std::initializer_list<std::pair<nostd::string_view, common::AttributeValue>>;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace logs
{

namespace
{
static opentelemetry::sdk::instrumentationscope::InstrumentationScope GetTestInstrumentationScope()
{
  opentelemetry::sdk::instrumentationscope::InstrumentationScope result =
      opentelemetry::sdk::logs::ReadableLogRecord::GetDefaultInstrumentationScope();

  result.SetAttribute("scope.attr.key", "scope.attr.value");

  return result;
}
}  // namespace

// Test that when OStream Log exporter is shutdown, no logs should be sent to stream
TEST(OStreamLogRecordExporter, Shutdown)
{
  auto exporter =
      std::unique_ptr<sdklogs::LogRecordExporter>(new exporterlogs::OStreamLogRecordExporter);

  // Save cerr original buffer here
  std::streambuf *original = std::cerr.rdbuf();

  // Redirect cerr to our stringstream buffer
  std::stringstream output;
  std::cerr.rdbuf(output.rdbuf());

  EXPECT_TRUE(exporter->Shutdown());

  // After processor/exporter is shutdown, no logs should be sent to stream
  auto record = exporter->MakeRecordable();
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())->SetBody("Log record not empty");
  exporter->Export(nostd::span<std::unique_ptr<sdklogs::Recordable>>(&record, 1));

  // Restore original stringstream buffer
  std::cerr.rdbuf(original);
  std::string err_message =
      "[Ostream Log Exporter] Exporting 1 log(s) failed, exporter is shutdown";
  EXPECT_TRUE(output.str().find(err_message) != std::string::npos);
}

// ---------------------------------- Print to cout -------------------------

// Testing what a default log record that has no values changed will print out
// This function tests MakeRecordable() as well as Export().
TEST(OstreamLogExporter, DefaultLogRecordToCout)
{
  auto exporter = std::unique_ptr<sdklogs::LogRecordExporter>(
      new exporterlogs::OStreamLogRecordExporter(std::cout));

  // Save cout's original buffer here
  std::streambuf *original = std::cout.rdbuf();

  // Redirect cout to our stringstream buffer
  std::stringstream output;
  std::cout.rdbuf(output.rdbuf());

  // Pass a default recordable created by the exporter to be exported
  auto log_record = exporter->MakeRecordable();
  opentelemetry::sdk::instrumentationscope::InstrumentationScope instrumentation_scope =
      GetTestInstrumentationScope();
  log_record->SetInstrumentationScope(instrumentation_scope);
  exporter->Export(nostd::span<std::unique_ptr<sdklogs::Recordable>>(&log_record, 1));

  // Restore cout's original stringstream
  std::cout.rdbuf(original);

  std::vector<std::string> expected_output{
      "{\n",
      "  timestamp          : 0\n",
      "  severity_num       : 0\n",
      "  severity_text      : INVALID\n",
      "  body               : \n",
      "  resource           : \n",
      "    telemetry.sdk.version: " OPENTELEMETRY_VERSION "\n",
      "    telemetry.sdk.name: opentelemetry\n",
      "    telemetry.sdk.language: cpp\n",
      "  attributes         : \n",
      "  event_id           : 0\n",
      "  event_name         : \n",
      "  trace_id           : 00000000000000000000000000000000\n",
      "  span_id            : 0000000000000000\n",
      "  trace_flags        : 00\n",
      "  scope              : \n",
      "    name             : otel-cpp\n",
      "    version          : " OPENTELEMETRY_SDK_VERSION "\n",
      "    schema_url       : https://opentelemetry.io/schemas/1.15.0\n",
      "    attributes       : \n",
      "      scope.attr.key: scope.attr.value\n",
      "}\n"};

  std::string ostream_output = output.str();
  for (auto &expected : expected_output)
  {
    std::string::size_type result = ostream_output.find(expected);
    if (result == std::string::npos)
    {
      std::cout << "Can not find: \"" << expected << "\" in\n" << ostream_output << '\n';
    }
    ASSERT_NE(result, std::string::npos);
  }
}

// Testing what a log record with only the "timestamp", "severity", "name" and "message" fields set,
// will print out
TEST(OStreamLogRecordExporter, SimpleLogToCout)
{
  // Initialize an Ostream exporter to std::cout
  auto exporter = std::unique_ptr<sdklogs::LogRecordExporter>(
      new exporterlogs::OStreamLogRecordExporter(std::cout));

  // Save original stream buffer, then redirect cout to our new stream buffer
  std::streambuf *original = std::cout.rdbuf();
  std::stringstream output;
  std::cout.rdbuf(output.rdbuf());

  // Pass a default recordable created by the exporter to be exported
  // Create a log record and manually timestamp, severity, name, message
  common::SystemTimestamp now(std::chrono::system_clock::now());

  auto record = std::unique_ptr<sdklogs::Recordable>(new sdklogs::ReadWriteLogRecord());
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())->SetTimestamp(now);
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())->SetObservedTimestamp(now);
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())
      ->SetSeverity(logs_api::Severity::kTrace);  // kTrace has enum value of 1
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())->SetBody("Message");

  opentelemetry::sdk::instrumentationscope::InstrumentationScope instrumentation_scope =
      GetTestInstrumentationScope();
  record->SetInstrumentationScope(instrumentation_scope);

  // Log a record to cout
  exporter->Export(nostd::span<std::unique_ptr<sdklogs::Recordable>>(&record, 1));

  // Reset cout's original stringstream buffer
  std::cout.rdbuf(original);

  std::vector<std::string> expected_output{
      "{\n",
      "  timestamp          : " + std::to_string(now.time_since_epoch().count()) +
          "\n"
          "  observed_timestamp : " +
          std::to_string(now.time_since_epoch().count()) +
          "\n"
          "  severity_num       : 1\n"
          "  severity_text      : TRACE\n"
          "  body               : Message\n",
      "  resource           : \n",
      "    telemetry.sdk.version: " OPENTELEMETRY_VERSION "\n",
      "    telemetry.sdk.name: opentelemetry\n",
      "    telemetry.sdk.language: cpp\n",
      "  attributes         : \n",
      "  event_id           : 0\n",
      "  event_name         : \n",
      "  trace_id           : 00000000000000000000000000000000\n",
      "  span_id            : 0000000000000000\n",
      "  trace_flags        : 00\n",
      "  scope              : \n",
      "    name             : otel-cpp\n",
      "    version          : " OPENTELEMETRY_SDK_VERSION "\n",
      "    schema_url       : https://opentelemetry.io/schemas/1.15.0\n",
      "    attributes       : \n",
      "      scope.attr.key: scope.attr.value\n",
      "}\n"};

  std::string ostream_output = output.str();
  for (auto &expected : expected_output)
  {
    std::string::size_type result = ostream_output.find(expected);
    if (result == std::string::npos)
    {
      std::cout << "Can not find: \"" << expected << "\" in\n" << ostream_output << '\n';
    }
    ASSERT_NE(result, std::string::npos);
  }
}

// ---------------------------------- Print to cerr --------------------------

// Testing what a log record with only the "resource" and "attributes" fields
// (i.e. KeyValueIterable types) set with primitive types, will print out
TEST(OStreamLogRecordExporter, LogWithStringAttributesToCerr)
{
  // Initialize an Ostream exporter to cerr
  auto exporter = std::unique_ptr<sdklogs::LogRecordExporter>(
      new exporterlogs::OStreamLogRecordExporter(std::cerr));

  // Save original stream buffer, then redirect cout to our new stream buffer
  std::streambuf *original = std::cerr.rdbuf();
  std::stringstream stdcerrOutput;
  std::cerr.rdbuf(stdcerrOutput.rdbuf());

  // Pass a recordable created by the exporter to be exported
  auto record = exporter->MakeRecordable();

  // Set resources for this log record only of type <string, string>
  auto resource = opentelemetry::sdk::resource::Resource::Create({{"key1", "val1"}});
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())->SetResource(resource);

  // Set attributes to this log record of type <string, AttributeValue>
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())->SetAttribute("a", true);

  opentelemetry::sdk::instrumentationscope::InstrumentationScope instrumentation_scope =
      GetTestInstrumentationScope();
  record->SetInstrumentationScope(instrumentation_scope);

  // Log record to cerr
  exporter->Export(nostd::span<std::unique_ptr<sdklogs::Recordable>>(&record, 1));

  // Reset cerr's original stringstream buffer
  std::cerr.rdbuf(original);

  std::vector<std::string> expected_output{
      "{\n",
      "  timestamp          : 0\n",
      "  severity_num       : 0\n",
      "  severity_text      : INVALID\n",
      "  body               : \n",
      "  resource           : \n",
      "    telemetry.sdk.version: " OPENTELEMETRY_VERSION "\n",
      "    telemetry.sdk.name: opentelemetry\n",
      "    telemetry.sdk.language: cpp\n",
      "    service.name: unknown_service\n",
      "    key1: val1\n",
      "  attributes         : \n",
      "    a: 1\n",
      "  event_id           : 0\n",
      "  event_name         : \n",
      "  trace_id           : 00000000000000000000000000000000\n",
      "  span_id            : 0000000000000000\n",
      "  trace_flags        : 00\n",
      "  scope              : \n",
      "    name             : otel-cpp\n",
      "    version          : " OPENTELEMETRY_SDK_VERSION "\n",
      "    schema_url       : https://opentelemetry.io/schemas/1.15.0\n",
      "    attributes       : \n",
      "      scope.attr.key: scope.attr.value\n",
      "}\n"};

  std::string ostream_output = stdcerrOutput.str();
  for (auto &expected : expected_output)
  {
    std::string::size_type result = ostream_output.find(expected);
    if (result == std::string::npos)
    {
      std::cout << "Can not find: \"" << expected << "\" in\n" << ostream_output << '\n';
    }
    ASSERT_NE(result, std::string::npos);
  }
}

// ---------------------------------- Print to clog -------------------------

// Testing what a log record with only the "resource", and "attributes" fields
// (i.e. KeyValueIterable types), set with 2D arrays as values, will print out
TEST(OStreamLogRecordExporter, LogWithVariantTypesToClog)
{

  // Initialize an Ostream exporter to cerr
  auto exporter = std::unique_ptr<sdklogs::LogRecordExporter>(
      new exporterlogs::OStreamLogRecordExporter(std::clog));

  // Save original stream buffer, then redirect cout to our new stream buffer
  std::streambuf *original = std::clog.rdbuf();
  std::stringstream stdclogOutput;
  std::clog.rdbuf(stdclogOutput.rdbuf());

  // Pass a recordable created by the exporter to be exported
  auto record = exporter->MakeRecordable();

  // Set resources for this log record of only integer types as the value
  std::array<int, 3> array1 = {1, 2, 3};
  nostd::span<int> data1{array1.data(), array1.size()};

  auto resource = opentelemetry::sdk::resource::Resource::Create({{"res1", data1}});
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())->SetResource(resource);

  // Set resources for this log record of bool types as the value
  // e.g. key/value is a par of type <string, array of bools>
  std::array<bool, 3> array = {false, true, false};
  static_cast<sdklogs::ReadWriteLogRecord *>(record.get())
      ->SetAttribute("attr1", nostd::span<bool>{array.data(), array.size()});

  opentelemetry::sdk::instrumentationscope::InstrumentationScope instrumentation_scope =
      GetTestInstrumentationScope();
  record->SetInstrumentationScope(instrumentation_scope);

  // Log a record to clog
  exporter->Export(nostd::span<std::unique_ptr<sdklogs::Recordable>>(&record, 1));

  // Reset clog's original stringstream buffer
  std::clog.rdbuf(original);

  std::vector<std::string> expected_output{
      "{\n",
      "  timestamp          : 0\n",
      "  severity_num       : 0\n",
      "  severity_text      : INVALID\n",
      "  body               : \n",
      "  resource           : \n",
      "    service.name: unknown_service\n",
      "    telemetry.sdk.version: " OPENTELEMETRY_VERSION "\n",
      "    telemetry.sdk.name: opentelemetry\n",
      "    telemetry.sdk.language: cpp\n",
      "    res1: [1,2,3]\n",
      "  attributes         : \n",
      "    attr1: [0,1,0]\n",
      "  event_id           : 0\n",
      "  event_name         : \n",
      "  trace_id           : 00000000000000000000000000000000\n",
      "  span_id            : 0000000000000000\n",
      "  trace_flags        : 00\n",
      "  scope              : \n",
      "    name             : otel-cpp\n",
      "    version          : " OPENTELEMETRY_SDK_VERSION "\n",
      "    schema_url       : https://opentelemetry.io/schemas/1.15.0\n",
      "    attributes       : \n",
      "      scope.attr.key: scope.attr.value\n",
      "}\n"};

  std::string ostream_output = stdclogOutput.str();
  for (auto &expected : expected_output)
  {
    std::string::size_type result = ostream_output.find(expected);
    if (result == std::string::npos)
    {
      std::cout << "Can not find: \"" << expected << "\" in\n" << ostream_output << '\n';
    }
    ASSERT_NE(result, std::string::npos);
  }
}

// // ---------------------------------- Integration Tests -------------------------

// Test using the simple log processor and ostream exporter to cout
// and use the rest of the logging pipeline (Logger, LoggerProvider, Provider) as well
TEST(OStreamLogRecordExporter, IntegrationTest)
{
  // Initialize a logger
  auto exporter =
      std::unique_ptr<sdklogs::LogRecordExporter>(new exporterlogs::OStreamLogRecordExporter);
  auto sdkProvider = std::shared_ptr<sdklogs::LoggerProvider>(new sdklogs::LoggerProvider());
  sdkProvider->AddProcessor(std::unique_ptr<sdklogs::LogRecordProcessor>(
      new sdklogs::SimpleLogRecordProcessor(std::move(exporter))));
  auto apiProvider = nostd::shared_ptr<logs_api::LoggerProvider>(sdkProvider);
  auto provider    = nostd::shared_ptr<logs_api::LoggerProvider>(apiProvider);
  sdklogs::Provider::SetLoggerProvider(provider);
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = logs_api::Provider::GetLoggerProvider()->GetLogger(
      "Logger", "opentelelemtry_library", OPENTELEMETRY_SDK_VERSION, schema_url,
      {{"scope.attr.key", 123}});

  // Back up cout's streambuf
  std::streambuf *original = std::cout.rdbuf();

  // Redirect cout to our string stream
  std::stringstream stdcoutOutput;
  std::cout.rdbuf(stdcoutOutput.rdbuf());

  // Write a log to ostream exporter
  common::SystemTimestamp now(std::chrono::system_clock::now());
  logger->EmitLogRecord(logs_api::Severity::kDebug, "Hello", now);

  // Restore cout's original streambuf
  std::cout.rdbuf(original);

  // Compare actual vs expected outputs
  std::vector<std::string> expected_output{
      "{\n",
      "  timestamp          : " + std::to_string(now.time_since_epoch().count()) + "\n",
      "  severity_num       : 5\n",
      "  severity_text      : DEBUG\n",
      "  body               : Hello\n",
      "  resource           : \n",
      "    telemetry.sdk.version: " OPENTELEMETRY_VERSION "\n",
      "    service.name: unknown_service\n",
      "    telemetry.sdk.name: opentelemetry\n",
      "    telemetry.sdk.language: cpp\n",
      "  attributes         : \n",
      "  event_id           : 0\n",
      "  event_name         : \n",
      "  trace_id           : 00000000000000000000000000000000\n",
      "  span_id            : 0000000000000000\n",
      "  trace_flags        : 00\n",
      "  scope              : \n",
      "    name             : opentelelemtry_library\n",
      "    version          : " OPENTELEMETRY_SDK_VERSION "\n",
      "    schema_url       : https://opentelemetry.io/schemas/1.11.0\n",
      "    attributes       : \n",
      "      scope.attr.key: 123\n",
      "}\n"};

  std::string ostream_output = stdcoutOutput.str();
  for (auto &expected : expected_output)
  {
    std::string::size_type result = ostream_output.find(expected);
    if (result == std::string::npos)
    {
      std::cout << "Can not find: \"" << expected << "\" in\n" << ostream_output << '\n';
    }
    ASSERT_NE(result, std::string::npos);
  }
}

// Test using the simple log processor and ostream exporter to cout
// and use the rest of the logging pipeline (Logger, LoggerProvider, Provider) and user-facing API
// as well
TEST(OStreamLogRecordExporter, IntegrationTestWithEventId)
{
  // Initialize a logger
  auto exporter =
      std::unique_ptr<sdklogs::LogRecordExporter>(new exporterlogs::OStreamLogRecordExporter);
  auto sdkProvider = std::shared_ptr<sdklogs::LoggerProvider>(new sdklogs::LoggerProvider());
  sdkProvider->AddProcessor(std::unique_ptr<sdklogs::LogRecordProcessor>(
      new sdklogs::SimpleLogRecordProcessor(std::move(exporter))));
  auto apiProvider = nostd::shared_ptr<logs_api::LoggerProvider>(sdkProvider);
  auto provider    = nostd::shared_ptr<logs_api::LoggerProvider>(apiProvider);
  sdklogs::Provider::SetLoggerProvider(provider);
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = logs_api::Provider::GetLoggerProvider()->GetLogger(
      "Logger", "opentelelemtry_library", OPENTELEMETRY_SDK_VERSION, schema_url,
      {{"scope.attr.key", 123}});

  // Back up cout's streambuf
  std::streambuf *original = std::cout.rdbuf();

  // Redirect cout to our string stream
  std::stringstream stdcoutOutput;
  std::cout.rdbuf(stdcoutOutput.rdbuf());

  logs_api::EventId event_id{12345678, "test_event_id"};

  // Write a log to ostream exporter
  logger->Debug(event_id, "Hello {key1} {key2}",
                common::MakeKeyValueIterableView<Attributes>({{"key1", 123}, {"key2", "value2"}}));

  // Restore cout's original streambuf
  std::cout.rdbuf(original);

  // Compare actual vs expected outputs
  std::vector<std::string> expected_output{
      "  severity_num       : 5\n",
      "  severity_text      : DEBUG\n",
      "  body               : Hello {key1} {key2}\n",
      "  resource           : \n",
      "    telemetry.sdk.version: " OPENTELEMETRY_VERSION "\n",
      "    service.name: unknown_service\n",
      "    telemetry.sdk.name: opentelemetry\n",
      "    telemetry.sdk.language: cpp\n",
      "  attributes         : \n",
      "  event_id           : 12345678\n",
      "  event_name         : test_event_id\n",
      "  trace_id           : 00000000000000000000000000000000\n",
      "  span_id            : 0000000000000000\n",
      "  trace_flags        : 00\n",
      "  scope              : \n",
      "    name             : opentelelemtry_library\n",
      "    version          : " OPENTELEMETRY_SDK_VERSION "\n",
      "    schema_url       : https://opentelemetry.io/schemas/1.11.0\n",
      "    attributes       : \n",
      "      scope.attr.key: 123\n",
      "}\n"};

  std::string ostream_output = stdcoutOutput.str();
  for (auto &expected : expected_output)
  {
    std::string::size_type result = ostream_output.find(expected);
    if (result == std::string::npos)
    {
      std::cout << "Can not find: \"" << expected << "\" in\n" << ostream_output << '\n';
    }
    ASSERT_NE(result, std::string::npos);
  }
}

// Test using the factory to create the ostream exporter
TEST(OStreamLogRecordExporter, Factory)
{
  auto exporter = OStreamLogRecordExporterFactory::Create();
  ASSERT_NE(exporter, nullptr);
}

}  // namespace logs
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
