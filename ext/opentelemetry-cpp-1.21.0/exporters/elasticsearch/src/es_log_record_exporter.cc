// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <sstream>  // std::stringstream

#include <condition_variable>
#include <mutex>
#include "opentelemetry/exporters/elasticsearch/es_log_record_exporter.h"
#include "opentelemetry/exporters/elasticsearch/es_log_recordable.h"
#include "opentelemetry/sdk_config.h"

namespace nostd       = opentelemetry::nostd;
namespace sdklogs     = opentelemetry::sdk::logs;
namespace http_client = opentelemetry::ext::http::client;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace logs
{
/**
 * This class handles the response message from the Elasticsearch request
 */
class ResponseHandler : public http_client::EventHandler
{
public:
  /**
   * Creates a response handler, that by default doesn't display to console
   */
  ResponseHandler(bool console_debug = false) : console_debug_{console_debug} {}

  std::string BuildResponseLogMessage(http_client::Response &response,
                                      const std::string &body) noexcept
  {
    std::stringstream ss;
    ss << "Status:" << response.GetStatusCode() << ", Header:";
    response.ForEachHeader([&ss](opentelemetry::nostd::string_view header_name,
                                 opentelemetry::nostd::string_view header_value) {
      ss << "\t" << header_name.data() << ": " << header_value.data() << ",";
      return true;
    });
    ss << "Body:" << body;

    return ss.str();
  }

  /**
   * Automatically called when the response is received, store the body into a string and notify any
   * threads blocked on this result
   */
  void OnResponse(http_client::Response &response) noexcept override
  {
    std::string log_message;

    // Lock the private members so they can't be read while being modified
    {
      std::unique_lock<std::mutex> lk(mutex_);

      // Store the body of the request
      body_ = std::string(response.GetBody().begin(), response.GetBody().end());

      if (!(response.GetStatusCode() >= 200 && response.GetStatusCode() <= 299))
      {
        log_message = BuildResponseLogMessage(response, body_);

        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Export failed, " << log_message);
      }

      if (console_debug_)
      {
        if (log_message.empty())
        {
          log_message = BuildResponseLogMessage(response, body_);
        }

        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Got response from Elasticsearch, "
                                << log_message);
      }

      // Set the response_received_ flag to true and notify any threads waiting on this result
      response_received_ = true;
    }
    cv_.notify_all();
  }

  /**
   * A method the user calls to block their thread until the response is received. The longest
   * duration is the timeout of the request, set by SetTimeoutMs()
   */
  bool waitForResponse()
  {
    std::unique_lock<std::mutex> lk(mutex_);
    cv_.wait(lk);
    return response_received_;
  }

  /**
   * Returns the body of the response
   */
  std::string GetResponseBody()
  {
    // Lock so that body_ can't be written to while returning it
    std::unique_lock<std::mutex> lk(mutex_);
    return body_;
  }

  // Callback method when an http event occurs
  void OnEvent(http_client::SessionState state, nostd::string_view /* reason */) noexcept override
  {
    // If any failure event occurs, release the condition variable to unblock main thread
    switch (state)
    {
      case http_client::SessionState::CreateFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Failed to create session");
        cv_.notify_all();
        break;
      case http_client::SessionState::Created:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Session created");
        break;
      case http_client::SessionState::Destroyed:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Session destroyed");
        break;
      case http_client::SessionState::Connecting:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Connecting to peer");
        break;
      case http_client::SessionState::ConnectFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Failed to connect to peer");
        cv_.notify_all();
        break;
      case http_client::SessionState::Connected:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Connected to peer");
        break;
      case http_client::SessionState::Sending:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Sending request");
        break;
      case http_client::SessionState::SendFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Failed to send request");
        cv_.notify_all();
        break;
      case http_client::SessionState::Response:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Received response");
        break;
      case http_client::SessionState::SSLHandshakeFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Failed SSL Handshake");
        cv_.notify_all();
        break;
      case http_client::SessionState::TimedOut:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Request timed out");
        cv_.notify_all();
        break;
      case http_client::SessionState::NetworkError:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Network error");
        cv_.notify_all();
        break;
      case http_client::SessionState::ReadError:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Read error");
        break;
      case http_client::SessionState::WriteError:
        OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Write error");
        break;
      case http_client::SessionState::Cancelled:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] (manually) cancelled");
        cv_.notify_all();
        break;
    }
  }

private:
  // Define a condition variable and mutex
  std::condition_variable cv_;
  std::mutex mutex_;

  // Whether the response from Elasticsearch has been received
  bool response_received_ = false;

  // A string to store the response body
  std::string body_ = "";

  // Whether to print the results from the callback
  bool console_debug_ = false;
};

#ifdef ENABLE_ASYNC_EXPORT
/**
 * This class handles the async response message from the Elasticsearch request
 */
