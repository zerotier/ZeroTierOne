// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>
#include <string>

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"

class GlobalLogHandlerChecker
{
public:
  GlobalLogHandlerChecker() {}
  ~GlobalLogHandlerChecker()
  {
    using opentelemetry::sdk::common::internal_log::GlobalLogHandler;
    auto handle = GlobalLogHandler::GetLogHandler();
    if (handle && custom_handler_destroyed)
    {
      OTEL_INTERNAL_LOG_ERROR("GlobalLogHandler should be destroyed here");
      abort();
    }
    std::cout << "GlobalLogHandlerChecker destroyed and check pass.\n";
  }

  void Print() { std::cout << "GlobalLogHandlerChecker constructed\n"; }

  GlobalLogHandlerChecker(const GlobalLogHandlerChecker &)            = delete;
  GlobalLogHandlerChecker(GlobalLogHandlerChecker &&)                 = delete;
  GlobalLogHandlerChecker &operator=(const GlobalLogHandlerChecker &) = delete;
  GlobalLogHandlerChecker &operator=(GlobalLogHandlerChecker &&)      = delete;

  static bool custom_handler_destroyed;
};
bool GlobalLogHandlerChecker::custom_handler_destroyed = false;

namespace
{
static GlobalLogHandlerChecker &ConstructChecker()
{
  static GlobalLogHandlerChecker checker;
  return checker;
}
}  // namespace

class CustomLogHandler : public opentelemetry::sdk::common::internal_log::LogHandler
{
public:
  ~CustomLogHandler() override
  {
    GlobalLogHandlerChecker::custom_handler_destroyed = true;
    std::cout << "Custom Gobal Log Handle destroyed\n";
  }

  void Handle(opentelemetry::sdk::common::internal_log::LogLevel level,
              const char *,
              int,
              const char *msg,
              const opentelemetry::sdk::common::AttributeMap &) noexcept override
  {
    if (level == opentelemetry::sdk::common::internal_log::LogLevel::Debug)
    {
      std::cout << "Custom Gobal Log Handle[Debug]: " << msg << "\n";
    }
    else if (level == opentelemetry::sdk::common::internal_log::LogLevel::Error)
    {
      std::cout << "Custom Gobal Log Handle[Error]: " << msg << "\n";
    }
    else if (level == opentelemetry::sdk::common::internal_log::LogLevel::Info)
    {
      std::cout << "Custom Gobal Log Handle[Info]: " << msg << "\n";
    }
    else if (level == opentelemetry::sdk::common::internal_log::LogLevel::Warning)
    {
      std::cout << "Custom Gobal Log Handle[Warning]: " << msg << "\n";
    }
    ++count;
  }

  size_t count = 0;
};

TEST(GlobalLogHandleSingletonTest, Lifetime)
{
  // Setup a new static variable which will be destroyed after the global handle
  ConstructChecker().Print();

  using opentelemetry::sdk::common::internal_log::GlobalLogHandler;
  using opentelemetry::sdk::common::internal_log::LogHandler;

  auto custom_log_handler = opentelemetry::nostd::shared_ptr<LogHandler>(new CustomLogHandler{});
  opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogHandler(custom_log_handler);
  auto before_count = static_cast<CustomLogHandler *>(custom_log_handler.get())->count;
  opentelemetry::sdk::common::AttributeMap attributes = {
      {"url", "https://opentelemetry.io/"}, {"content-length", 0}, {"content-type", "text/html"}};
  OTEL_INTERNAL_LOG_ERROR("Error message");
  OTEL_INTERNAL_LOG_DEBUG("Debug message. Headers:", attributes);
  EXPECT_EQ(before_count + 1, static_cast<CustomLogHandler *>(custom_log_handler.get())->count);

  {
    auto handle = GlobalLogHandler::GetLogHandler();
    EXPECT_TRUE(!!handle);
  }
}
