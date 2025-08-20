// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/ext/http/client/http_client.h>
#include <opentelemetry/ext/http/client/http_client_factory.h>

TEST(ExtHttpCurlInstall, HttpClient)
{
  auto client = opentelemetry::ext::http::client::HttpClientFactory::Create();
  ASSERT_TRUE(client != nullptr);
}