class AsyncResponseHandler : public http_client::EventHandler
{
public:
  /**
   * Creates a response handler, that by default doesn't display to console
   */
  AsyncResponseHandler(
      std::shared_ptr<ext::http::client::Session> session,
      std::function<bool(opentelemetry::sdk::common::ExportResult)> &&result_callback,
      bool console_debug = false)
      : session_{std::move(session)},
        result_callback_{std::move(result_callback)},
        console_debug_{console_debug}
  {}

  /**
   * Cleans up the session in the destructor.
   */
  ~AsyncResponseHandler() override { session_->FinishSession(); }

  /**
   * Automatically called when the response is received
   */
  void OnResponse(http_client::Response &response) noexcept override
  {

    // Store the body of the response
    body_ = std::string(response.GetBody().begin(), response.GetBody().end());
    if (console_debug_)
    {
      OTEL_INTERNAL_LOG_DEBUG(
          "[ES Log Exporter] Got response from Elasticsearch,  response body: " << body_);
    }
    if (body_.find("\"failed\" : 0") == std::string::npos)
    {
      OTEL_INTERNAL_LOG_ERROR(
          "[ES Log Exporter] Logs were not written to Elasticsearch correctly, response body: "
          << body_);
      result_callback_(sdk::common::ExportResult::kFailure);
    }
    else
    {
      result_callback_(sdk::common::ExportResult::kSuccess);
    }
  }

  // Callback method when an http event occurs
  void OnEvent(http_client::SessionState state, nostd::string_view /* reason */) noexcept override
  {
    bool need_stop = false;
    switch (state)
    {
      case http_client::SessionState::CreateFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Create request to elasticsearch failed");
        need_stop = true;
        break;
      case http_client::SessionState::ConnectFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Connection to elasticsearch failed");
        need_stop = true;
        break;
      case http_client::SessionState::SendFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Request failed to be sent to elasticsearch");
        need_stop = true;
        break;
      case http_client::SessionState::SSLHandshakeFailed:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] SSL handshake to elasticsearch failed");
        need_stop = true;
        break;
      case http_client::SessionState::TimedOut:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Request to elasticsearch timed out");
        need_stop = true;
        break;
      case http_client::SessionState::NetworkError:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Network error to elasticsearch");
        need_stop = true;
        break;
      case http_client::SessionState::Cancelled:
        OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Request to elasticsearch cancelled");
        need_stop = true;
        break;
      default:
        break;
    }
    if (need_stop)
    {
      result_callback_(sdk::common::ExportResult::kFailure);
    }
  }

private:
  // Stores the session object for the request
  std::shared_ptr<ext::http::client::Session> session_;
  // Callback to call to on receiving events
  std::function<bool(opentelemetry::sdk::common::ExportResult)> result_callback_;

  // A string to store the response body
  std::string body_ = "";

  // Whether to print the results from the callback
  bool console_debug_ = false;
};
#endif

ElasticsearchLogRecordExporter::ElasticsearchLogRecordExporter()
    : ElasticsearchLogRecordExporter(ElasticsearchExporterOptions())
{}

ElasticsearchLogRecordExporter::ElasticsearchLogRecordExporter(
    const ElasticsearchExporterOptions &options)
    : options_{options},
      http_client_{ext::http::client::HttpClientFactory::Create()}
#ifdef ENABLE_ASYNC_EXPORT
      ,
      synchronization_data_(new SynchronizationData())
#endif
{
#ifdef ENABLE_ASYNC_EXPORT
  synchronization_data_->finished_session_counter_.store(0);
  synchronization_data_->session_counter_.store(0);
#endif
}

std::unique_ptr<sdklogs::Recordable> ElasticsearchLogRecordExporter::MakeRecordable() noexcept
{
  return std::unique_ptr<sdklogs::Recordable>(new ElasticSearchRecordable());
}

