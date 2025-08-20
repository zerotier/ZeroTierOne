// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/ext/http/client/http_client_factory.h"
#include "opentelemetry/ext/http/common/url_parser.h"
#include "opentelemetry/semconv/http_attributes.h"
#include "opentelemetry/semconv/url_attributes.h"
#include "tracer_common.h"

namespace
{

using namespace opentelemetry::trace;
namespace http_client = opentelemetry::ext::http::client;
namespace context     = opentelemetry::context;
namespace nostd       = opentelemetry::nostd;
namespace semconv     = opentelemetry::semconv;

void sendRequest(const std::string &url)
{
  auto http_client = http_client::HttpClientFactory::CreateSync();

  // start active span
  StartSpanOptions options;
  options.kind = SpanKind::kClient;  // client
  opentelemetry::ext::http::common::UrlParser url_parser(url);

  std::string span_name = url_parser.path_;
  auto span             = get_tracer("http-client")
                  ->StartSpan(span_name,
                              {{semconv::url::kUrlFull, url_parser.url_},
                               {semconv::url::kUrlScheme, url_parser.scheme_},
                               {semconv::http::kHttpRequestMethod, "GET"}},
                              options);
  auto scope = get_tracer("http-client")->WithActiveSpan(span);

  // inject current context into http header
  auto current_ctx = context::RuntimeContext::GetCurrent();
  HttpTextMapCarrier<http_client::Headers> carrier;
  auto prop = context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
  prop->Inject(carrier, current_ctx);

  // send http request
  http_client::Result result = http_client->GetNoSsl(url, carrier.headers_);
  if (result)
  {
    // set span attributes
    auto status_code = result.GetResponse().GetStatusCode();
    span->SetAttribute(semconv::http::kHttpResponseStatusCode, status_code);
    result.GetResponse().ForEachHeader(
        [&span](nostd::string_view header_name, nostd::string_view header_value) {
          span->SetAttribute("http.header." + std::string(header_name.data()), header_value);
          return true;
        });

    if (status_code >= 400)
    {
      span->SetStatus(StatusCode::kError);
    }
  }
  else
  {
    span->SetStatus(
        StatusCode::kError,
        "Response Status :" +
            std::to_string(
                static_cast<typename std::underlying_type<http_client::SessionState>::type>(
                    result.GetSessionState())));
  }
  // end span and export data
  span->End();
}

}  // namespace

int main(int argc, char *argv[])
{
  InitTracer();
  constexpr char default_host[]   = "localhost";
  constexpr char default_path[]   = "/helloworld";
  constexpr uint16_t default_port = 8800;
  uint16_t port;

  // The port the validation service listens to can be specified via the command line.
  if (argc > 1)
  {
    port = static_cast<uint16_t>(atoi(argv[1]));
  }
  else
  {
    port = default_port;
  }

  std::string url = "http://" + std::string(default_host) + ":" + std::to_string(port) +
                    std::string(default_path);
  sendRequest(url);
  CleanupTracer();
  return 0;
}
