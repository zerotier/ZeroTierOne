// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

// NOLINTNEXTLINE
#define _WINSOCKAPI_  // stops including winsock.h

#include <atomic>
#include <chrono>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "nlohmann/json.hpp"

#include "opentelemetry/exporters/zipkin/recordable.h"
#include "opentelemetry/exporters/zipkin/zipkin_exporter.h"
#include "opentelemetry/exporters/zipkin/zipkin_exporter_options.h"
#include "opentelemetry/ext/http/client/http_client.h"
#include "opentelemetry/ext/http/client/http_client_factory.h"
#include "opentelemetry/ext/http/common/url_parser.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/version.h"

namespace http_client = opentelemetry::ext::http::client;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace zipkin
{

// -------------------------------- Constructors --------------------------------

ZipkinExporter::ZipkinExporter(const ZipkinExporterOptions &options)
    : options_(options),
      http_client_(ext::http::client::HttpClientFactory::CreateSync()),
      url_parser_(options_.endpoint)
{
  InitializeLocalEndpoint();
}

ZipkinExporter::ZipkinExporter()
    : options_(ZipkinExporterOptions()),
      http_client_(ext::http::client::HttpClientFactory::CreateSync()),
      url_parser_(options_.endpoint)
{
  InitializeLocalEndpoint();
}

ZipkinExporter::ZipkinExporter(
    std::shared_ptr<opentelemetry::ext::http::client::HttpClientSync> http_client)
    : options_(ZipkinExporterOptions()),
      http_client_(std::move(http_client)),
      url_parser_(options_.endpoint)
{

  InitializeLocalEndpoint();
}

// ----------------------------- Exporter methods ------------------------------

std::unique_ptr<sdk::trace::Recordable> ZipkinExporter::MakeRecordable() noexcept
{
  return std::unique_ptr<sdk::trace::Recordable>(new Recordable);
}

sdk::common::ExportResult ZipkinExporter::Export(
    const nostd::span<std::unique_ptr<sdk::trace::Recordable>> &spans) noexcept
{
  if (isShutdown())
  {
    OTEL_INTERNAL_LOG_ERROR("[Zipkin Trace Exporter] Exporting "
                            << spans.size() << " span(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }
  exporter::zipkin::ZipkinSpan json_spans = {};
  for (auto &recordable : spans)
  {
    auto rec = std::unique_ptr<Recordable>(static_cast<Recordable *>(recordable.release()));
    if (rec != nullptr)
    {
      auto json_span = rec->span();
      // add localEndPoint
      json_span["localEndpoint"] = local_end_point_;
      // check service.name
      auto service_name = rec->GetServiceName();
      if (service_name.size())
      {
        json_span["localEndpoint"]["serviceName"] = service_name;
      }
      json_spans.push_back(json_span);
    }
  }
  auto body_s = json_spans.dump();
  http_client::Body body_v(body_s.begin(), body_s.end());
  auto result = http_client_->PostNoSsl(url_parser_.url_, body_v, options_.headers);
  if (result &&
      (result.GetResponse().GetStatusCode() >= 200 && result.GetResponse().GetStatusCode() <= 299))
  {
    return sdk::common::ExportResult::kSuccess;
  }
  else
  {
    if (result.GetSessionState() == http_client::SessionState::ConnectFailed)
    {
      OTEL_INTERNAL_LOG_ERROR("ZIPKIN EXPORTER] Zipkin Exporter: Connection failed");
    }
    return sdk::common::ExportResult::kFailure;
  }
}

void ZipkinExporter::InitializeLocalEndpoint()
{
  if (options_.service_name.length())
  {
    local_end_point_["serviceName"] = options_.service_name;
  }
  if (options_.ipv4.length())
  {
    local_end_point_["ipv4"] = options_.ipv4;
  }
  if (options_.ipv6.length())
  {
    local_end_point_["ipv6"] = options_.ipv6;
  }
  local_end_point_["port"] = url_parser_.port_;
}

bool ZipkinExporter::ForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

bool ZipkinExporter::Shutdown(std::chrono::microseconds /* timeout */) noexcept
{
  is_shutdown_ = true;
  return true;
}

bool ZipkinExporter::isShutdown() const noexcept
{
  return is_shutdown_;
}

}  // namespace zipkin
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
