// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <string>

#include "opentelemetry/plugin/dynamic_load.h"
#include "opentelemetry/plugin/factory.h"

TEST(LoadFactoryTest, FailureTest)
{
  std::string error_message;
  auto factory = opentelemetry::plugin::LoadFactory("no-such-plugin", error_message);
  EXPECT_EQ(factory, nullptr);
  EXPECT_FALSE(error_message.empty());
}
