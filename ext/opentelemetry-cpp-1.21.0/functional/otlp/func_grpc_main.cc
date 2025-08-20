// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <utility>

#include "opentelemetry/exporters/otlp/otlp_environment.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/provider.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_id.h"
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

enum class TestMode : std::uint8_t
{
  kNone,
  kHttp,
  kHttps
};

bool opt_help   = false;
bool opt_list   = false;
bool opt_debug  = false;
bool opt_secure = false;
// HTTPS by default
std::string opt_endpoint = "https://127.0.0.1:4317";
std::string opt_cert_dir;
std::string opt_test_name;
TestMode opt_mode = TestMode::kNone;

/*
  Log parsing
*/

struct TestResult
{
  bool found_connection_failed = false;
  bool found_export_error      = false;
  bool found_export_success    = false;
  bool deadline_exceeded       = false;
  bool empty_address_list      = false;

  void reset()
  {
    found_connection_failed = false;
    found_export_error      = false;
    found_export_success    = false;
    deadline_exceeded       = false;
    empty_address_list      = false;
  }
};

struct TestResult g_test_result;

void parse_error_msg(TestResult *result, const std::string &msg)
{
  static std::string connection_failed("failed to connect to all addresses");

  if (msg.find(connection_failed) != std::string::npos)
  {
    result->found_connection_failed = true;
  }

  static std::string export_failed("Export() failed with status_code");

  if (msg.find(export_failed) != std::string::npos)
  {
    result->found_export_error = true;
  }

  static std::string deadline_exceeded("Deadline Exceeded");

  if (msg.find(deadline_exceeded) != std::string::npos)
  {
    result->deadline_exceeded = true;
  }

  static std::string empty_address_list("empty address list:");

  if (msg.find(empty_address_list) != std::string::npos)
  {
    result->empty_address_list = true;
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

void init(const otlp::OtlpGrpcExporterOptions &opts)
{
  // Create OTLP exporter instance
  auto exporter  = otlp::OtlpGrpcExporterFactory::Create(opts);
  auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor));
  // Set the global trace provider
  trace_sdk::Provider::SetTracerProvider(provider);
}

void payload()
{
  static const nostd::string_view k_tracer_name("func_test");
  static const nostd::string_view k_span_name("func_grpc_main");
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

void instrumented_payload(const otlp::OtlpGrpcExporterOptions &opts)
{
  g_test_result.reset();
  init(opts);
  payload();
  cleanup();
}

void usage(FILE *out)
{
  static const char *msg =
      "Usage: func_otlp_grpc [options] test_name\n"
      "Valid options are:\n"
      "  --help            Print this help\n"
      "  --list            List test names\n"
      "  --endpoint url    OTLP gRPC endpoint (https://localhost:4317/ by default)\n"
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
          opt_mode = TestMode::kNone;
        }
        else if (mode == "http")
        {
          opt_mode = TestMode::kHttp;
        }
        else if (mode == "https")
        {
          opt_mode = TestMode::kHttps;
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
int test_client_cert_not_found();
int test_client_cert_invalid();
int test_client_cert_unreadable();
int test_client_cert_no_key();
int test_client_key_not_found();
int test_client_key_invalid();
int test_client_key_unreadable();

int test_mtls_ok();

static const test_case all_tests[] = {{"basic", test_basic},
                                      {"cert-not-found", test_cert_not_found},
                                      {"cert-invalid", test_cert_invalid},
                                      {"cert-unreadable", test_cert_unreadable},
#ifdef ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
                                      {"client-cert-not-found", test_client_cert_not_found},
                                      {"client-cert-invalid", test_client_cert_invalid},
                                      {"client-cert-unreadable", test_client_cert_unreadable},
                                      {"client-cert-no-key", test_client_cert_no_key},
                                      {"client-key-not-found", test_client_key_not_found},
                                      {"client-key-invalid", test_client_key_invalid},
                                      {"client-key-unreadable", test_client_key_unreadable},
                                      {"mtls-ok", test_mtls_ok},
#endif  // ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
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

void set_common_opts(otlp::OtlpGrpcExporterOptions &opts)
{
  opts.endpoint            = opt_endpoint;
  opts.timeout             = std::chrono::milliseconds{100};
  opts.use_ssl_credentials = (opt_mode == TestMode::kHttps);

#ifdef ENABLE_ASYNC_EXPORT
  // Work around, ASYNC export not working.
  opts.max_concurrent_requests = 0;
#endif
}

int expect_connection_failed()
{
  if (g_test_result.found_export_error &&
      (g_test_result.found_connection_failed || g_test_result.deadline_exceeded ||
       g_test_result.empty_address_list))
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
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  if (opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_connection_failed();
  }

  return expect_connection_failed();
}

int test_cert_not_found()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/no-such-file.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_cert_invalid()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/garbage.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_cert_unreadable()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/unreadable.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

#ifdef ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
int test_client_cert_not_found()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/no-such-file.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_cert_invalid()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/garbage.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_cert_unreadable()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/unreadable.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_cert_no_key()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/client_cert.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_key_not_found()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path         = opt_cert_dir + "/no-such-file.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_key_invalid()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path         = opt_cert_dir + "/garbage.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_client_key_unreadable()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path         = opt_cert_dir + "/unreadable.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_success();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_export_failed();
  }

  return expect_connection_failed();
}

int test_mtls_ok()
{
  otlp::OtlpGrpcExporterOptions opts;

  set_common_opts(opts);
  opts.ssl_credentials_cacert_path = opt_cert_dir + "/ca.pem";
  opts.ssl_client_cert_path        = opt_cert_dir + "/client_cert.pem";
  opts.ssl_client_key_path         = opt_cert_dir + "/client_cert-key.pem";

  instrumented_payload(opts);

  if (opt_mode == TestMode::kNone)
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_connection_failed();
  }

  if (!opt_secure && (opt_mode == TestMode::kHttps))
  {
    return expect_success();
  }

  if (opt_secure && (opt_mode == TestMode::kHttp))
  {
    return expect_connection_failed();
  }

  return expect_success();
}
#endif  // ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW
