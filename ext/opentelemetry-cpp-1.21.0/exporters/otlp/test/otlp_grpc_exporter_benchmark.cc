// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_recordable.h"

#include <benchmark/benchmark.h>
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/provider.h"

#ifdef BAZEL_BUILD
#  include "examples/common/foo_library/foo_library.h"
#else
#  include "foo_library/foo_library.h"
#endif

namespace trace     = opentelemetry::trace;
namespace nostd     = opentelemetry::nostd;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp      = opentelemetry::exporter::otlp;

#include <benchmark/benchmark.h>

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

namespace trace_api = opentelemetry::trace;

const int kBatchSize     = 200;
const int kNumAttributes = 5;
const int kNumIterations = 1000;

const trace::TraceId kTraceId(std::array<const uint8_t, trace::TraceId::kSize>(
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));
const trace::SpanId kSpanId(std::array<const uint8_t, trace::SpanId::kSize>({0, 0, 0, 0, 0, 0, 0,
                                                                             2}));
const trace::SpanId kParentSpanId(std::array<const uint8_t, trace::SpanId::kSize>({0, 0, 0, 0, 0, 0,
                                                                                   0, 3}));
const auto kTraceState = trace_api::TraceState::GetDefault()->Set("key1", "value");
const trace_api::SpanContext kSpanContext{
    kTraceId, kSpanId, trace_api::TraceFlags{trace_api::TraceFlags::kIsSampled}, true, kTraceState};

// ----------------------- Helper classes and functions ------------------------

// Create a fake service stub to avoid dependency on gmock
class FakeServiceStub : public proto::collector::trace::v1::TraceService::StubInterface
{
public:
#if defined(GRPC_CPP_VERSION_MAJOR) && \
    (GRPC_CPP_VERSION_MAJOR * 1000 + GRPC_CPP_VERSION_MINOR) >= 1039
  using async_interface_base =
      proto::collector::trace::v1::TraceService::StubInterface::async_interface;
#else
  using async_interface_base =
      proto::collector::trace::v1::TraceService::StubInterface::experimental_async_interface;
#endif

  FakeServiceStub() : async_interface_(this) {}

  class async_interface : public async_interface_base
  {
  public:
    async_interface(FakeServiceStub *owner) : stub_(owner) {}

    virtual ~async_interface() {}

    void Export(
        ::grpc::ClientContext *context,
        const ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request,
        ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse *response,
        std::function<void(::grpc::Status)> callback) override
    {
      callback(stub_->Export(context, *request, response));
    }

#if defined(GRPC_CPP_VERSION_MAJOR) &&                                      \
        (GRPC_CPP_VERSION_MAJOR * 1000 + GRPC_CPP_VERSION_MINOR) >= 1039 || \
    defined(GRPC_CALLBACK_API_NONEXPERIMENTAL)
    void Export(
        ::grpc::ClientContext * /*context*/,
        const ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest * /*request*/,
        ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse * /*response*/,
        ::grpc::ClientUnaryReactor * /*reactor*/) override
    {}
#else
    void Export(
        ::grpc::ClientContext * /*context*/,
        const ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest * /*request*/,
        ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse * /*response*/,
        ::grpc::experimental::ClientUnaryReactor * /*reactor*/)
    {}
#endif

  private:
    FakeServiceStub *stub_;
  };

  async_interface_base *async() { return &async_interface_; }
  async_interface_base *experimental_async() { return &async_interface_; }

  grpc::Status Export(grpc::ClientContext *,
                      const proto::collector::trace::v1::ExportTraceServiceRequest &,
                      proto::collector::trace::v1::ExportTraceServiceResponse *) override
  {
    return grpc::Status::OK;
  }

  grpc::ClientAsyncResponseReaderInterface<
      proto::collector::trace::v1::ExportTraceServiceResponse> *
  AsyncExportRaw(grpc::ClientContext *,
                 const proto::collector::trace::v1::ExportTraceServiceRequest &,
                 grpc::CompletionQueue *) override
  {
    return nullptr;
  }

  grpc::ClientAsyncResponseReaderInterface<
      proto::collector::trace::v1::ExportTraceServiceResponse> *
  PrepareAsyncExportRaw(grpc::ClientContext *,
                        const proto::collector::trace::v1::ExportTraceServiceRequest &,
                        grpc::CompletionQueue *) override
  {
    return nullptr;
  }

private:
  async_interface async_interface_;
};

// OtlpGrpcExporterTestPeer is a friend class of OtlpGrpcExporter
class OtlpGrpcExporterTestPeer
{
public:
  std::unique_ptr<sdk::trace::SpanExporter> GetExporter()
  {
    auto mock_stub = new FakeServiceStub();
    std::unique_ptr<proto::collector::trace::v1::TraceService::StubInterface> stub_interface(
        mock_stub);
    return std::unique_ptr<sdk::trace::SpanExporter>(
        new exporter::otlp::OtlpGrpcExporter(std::move(stub_interface)));
  }
};

