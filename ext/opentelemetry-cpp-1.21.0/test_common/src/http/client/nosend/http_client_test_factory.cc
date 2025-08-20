// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/test_common/ext/http/client/http_client_test_factory.h"
#include "opentelemetry/ext/http/client/http_client.h"
#include "opentelemetry/test_common/ext/http/client/nosend/http_client_nosend.h"

namespace http_client = opentelemetry::ext::http::client;

std::shared_ptr<http_client::HttpClient> http_client::HttpClientTestFactory::Create()
{
  return std::make_shared<http_client::nosend::HttpClient>();
}
