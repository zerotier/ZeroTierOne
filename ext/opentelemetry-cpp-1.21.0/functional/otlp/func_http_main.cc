// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/provider.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/trace/tracer_provider.h"

namespace trace     = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp      = opentelemetry::exporter::otlp;
namespace nostd     = opentelemetry::nostd;

namespace internal_log = opentelemetry::sdk::common::internal_log;

const int TEST_PASSED = 0;
const int TEST_FAILED = 1;

/*
  Command line parameters.
*/

enum test_mode : std::uint8_t
{
  MODE_NONE,
  MODE_HTTP,
  MODE_HTTPS
};

bool opt_help   = false;
bool opt_list   = false;
bool opt_debug  = false;
bool opt_secure = false;
// HTTPS by default
std::string opt_endpoint = "https://localhost:4318/v1/traces";
std::string opt_cert_dir;
std::string opt_test_name;
test_mode opt_mode = MODE_NONE;

/*
  Log parsing
*/

struct TestResult
{
  bool found_connection_failed    = false;
  bool found_connection_refused   = false;
  bool found_request_send_failure = false;
  bool found_export_error         = false;
  bool found_export_success       = false;
  bool found_unknown_min_tls      = false;
  bool found_unknown_max_tls      = false;

  void reset()
  {
    found_connection_failed    = false;
    found_connection_refused   = false;
    found_request_send_failure = false;
    found_export_error         = false;
    found_export_success       = false;
    found_unknown_min_tls      = false;
    found_unknown_max_tls      = false;
  }
};

struct TestResult g_test_result;

void parse_error_msg(TestResult *result, const std::string &msg)
{
  static std::string connection_failed("Session state: connection failed.");

  if (msg.find(connection_failed) != std::string::npos)
  {
    result->found_connection_failed = true;
  }

  static std::string connection_refused("Connection refused");

  if (msg.find(connection_refused) != std::string::npos)
  {
    result->found_connection_refused = true;
  }

  static std::string request_send_failed("Session state: request send failed.");

  if (msg.find(request_send_failed) != std::string::npos)
  {
    result->found_request_send_failure = true;
  }

  static std::string export_failed("ERROR: Export 1 trace span(s) error: 1");

  if (msg.find(export_failed) != std::string::npos)
  {
    result->found_export_error = true;
  }

  static std::string unknown_min_tls("Unknown min TLS version");

  if (msg.find(unknown_min_tls) != std::string::npos)
  {
    result->found_unknown_min_tls = true;
  }

  static std::string unknown_max_tls("Unknown max TLS version");

  if (msg.find(unknown_max_tls) != std::string::npos)
  {
    result->found_unknown_max_tls = true;
  }
}

void parse_warning_msg(TestResult * /* result */, const std::string & /* msg */) {}

void parse_info_msg(TestResult * /* result */, const std::string & /* msg */) {}

void parse_debug_msg(TestResult *result, const std::string &msg)
{
  static std::string export_success("Export 1 trace span(s) success");

  if (msg.find(export_success) != std::string::npos)
  {
    result->found_export_success = true;
  }
}