sdk::common::ExportResult ElasticsearchLogRecordExporter::Export(
    const nostd::span<std::unique_ptr<sdklogs::Recordable>> &records) noexcept
{
  // Return failure if this exporter has been shutdown
  if (isShutdown())
  {
    OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] Exporting "
                            << records.size() << " log(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }

  // Create a connection to the ElasticSearch instance
  auto session = http_client_->CreateSession(options_.host_ + ":" + std::to_string(options_.port_));
  auto request = session->CreateRequest();

  // Populate the request with headers and methods
  request->SetUri(options_.index_ + "/_bulk?pretty");
  request->SetMethod(http_client::Method::Post);
  request->AddHeader("Content-Type", "application/json");

  // Add options headers
  for (auto it = options_.http_headers_.cbegin(); it != options_.http_headers_.cend(); ++it)
  {
    request->AddHeader(it->first, it->second);
  }

  request->SetTimeoutMs(std::chrono::milliseconds(1000 * options_.response_timeout_));

  // Create the request body
  std::string body = "";
  for (auto &record : records)
  {
    // Append {"index":{}} before JSON body, which tells Elasticsearch to write to index specified
    // in URI
    body += "{\"index\" : {}}\n";

    // Add the context of the Recordable
    auto json_record = std::unique_ptr<ElasticSearchRecordable>(
        static_cast<ElasticSearchRecordable *>(record.release()));
    body += json_record->GetJSON().dump() + "\n";
  }
  std::vector<uint8_t> body_vec(body.begin(), body.end());
  request->SetBody(body_vec);

#ifdef ENABLE_ASYNC_EXPORT
  // Send the request
  synchronization_data_->session_counter_.fetch_add(1, std::memory_order_release);
  std::size_t span_count    = records.size();
  auto synchronization_data = synchronization_data_;
  auto handler              = std::make_shared<AsyncResponseHandler>(
      session,
      [span_count, synchronization_data](opentelemetry::sdk::common::ExportResult result) {
        if (result != opentelemetry::sdk::common::ExportResult::kSuccess)
        {
          OTEL_INTERNAL_LOG_ERROR("[ES Log Exporter] ERROR: Export "
                                               << span_count
                                               << " trace span(s) error: " << static_cast<int>(result));
        }
        else
        {
          OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] Export " << span_count
                                                                           << " trace span(s) success");
        }

        synchronization_data->finished_session_counter_.fetch_add(1, std::memory_order_release);
        synchronization_data->force_flush_cv.notify_all();
        return true;
      },
      options_.console_debug_);
  session->SendRequest(handler);
  return sdk::common::ExportResult::kSuccess;
#else
  // Send the request
  auto handler = std::make_shared<ResponseHandler>(options_.console_debug_);
  session->SendRequest(handler);

  // Wait for the response to be received
  if (options_.console_debug_)
  {
    OTEL_INTERNAL_LOG_DEBUG("[ES Log Exporter] waiting for response from Elasticsearch (timeout = "
                            << options_.response_timeout_ << " seconds)");
  }
  bool write_successful = handler->waitForResponse();

  // End the session
  session->FinishSession();

  // If an error occurred with the HTTP request
  if (!write_successful)
  {
    // TODO: retry logic
    return sdk::common::ExportResult::kFailure;
  }

  // Parse the response output to determine if Elasticsearch consumed it correctly
  std::string responseBody = handler->GetResponseBody();
  if (responseBody.find("\"failed\" : 0") == std::string::npos)
  {
    OTEL_INTERNAL_LOG_ERROR(
        "[ES Log Exporter] Logs were not written to Elasticsearch correctly, response body: "
        << responseBody);
    // TODO: Retry logic
    return sdk::common::ExportResult::kFailure;
  }

  return sdk::common::ExportResult::kSuccess;
#endif
}

bool ElasticsearchLogRecordExporter::ForceFlush(std::chrono::microseconds timeout
                                                OPENTELEMETRY_MAYBE_UNUSED) noexcept
{
#ifdef ENABLE_ASYNC_EXPORT
  std::lock_guard<std::recursive_mutex> lock_guard{synchronization_data_->force_flush_m};
  std::size_t running_counter =
      synchronization_data_->session_counter_.load(std::memory_order_acquire);
  // ASAN will report chrono: runtime error: signed integer overflow: A + B cannot be represented
  //   in type 'long int' here. So we reset timeout to meet signed long int limit here.
  timeout = opentelemetry::common::DurationUtil::AdjustWaitForTimeout(
      timeout, std::chrono::microseconds::zero());

  std::chrono::steady_clock::duration timeout_steady =
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout);
  if (timeout_steady <= std::chrono::steady_clock::duration::zero())
  {
    timeout_steady = (std::chrono::steady_clock::duration::max)();
  }

  std::unique_lock<std::mutex> lk_cv(synchronization_data_->force_flush_cv_m);
  // Wait for all the sessions to finish
  while (timeout_steady > std::chrono::steady_clock::duration::zero())
  {
    if (synchronization_data_->finished_session_counter_.load(std::memory_order_acquire) >=
        running_counter)
    {
      break;
    }

    std::chrono::steady_clock::time_point start_timepoint = std::chrono::steady_clock::now();
    if (std::cv_status::no_timeout != synchronization_data_->force_flush_cv.wait_for(
                                          lk_cv, std::chrono::seconds{options_.response_timeout_}))
    {
      break;
    }
    timeout_steady -= std::chrono::steady_clock::now() - start_timepoint;
  }

  return timeout_steady > std::chrono::steady_clock::duration::zero();
#else
  return true;
#endif
}

bool ElasticsearchLogRecordExporter::Shutdown(std::chrono::microseconds /* timeout */) noexcept
{
  is_shutdown_ = true;

  // Shutdown the session manager
  http_client_->CancelAllSessions();
  http_client_->FinishAllSessions();

  return true;
}

bool ElasticsearchLogRecordExporter::isShutdown() const noexcept
{
  return is_shutdown_;
}
}  // namespace logs
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
