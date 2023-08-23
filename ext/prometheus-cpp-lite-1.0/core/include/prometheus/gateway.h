#pragma once

#include "prometheus/collectable.h"
#include "prometheus/text_serializer.h"
#include "prometheus/metric_family.h"

#include <jdl/httpclientlite.h>

#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <future>
#include <algorithm>
#include <utility>


namespace prometheus {

  class  Gateway {
    using CollectableEntry = std::pair<std::weak_ptr<Collectable>, std::string>;

    std::string job_uri_;
    std::string labels_;

    std::mutex mutex_;

    std::vector<CollectableEntry> collectables_;

    enum class HttpMethod : uint8_t{
      Post,
      Put,
      Delete,
    };

  public:
    using Labels = std::map<std::string, std::string>;

    Gateway(const std::string host, const std::string port,
            const std::string jobname, const Labels& labels = {})
      : job_uri_(host + ':' + port + std::string("/metrics/job/") + jobname)
      , labels_{}
    {
      std::stringstream label_strm;
      for (const auto& label : labels) {
        label_strm << "/" << label.first << "/" << label.second;
      }
      labels_ = label_strm.str();
    }

    void RegisterCollectable(const std::weak_ptr<Collectable>& collectable,
                             const Labels* labels = nullptr) {
      std::stringstream label_strm;

      if (labels != nullptr) {
        for (const auto& label : *labels) {
          label_strm << "/" << label.first << "/" << label.second;
        }
      }

      CleanupStalePointers(collectables_);
      collectables_.emplace_back(std::make_pair(collectable, label_strm.str()));
    }


    static const Labels GetInstanceLabel(const std::string& hostname) {
      if (hostname.empty()) {
        return Gateway::Labels{};
      }

      return Gateway::Labels{{"instance", hostname}};
    }


    // Push metrics to the given pushgateway.
    int Push() {
      return push(HttpMethod::Post);
    }


    std::future<int> AsyncPush() {
      return async_push(HttpMethod::Post);
    }


    // PushAdd metrics to the given pushgateway.
    int PushAdd() {
      return push(HttpMethod::Put);
    }


    std::future<int> AsyncPushAdd() {
      return async_push(HttpMethod::Put);
    }


    // Delete metrics from the given pushgateway.
    int Delete() {
      return performHttpRequest(HttpMethod::Delete, job_uri_, {});
    }

    // Delete metrics from the given pushgateway.
    std::future<int> AsyncDelete() {
      return std::async(std::launch::async, [&] { return Delete(); });
    }


  private:
    std::string getUri(const CollectableEntry& collectable) const {
      return (job_uri_ + labels_ + collectable.second);
    }


    int performHttpRequest(HttpMethod /*method*/, const std::string& uri_str, const std::string& body) {
      std::lock_guard<std::mutex> l(mutex_);

      /* Stub function. The implementation will be later, after connecting the
       * additional library of HTTP requests. */

      jdl::URI uri(uri_str);
      jdl::HTTPResponse response = jdl::HTTPClient::request(jdl::HTTPClient::m_post, uri, body);

      return std::stoi(response.response);
    }


    int push(HttpMethod method) {
      const auto serializer = TextSerializer{};

      for (const auto& wcollectable : collectables_) {
        auto collectable = wcollectable.first.lock();
        if (!collectable) {
          continue;
        }

        auto metrics = collectable->Collect();
        auto uri = getUri(wcollectable);

        std::stringstream body;
        serializer.Serialize(body, metrics);
        std::string body_str = body.str();

        auto status_code = performHttpRequest(method, uri, body_str);

        if (status_code < 100 || status_code >= 400) {
          return status_code;
        }
      }

      return 200;
    }


    std::future<int> async_push(HttpMethod method) {
      const auto serializer = TextSerializer{};
      std::vector<std::future<int>> futures;

      for (const auto& wcollectable : collectables_) {
        auto collectable = wcollectable.first.lock();
        if (!collectable) {
          continue;
        }

        auto metrics = collectable->Collect();
        auto uri = getUri(wcollectable);

        std::stringstream body;
        serializer.Serialize(body, metrics);
        auto body_ptr = std::make_shared<std::string>(body.str());

        futures.emplace_back(std::async(std::launch::async, [method, &uri, &body_ptr, this] {
          return performHttpRequest(method, uri, *body_ptr);
        }));
      }

      const auto reduceFutures = [](std::vector<std::future<int>> lfutures) {
        auto final_status_code = 200;

        for (auto& future : lfutures) {
          auto status_code = future.get();

          if (status_code < 100 || status_code >= 400) {
            final_status_code = status_code;
          }
        }

        return final_status_code;
      };

      return std::async(std::launch::async, reduceFutures, std::move(futures));
    }


    static void CleanupStalePointers(std::vector<CollectableEntry>& collectables) {
      collectables.erase(std::remove_if(std::begin(collectables), std::end(collectables),
                                        [](const CollectableEntry& candidate) {
                                          return candidate.first.expired();
                                        }),
                         std::end(collectables));
    }
  };

}  // namespace prometheus