class TestLogHandler : public opentelemetry::sdk::common::internal_log::LogHandler
{
public:
  void Handle(opentelemetry::sdk::common::internal_log::LogLevel level,
              const char * /* file */,
              int /* line */,
              const char *msg,
              const opentelemetry::sdk::common::AttributeMap & /* attributes */) noexcept override
  {
    if (msg == nullptr)
    {
      msg = "<no msg>";
    }

    switch (level)
    {
      case opentelemetry::sdk::common::internal_log::LogLevel::None:
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Error:
        std::cout << " - [E] " << msg << '\n';
        parse_error_msg(&g_test_result, msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Warning:
        std::cout << " - [W] " << msg << '\n';
        parse_warning_msg(&g_test_result, msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Info:
        std::cout << " - [I] " << msg << '\n';
        parse_info_msg(&g_test_result, msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Debug:
        std::cout << " - [D] " << msg << '\n';
        parse_debug_msg(&g_test_result, msg);
        break;
    }
  }
};

void init(const otlp::OtlpHttpExporterOptions &opts)
{
  // Create OTLP exporter instance
  auto exporter  = otlp::OtlpHttpExporterFactory::Create(opts);
  auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor));
  // Set the global trace provider
  trace_sdk::Provider::SetTracerProvider(provider);
}

void payload()
{
  static const nostd::string_view k_tracer_name("func_test");
  static const nostd::string_view k_span_name("func_http_main");
  static const nostd::string_view k_attr_test_name("test_name");

  auto provider = trace::Provider::GetTracerProvider();
  auto tracer   = provider->GetTracer(k_tracer_name, "1.0");

  auto span = tracer->StartSpan(k_span_name);
  span->SetAttribute(k_attr_test_name, opt_test_name);
  span->End();
}

void cleanup()
{
  std::shared_ptr<opentelemetry::trace::TracerProvider> none;
  trace_sdk::Provider::SetTracerProvider(none);
}

void instrumented_payload(const otlp::OtlpHttpExporterOptions &opts)
{
  g_test_result.reset();
  init(opts);
  payload();
  cleanup();
}

void usage(FILE *out)
{
  static const char *msg =
      "Usage: func_otlp_http [options] test_name\n"
      "Valid options are:\n"
      "  --help            Print this help\n"
      "  --list            List test names\n"
      "  --endpoint url    OTLP HTTP endpoint (https://localhost:4318/v1/traces by default)\n"
      "  --cert-dir dir    Directory that contains test ssl certificates\n"
      "  --mode mode       Test server mode (used to verify expected results)\n"
      "                      - none: no endpoint\n"
      "                      - http: http endpoint\n"
      "                      - https: https endpoint\n";
  fprintf(out, "%s", msg);
}

int parse_args(int argc, char *argv[])
{
  int remaining_argc    = argc;
  char **remaining_argv = argv;

  while (remaining_argc > 0)
  {
    if (strcmp(*remaining_argv, "--help") == 0)
    {
      opt_help = true;
      return 0;
    }

    if (strcmp(*remaining_argv, "--list") == 0)
    {
      opt_list = true;
      return 0;
    }

    if (strcmp(*remaining_argv, "--debug") == 0)
    {
      opt_debug = true;
      remaining_argc--;
      remaining_argv++;
      continue;
    }

    if (remaining_argc >= 2)
    {
      if (strcmp(*remaining_argv, "--cert-dir") == 0)
      {
        remaining_argc--;
        remaining_argv++;
        opt_cert_dir = *remaining_argv;
        remaining_argc--;
        remaining_argv++;
        continue;
      }

      if (strcmp(*remaining_argv, "--endpoint") == 0)
      {
        remaining_argc--;
        remaining_argv++;
        opt_endpoint = *remaining_argv;
        remaining_argc--;
        remaining_argv++;
        continue;
      }

      if (strcmp(*remaining_argv, "--mode") == 0)
      {
        remaining_argc--;
        remaining_argv++;
        std::string mode = *remaining_argv;
        remaining_argc--;
        remaining_argv++;

        if (mode == "none")
        {
          opt_mode = MODE_NONE;
        }
        else if (mode == "http")
        {
          opt_mode = MODE_HTTP;
        }
        else if (mode == "https")
        {
          opt_mode = MODE_HTTPS;
        }
        else
        {
          // Unknown mode
          return 2;
        }

        continue;
      }
    }

    opt_test_name = *remaining_argv;
    remaining_argc--;
    remaining_argv++;

    if (remaining_argc)
    {
      // Unknown option
      return 1;
    }
  }

  return 0;
}

typedef int (*test_func_t)();

struct test_case
{
  std::string m_name;
  test_func_t m_func;
};

int test_basic();

int test_cert_not_found();
int test_cert_invalid();
int test_cert_unreadable();
int test_cert_ok();
int test_client_cert_not_found();
int test_client_cert_invalid();
int test_client_cert_unreadable();
int test_client_cert_no_key();
int test_client_key_not_found();
int test_client_key_invalid();
int test_client_key_unreadable();
int test_client_key_ok();

int test_min_tls_unknown();
int test_min_tls_10();
int test_min_tls_11();
int test_min_tls_12();
int test_min_tls_13();
int test_max_tls_unknown();
int test_max_tls_10();
int test_max_tls_11();
int test_max_tls_12();
int test_max_tls_13();
int test_range_tls_10();
int test_range_tls_11();
int test_range_tls_12();
int test_range_tls_13();
int test_range_tls_10_11();
int test_range_tls_10_12();
int test_range_tls_10_13();
int test_range_tls_11_10();
int test_range_tls_11_12();
int test_range_tls_11_13();
int test_range_tls_12_10();
int test_range_tls_12_11();
int test_range_tls_12_13();
int test_range_tls_13_10();
int test_range_tls_13_11();
int test_range_tls_13_12();

int test_gzip_compression();

static const test_case all_tests[] = {{"basic", test_basic},
                                      {"cert-not-found", test_cert_not_found},
                                      {"cert-invalid", test_cert_invalid},
                                      {"cert-unreadable", test_cert_unreadable},
                                      {"cert-ok", test_cert_ok},
                                      {"client-cert-not-found", test_client_cert_not_found},
                                      {"client-cert-invalid", test_client_cert_invalid},
                                      {"client-cert-unreadable", test_client_cert_unreadable},
                                      {"client-cert-no-key", test_client_cert_no_key},
                                      {"client-key-not-found", test_client_key_not_found},
                                      {"client-key-invalid", test_client_key_invalid},
                                      {"client-key-unreadable", test_client_key_unreadable},
                                      {"client-key-ok", test_client_key_ok},
                                      {"min-tls-unknown", test_min_tls_unknown},
                                      {"min-tls-10", test_min_tls_10},
                                      {"min-tls-11", test_min_tls_11},
                                      {"min-tls-12", test_min_tls_12},
                                      {"min-tls-13", test_min_tls_13},
                                      {"max-tls-unknown", test_max_tls_unknown},
                                      {"max-tls-10", test_max_tls_10},
                                      {"max-tls-11", test_max_tls_11},
                                      {"max-tls-12", test_max_tls_12},
                                      {"max-tls-13", test_max_tls_13},
                                      {"range-tls-10", test_range_tls_10},
                                      {"range-tls-11", test_range_tls_11},
                                      {"range-tls-12", test_range_tls_12},
                                      {"range-tls-13", test_range_tls_13},
                                      {"range-tls-10-11", test_range_tls_10_11},
                                      {"range-tls-10-12", test_range_tls_10_12},
                                      {"range-tls-10-13", test_range_tls_10_13},
                                      {"range-tls-11-10", test_range_tls_11_10},
                                      {"range-tls-11-12", test_range_tls_11_12},
                                      {"range-tls-11-13", test_range_tls_11_13},
                                      {"range-tls-12-10", test_range_tls_12_10},
                                      {"range-tls-12-11", test_range_tls_12_11},
                                      {"range-tls-12-13", test_range_tls_12_13},
                                      {"range-tls-13-10", test_range_tls_13_10},
                                      {"range-tls-13-11", test_range_tls_13_11},
                                      {"range-tls-13-12", test_range_tls_13_12},
                                      {"gzip-compression", test_gzip_compression},
                                      {"", nullptr}};

void list_test_cases()
{
  const test_case *current = all_tests;

  while (current->m_func != nullptr)
  {
    std::cout << current->m_name << '\n';
    current++;
  }
}

int run_test_case(const std::string &name)
{
  const test_case *current = all_tests;

  while (current->m_func != nullptr)
  {
    if (current->m_name == name)
    {
      int rc = current->m_func();
      return rc;
    }
    current++;
  }

  std::cerr << "Unknown test <" << name << ">" << '\n';
  return 1;
}

int main(int argc, char *argv[])
{
  // Program name
  argc--;
  argv++;

  int rc = parse_args(argc, argv);

  if (rc != 0)
  {
    usage(stderr);
    return 1;
  }

  if (opt_help)
  {
    usage(stdout);
    return 0;
  }

  if (opt_list)
  {
    list_test_cases();
    return 0;
  }

  if (opt_endpoint.find("https:") != std::string::npos)
  {
    opt_secure = true;
  }
  else
  {
    opt_secure = false;
  }

  opentelemetry::nostd::shared_ptr<internal_log::LogHandler> log_handler(new TestLogHandler);

  internal_log::GlobalLogHandler::SetLogHandler(log_handler);
  internal_log::GlobalLogHandler::SetLogLevel(internal_log::LogLevel::Debug);

  rc = run_test_case(opt_test_name);
  return rc;
}

void set_common_opts(otlp::OtlpHttpExporterOptions &opts)
{
  opts.url = opt_endpoint;

  if (opt_debug)
  {
    opts.console_debug = true;
  }

#ifdef ENABLE_ASYNC_EXPORT
  // Work around, ASYNC export not working.
  opts.max_concurrent_requests = 0;
#endif
}

int expect_connection_failed()
{
  if (g_test_result.found_export_error && g_test_result.found_connection_failed)
  {
    return TEST_PASSED;
  }
  return TEST_FAILED;
}

int expect_success()
{
  if (g_test_result.found_export_success)
  {
    return TEST_PASSED;
  }
  return TEST_FAILED;
}

int expect_request_send_failed()
{
  if (g_test_result.found_export_error && g_test_result.found_request_send_failure)
  {
    return TEST_PASSED;
  }
  return TEST_FAILED;
}

int expect_unknown_min_tls()
{
  if (g_test_result.found_export_error && g_test_result.found_unknown_min_tls)
  {
    return TEST_PASSED;
  }
  return TEST_FAILED;
}

int expect_unknown_max_tls()
{
  if (g_test_result.found_export_error && g_test_result.found_unknown_max_tls)
  {
    return TEST_PASSED;
  }
  return TEST_FAILED;
}

int expect_export_failed()
{
  /*
    Can not test exact root cause:
    - connect failed ?
    - send request failed ?
    - exact error message ?
    so only verifying export failed.
  */

  if (g_test_result.found_export_error)
  {
    return TEST_PASSED;
  }
  return TEST_FAILED;
}

int test_basic()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_connection_failed();
}

