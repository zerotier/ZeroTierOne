// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/ext/http/common/url_parser.h>

TEST(ExtCommonInstall, UrlParser)
{
  auto url_parser = opentelemetry::ext::http::common::UrlParser("www.opentelemetry.io");
  ASSERT_TRUE(url_parser.success_);
}