// Helper function to create empty spans
void CreateEmptySpans(std::array<std::unique_ptr<sdk::trace::Recordable>, kBatchSize> &recordables)
{
  for (int i = 0; i < kBatchSize; i++)
  {
    auto recordable = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);
    recordables[i]  = std::move(recordable);
  }
}

// Helper function to create sparse spans
void CreateSparseSpans(std::array<std::unique_ptr<sdk::trace::Recordable>, kBatchSize> &recordables)
{
  for (int i = 0; i < kBatchSize; i++)
  {
    auto recordable = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);

    recordable->SetIdentity(kSpanContext, kParentSpanId);
    recordable->SetName("TestSpan");
    recordable->SetStartTime(common::SystemTimestamp(std::chrono::system_clock::now()));
    recordable->SetDuration(std::chrono::nanoseconds(10));

    recordables[i] = std::move(recordable);
  }
}

// Helper function to create dense spans
void CreateDenseSpans(std::array<std::unique_ptr<sdk::trace::Recordable>, kBatchSize> &recordables)
{
  for (int i = 0; i < kBatchSize; i++)
  {
    auto recordable = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);

    recordable->SetIdentity(kSpanContext, kParentSpanId);
    recordable->SetName("TestSpan");
    recordable->SetStartTime(common::SystemTimestamp(std::chrono::system_clock::now()));
    recordable->SetDuration(std::chrono::nanoseconds(10));

    for (int i = 0; i < kNumAttributes; i++)
    {
      recordable->SetAttribute("int_key_" + i, static_cast<int64_t>(i));
      recordable->SetAttribute("str_key_" + i, "string_val_" + i);
      recordable->SetAttribute("bool_key_" + i, true);
    }

    recordables[i] = std::move(recordable);
  }
}

// ------------------------------ Benchmark tests ------------------------------

// Benchmark Export() with empty spans
void BM_OtlpExporterEmptySpans(benchmark::State &state)
{
  std::unique_ptr<OtlpGrpcExporterTestPeer> testpeer(new OtlpGrpcExporterTestPeer());
  auto exporter = testpeer->GetExporter();

  while (state.KeepRunningBatch(kNumIterations))
  {
    std::array<std::unique_ptr<sdk::trace::Recordable>, kBatchSize> recordables;
    CreateEmptySpans(recordables);
    exporter->Export(nostd::span<std::unique_ptr<sdk::trace::Recordable>>(recordables));
  }
}
BENCHMARK(BM_OtlpExporterEmptySpans);

// Benchmark Export() with sparse spans
void BM_OtlpExporterSparseSpans(benchmark::State &state)
{
  std::unique_ptr<OtlpGrpcExporterTestPeer> testpeer(new OtlpGrpcExporterTestPeer());
  auto exporter = testpeer->GetExporter();

  while (state.KeepRunningBatch(kNumIterations))
  {
    std::array<std::unique_ptr<sdk::trace::Recordable>, kBatchSize> recordables;
    CreateSparseSpans(recordables);
    exporter->Export(nostd::span<std::unique_ptr<sdk::trace::Recordable>>(recordables));
  }
}
BENCHMARK(BM_OtlpExporterSparseSpans);

// Benchmark Export() with dense spans
void BM_OtlpExporterDenseSpans(benchmark::State &state)
{
  std::unique_ptr<OtlpGrpcExporterTestPeer> testpeer(new OtlpGrpcExporterTestPeer());
  auto exporter = testpeer->GetExporter();

  while (state.KeepRunningBatch(kNumIterations))
  {
    std::array<std::unique_ptr<sdk::trace::Recordable>, kBatchSize> recordables;
    CreateDenseSpans(recordables);
    exporter->Export(nostd::span<std::unique_ptr<sdk::trace::Recordable>>(recordables));
  }
}
BENCHMARK(BM_OtlpExporterDenseSpans);

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE

#ifdef NDEBUG
// disabled as valgrind reports a memroy leak at absl::lts_20220623::random_internal::(anonymous
// namespace)::PoolAlignedAlloc()
// see PR #1737
namespace
{
opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
void InitTracer()
{
  // Create OTLP exporter instance
  auto exporter  = std::unique_ptr<trace_sdk::SpanExporter>(new otlp::OtlpGrpcExporter(opts));
  auto processor = std::unique_ptr<trace_sdk::SpanProcessor>(
      new trace_sdk::SimpleSpanProcessor(std::move(exporter)));
  auto provider =
      nostd::shared_ptr<trace::TracerProvider>(new trace_sdk::TracerProvider(std::move(processor)));
  // Set the global trace provider
  trace::Provider::SetTracerProvider(provider);
}

void BM_otlp_grpc_with_collector(benchmark::State &state)
{
  InitTracer();
  while (state.KeepRunning())
  {
    foo_library();
  }
}
BENCHMARK(BM_otlp_grpc_with_collector);
}  // namespace
#endif

BENCHMARK_MAIN();
