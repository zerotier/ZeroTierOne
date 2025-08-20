// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <atomic>
#include <string>
#include "opentelemetry/ext/http/server/http_server.h"

namespace
{

class HttpServer : public HTTP_SERVER_NS::HttpRequestCallback
{

protected:
  HTTP_SERVER_NS::HttpServer server_;
  std::string server_url_;
  uint16_t port_;
  std::atomic<bool> is_running_{false};

public:
  HttpServer(const std::string &server_name = "test_server", uint16_t port = 8800) : port_(port)
  {
    server_.setServerName(server_name);
    server_.setKeepalive(false);
  }

  void AddHandler(const std::string &path, HTTP_SERVER_NS::HttpRequestCallback *request_handler)
  {
    server_.addHandler(path, *request_handler);
  }

  void Start()
  {
    if (!is_running_.exchange(true))
    {
      server_.addListeningPort(port_);
      server_.start();
    }
  }

  void Stop()
  {
    if (is_running_.exchange(false))
    {
      server_.stop();
    }
  }

  ~HttpServer() { Stop(); }
};

}  // namespace
