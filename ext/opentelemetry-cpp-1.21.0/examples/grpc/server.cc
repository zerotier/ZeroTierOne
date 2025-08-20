// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/string_ref.h>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <string>
#include <utility>

#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/semconv/incubating/rpc_attributes.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/span_startoptions.h"
#include "opentelemetry/trace/tracer.h"
#include "tracer_common.h"

#ifdef BAZEL_BUILD
#  include "examples/grpc/protos/messages.grpc.pb.h"
#  include "examples/grpc/protos/messages.pb.h"
#else
#  include "messages.grpc.pb.h"
#  include "messages.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using grpc_example::Greeter;
using grpc_example::GreetRequest;
using grpc_example::GreetResponse;

using Span        = opentelemetry::trace::Span;
using SpanContext = opentelemetry::trace::SpanContext;
using namespace opentelemetry::trace;

namespace context = opentelemetry::context;
namespace semconv = opentelemetry::semconv;

namespace
{
class GreeterServer final : public Greeter::Service
{
public:
  Status Greet(ServerContext *context,
               const GreetRequest *request,
               GreetResponse *response) override
  {
    for (const auto &elem : context->client_metadata())
    {
      std::cout << "ELEM: " << elem.first << " " << elem.second << "\n";
    }

    // Create a SpanOptions object and set the kind to Server to inform OpenTel.
    StartSpanOptions options;
    options.kind = SpanKind::kServer;

    // extract context from grpc metadata
    GrpcServerCarrier carrier(context);

    auto prop        = context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
    auto current_ctx = context::RuntimeContext::GetCurrent();
    auto new_context = prop->Extract(carrier, current_ctx);
    options.parent   = GetSpan(new_context)->GetContext();

    std::string span_name = "GreeterService/Greet";
    auto span             = get_tracer("grpc")->StartSpan(span_name,
                                                          {{semconv::rpc::kRpcSystem, "grpc"},
                                                           {semconv::rpc::kRpcService, "GreeterService"},
                                                           {semconv::rpc::kRpcMethod, "Greet"},
                                                           {semconv::rpc::kRpcGrpcStatusCode, 0}},
                                                          options);
    auto scope            = get_tracer("grpc")->WithActiveSpan(span);

    // Fetch and parse whatever HTTP headers we can from the gRPC request.
    span->AddEvent("Processing client attributes");

    const std::string &req = request->request();
    std::cout << '\n' << "grpc_client says: " << req << '\n';
    std::string message = "The pleasure is mine.";
    // Send response to client
    response->set_response(message);
    span->AddEvent("Response sent to client");

    span->SetStatus(StatusCode::kOk);
    // Make sure to end your spans!
    span->End();
    return Status::OK;
  }
};  // GreeterServer class

void RunServer(uint16_t port)
{
  std::string address("0.0.0.0:" + std::to_string(port));
  GreeterServer service;
  ServerBuilder builder;

  builder.RegisterService(&service);
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on port: " << address << '\n';
  server->Wait();
  server->Shutdown();
}
}  // namespace

int main(int argc, char **argv)
{
  InitTracer();
  constexpr uint16_t default_port = 8800;
  uint16_t port;
  if (argc > 1)
  {
    port = atoi(argv[1]);
  }
  else
  {
    port = default_port;
  }

  RunServer(port);
  CleanupTracer();
  return 0;
}