int test_cert_not_found()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path = opt_cert_dir + "/no-such-file.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_cert_invalid()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path = opt_cert_dir + "/garbage.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_cert_unreadable()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path = opt_cert_dir + "/unreadable.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_cert_ok()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path = opt_cert_dir + "/ca.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_client_cert_not_found()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/no-such-file.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_cert_invalid()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/garbage.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_cert_unreadable()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/unreadable.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_cert_no_key()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_key_not_found()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/no-such-file.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_key_invalid()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/garbage.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_key_unreadable()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/unreadable.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_key_ok()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_min_tls_unknown()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "99.99";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_unknown_min_tls();
}

int test_min_tls_10()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.0";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_unknown_min_tls();
}

int test_min_tls_11()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.1";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_unknown_min_tls();
}

int test_min_tls_12()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.2";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_min_tls_13()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.3";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_max_tls_unknown()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_max_tls          = "99.99";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  return expect_unknown_max_tls();
}

int test_max_tls_10()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_max_tls          = "1.0";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_export_failed();
  }

  // No support for TLS 1.0
  return expect_unknown_max_tls();
}

int test_max_tls_11()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_max_tls          = "1.1";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_export_failed();
  }

  // No support for TLS 1.1
  return expect_unknown_max_tls();
}

int test_max_tls_12()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_max_tls          = "1.2";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_max_tls_13()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_max_tls          = "1.3";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_range_tls_10()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.0";
  opts.ssl_max_tls          = "1.0";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  // No support for TLS 1.0
  return expect_unknown_min_tls();
}

int test_range_tls_11()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.1";
  opts.ssl_max_tls          = "1.1";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  // No support for TLS 1.0
  return expect_unknown_min_tls();
}

int test_range_tls_12()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.2";
  opts.ssl_max_tls          = "1.2";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_range_tls_13()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.3";
  opts.ssl_max_tls          = "1.3";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_range_tls_10_11()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.0";
  opts.ssl_max_tls          = "1.1";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  // No support for TLS 1.0, TLS 1.1
  return expect_unknown_min_tls();
}

int test_range_tls_10_12()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.0";
  opts.ssl_max_tls          = "1.2";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_unknown_min_tls();
}

int test_range_tls_10_13()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.0";
  opts.ssl_max_tls          = "1.3";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_unknown_min_tls();
}

int test_range_tls_11_10()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.1";
  opts.ssl_max_tls          = "1.0";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  // Impossible
  return expect_connection_failed();
}

int test_range_tls_11_12()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.1";
  opts.ssl_max_tls          = "1.2";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_unknown_min_tls();
}

int test_range_tls_11_13()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.1";
  opts.ssl_max_tls          = "1.3";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_unknown_min_tls();
}

int test_range_tls_12_10()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.2";
  opts.ssl_max_tls          = "1.0";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  // Impossible
  return expect_connection_failed();
}

int test_range_tls_12_11()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.2";
  opts.ssl_max_tls          = "1.1";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  // Impossible
  return expect_connection_failed();
}

int test_range_tls_12_13()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.2";
  opts.ssl_max_tls          = "1.3";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  return expect_success();
}

int test_range_tls_13_10()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.3";
  opts.ssl_max_tls          = "1.0";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  // Impossible
  return expect_connection_failed();
}

int test_range_tls_13_11()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.3";
  opts.ssl_max_tls          = "1.1";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  // Impossible
  return expect_connection_failed();
}

int test_range_tls_13_12()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_ca_cert_path     = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path  = opt_cert_dir + "/client_cert-key.pem";
  opts.ssl_min_tls          = "1.3";
  opts.ssl_max_tls          = "1.2";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  // Impossible
  return expect_connection_failed();
}

int test_gzip_compression()
{
  otlp::OtlpHttpExporterOptions opts;

  set_common_opts(opts);
  opts.compression = "gzip";

  instrumented_payload(opts);

  if (opt_mode == MODE_NONE)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == MODE_HTTPS))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == MODE_HTTP))
  {
    return expect_connection_failed();
  }

  // Impossible
  return expect_connection_failed();
}
