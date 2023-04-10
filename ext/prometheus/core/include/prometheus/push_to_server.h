#pragma once

#include <thread>
#include <chrono>
#include <string>

#include "registry.h"
#include "text_serializer.h"

#include <jdl/httpclientlite.h>


namespace prometheus {
  class PushToServer {
    std::chrono::seconds      period { 1 };
    std::string               uri { "" };
    std::thread               worker_thread { &PushToServer::worker_function, this };
    std::shared_ptr<Registry> registry_ptr  { nullptr };
    bool                      must_die      { false };

    void push_data() {
      if (registry_ptr) {
        if (!uri.empty()) {
          std::stringstream body_strm;
          TextSerializer::Serialize(body_strm, registry_ptr->Collect());

          std::string body = body_strm.str();
          jdl::HTTPResponse response = jdl::HTTPClient::request(jdl::HTTPClient::m_post, jdl::URI(uri), body);
        }
      }
    }

    void worker_function() {
      // it need for fast shutdown this thread when SaveToFile destructor is called
      const uint64_t divider = 100;
      uint64_t fraction = divider;

      for (;;) {
        std::chrono::milliseconds period_ms
          = std::chrono::duration_cast<std::chrono::milliseconds>(period);
        std::this_thread::sleep_for( period_ms / divider );

        if (must_die) {
          push_data();
          return;
        }

        if (--fraction == 0) {
          fraction = divider;
          push_data();
        }
      }
    }

  public:
    PushToServer() {
      jdl::init_socket();
    }

    ~PushToServer() {
      must_die = true;
      worker_thread.join();
      jdl::deinit_socket();
    }

    PushToServer(std::shared_ptr<Registry>& registry_, const std::chrono::seconds& period_, const std::string& uri_) {
      set_registry(registry_);
      set_delay(period_);
      set_uri(uri_);
    }

    void set_delay (const std::chrono::seconds& new_period) {
      period = new_period;
    }


    void set_uri (const std::string& uri_) {
      uri = std::move(uri_);
    }

    void set_registry (std::shared_ptr<Registry>& new_registry_ptr) {
      registry_ptr = new_registry_ptr;
    }

  };
}
