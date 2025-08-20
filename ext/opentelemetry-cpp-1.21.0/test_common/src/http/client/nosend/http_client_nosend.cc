// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "opentelemetry/ext/http/client/http_client.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/test_common/ext/http/client/nosend/http_client_nosend.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace ext
{
namespace http
{
namespace client
{
namespace nosend
{
void Request::ReplaceHeader(nostd::string_view name, nostd::string_view value) noexcept
{
  // erase matching headers
  auto range = headers_.equal_range(static_cast<std::string>(name));
  headers_.erase(range.first, range.second);
  AddHeader(name, value);
}

bool Response::ForEachHeader(
    nostd::function_ref<bool(nostd::string_view name, nostd::string_view value)> callable)
    const noexcept
{
  for (const auto &header : headers_)
  {
    if (!callable(header.first, header.second))
    {
      return false;
    }
  }
  return true;
}

bool Response::ForEachHeader(
    const nostd::string_view &name,
    nostd::function_ref<bool(nostd::string_view name, nostd::string_view value)> callable)
    const noexcept
{
  auto range = headers_.equal_range(static_cast<std::string>(name));
  for (auto it = range.first; it != range.second; ++it)
  {
    if (!callable(it->first, it->second))
    {
      return false;
    }
  }
  return true;
}

bool Session::CancelSession() noexcept
{
  http_client_.CleanupSession(session_id_);
  return true;
}

bool Session::FinishSession() noexcept
{
  http_client_.CleanupSession(session_id_);
  return true;
}

HttpClient::HttpClient()
{
  session_ = std::shared_ptr<Session>{new Session(*this)};
}

std::shared_ptr<opentelemetry::ext::http::client::Session> HttpClient::CreateSession(
    nostd::string_view) noexcept
{
  return session_;
}

bool HttpClient::CancelAllSessions() noexcept
{
  session_->CancelSession();
  return true;
}

bool HttpClient::FinishAllSessions() noexcept
{
  session_->FinishSession();
  return true;
}

void HttpClient::SetMaxSessionsPerConnection(std::size_t /* max_requests_per_connection */) noexcept
{}

void HttpClient::CleanupSession(uint64_t /* session_id */) {}

}  // namespace nosend
}  // namespace client
}  // namespace http
}  // namespace ext
OPENTELEMETRY_END_NAMESPACE
