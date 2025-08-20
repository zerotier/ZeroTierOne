// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/macros.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/exporters/memory/in_memory_span_data.h"
#include "opentelemetry/exporters/memory/in_memory_span_exporter.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/id_generator.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/random_id_generator.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/sdk/trace/samplers/always_off.h"
#include "opentelemetry/sdk/trace/samplers/always_on.h"
#include "opentelemetry/sdk/trace/samplers/parent.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/sdk/trace/tracer.h"
#include "opentelemetry/sdk/trace/tracer_config.h"
#include "opentelemetry/sdk/trace/tracer_context.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/noop.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_context_kv_iterable.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/span_startoptions.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"
#include "opentelemetry/trace/tracer.h"

using namespace opentelemetry::sdk::trace;
using namespace opentelemetry::sdk::resource;
using namespace opentelemetry::sdk::instrumentationscope;
using opentelemetry::common::SteadyTimestamp;
using opentelemetry::common::SystemTimestamp;
namespace nostd     = opentelemetry::nostd;
namespace common    = opentelemetry::common;
namespace trace_api = opentelemetry::trace;
using opentelemetry::exporter::memory::InMemorySpanData;
using opentelemetry::exporter::memory::InMemorySpanExporter;
using opentelemetry::trace::SpanContext;

/**
 * A mock sampler with ShouldSample returning:
 *  Decision::RECORD_AND_SAMPLE if trace_id is valid
 *  Decision::DROP otherwise.
 */
class MockSampler final : public Sampler
{
public:
  SamplingResult ShouldSample(
      const SpanContext & /*parent_context*/,
      trace_api::TraceId trace_id,
      nostd::string_view /*name*/,
      trace_api::SpanKind /*span_kind*/,
      const opentelemetry::common::KeyValueIterable & /*attributes*/,
      const opentelemetry::trace::SpanContextKeyValueIterable & /*links*/) noexcept override
  {
    // Sample only if valid trace_id ( This is to test Sampler get's valid trace id)
    if (trace_id.IsValid())
    {
      // Return two pairs of attributes. These attributes should be added to the
      // span attributes
      return {Decision::RECORD_AND_SAMPLE,
              nostd::unique_ptr<const std::map<std::string, opentelemetry::common::AttributeValue>>(
                  new const std::map<std::string, opentelemetry::common::AttributeValue>(
                      {{"sampling_attr1", 123}, {"sampling_attr2", "string"}})),
              nostd::shared_ptr<opentelemetry::trace::TraceState>(nullptr)};
    }
    else
    {
      // we should never reach here
      assert(false);
      return {Decision::DROP,
              nostd::unique_ptr<const std::map<std::string, opentelemetry::common::AttributeValue>>(
                  nullptr),
              nostd::shared_ptr<opentelemetry::trace::TraceState>(nullptr)};
    }
  }

  nostd::string_view GetDescription() const noexcept override { return "MockSampler"; }
};

/**
 * A Mock Custom ID Generator
 */
class MockIdGenerator : public IdGenerator
{
public:
  MockIdGenerator() : IdGenerator(false) {}

  opentelemetry::trace::SpanId GenerateSpanId() noexcept override
  {
    return opentelemetry::trace::SpanId(buf_span);
  }

  opentelemetry::trace::TraceId GenerateTraceId() noexcept override
  {
    return opentelemetry::trace::TraceId(buf_trace);
  }
  uint8_t buf_span[8]   = {1, 2, 3, 4, 5, 6, 7, 8};
  uint8_t buf_trace[16] = {1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1};
};

namespace
{
std::shared_ptr<opentelemetry::trace::Tracer> initTracer(std::unique_ptr<SpanExporter> &&exporter)
{
  auto processor = std::unique_ptr<SpanProcessor>(new SimpleSpanProcessor(std::move(exporter)));
  std::vector<std::unique_ptr<SpanProcessor>> processors;
  processors.push_back(std::move(processor));
  auto context = std::make_shared<TracerContext>(std::move(processors));
  return std::shared_ptr<opentelemetry::trace::Tracer>(new Tracer(context));
}

std::shared_ptr<opentelemetry::trace::Tracer> initTracer(
    std::unique_ptr<SpanExporter> &&exporter,
    // For testing, just shove a pointer over, we'll take it over.
    Sampler *sampler,
    IdGenerator *id_generator = new RandomIdGenerator,
    const ScopeConfigurator<TracerConfig> &tracer_configurator =
        ScopeConfigurator<TracerConfig>::Builder(TracerConfig::Default()).Build(),
    std::unique_ptr<InstrumentationScope> scope = InstrumentationScope::Create(""))
{
  auto processor = std::unique_ptr<SpanProcessor>(new SimpleSpanProcessor(std::move(exporter)));
  std::vector<std::unique_ptr<SpanProcessor>> processors;
  processors.push_back(std::move(processor));
  auto resource = Resource::Create({});
  auto context  = std::make_shared<TracerContext>(
      std::move(processors), resource, std::unique_ptr<Sampler>(sampler),
      std::unique_ptr<IdGenerator>(id_generator),
      std::make_unique<ScopeConfigurator<TracerConfig>>(tracer_configurator));
  return std::shared_ptr<opentelemetry::trace::Tracer>(new Tracer(context, std::move(scope)));
}

}  // namespace

TEST(Tracer, ToInMemorySpanExporter)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  auto span_first  = tracer->StartSpan("span 1");
  auto scope_first = tracer->WithActiveSpan(span_first);
  auto span_second = tracer->StartSpan("span 2");

  ASSERT_EQ(0, span_data->GetSpans().size());

  span_second->End();

  auto span2 = span_data->GetSpans();
  ASSERT_EQ(1, span2.size());
  ASSERT_EQ("span 2", span2.at(0)->GetName());
  EXPECT_TRUE(span2.at(0)->GetTraceId().IsValid());
  EXPECT_TRUE(span2.at(0)->GetSpanId().IsValid());
  EXPECT_TRUE(span2.at(0)->GetParentSpanId().IsValid());

  span_first->End();

  auto span1 = span_data->GetSpans();
  ASSERT_EQ(1, span1.size());
  ASSERT_EQ("span 1", span1.at(0)->GetName());
  EXPECT_TRUE(span1.at(0)->GetTraceId().IsValid());
  EXPECT_TRUE(span1.at(0)->GetSpanId().IsValid());
  EXPECT_FALSE(span1.at(0)->GetParentSpanId().IsValid());

  // Verify trace and parent span id propagation
  EXPECT_EQ(span1.at(0)->GetTraceId(), span2.at(0)->GetTraceId());
  EXPECT_EQ(span2.at(0)->GetParentSpanId(), span1.at(0)->GetSpanId());
}

TEST(Tracer, StartSpanSampleOn)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_on                              = initTracer(std::unique_ptr<SpanExporter>{exporter});

  tracer_on->StartSpan("span 1")->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_LT(std::chrono::nanoseconds(0), cur_span_data->GetStartTime().time_since_epoch());
  ASSERT_LT(std::chrono::nanoseconds(0), cur_span_data->GetDuration());
}

TEST(Tracer, StartSpanSampleOff)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_off = initTracer(std::unique_ptr<SpanExporter>{exporter}, new AlwaysOffSampler());

  // This span will not be recorded.
  auto span = tracer_off->StartSpan("span 2");

  // Always generate a valid span-context (span-id)
  auto context = span->GetContext();
  EXPECT_TRUE(context.IsValid());
  EXPECT_FALSE(context.IsSampled());

  span->End();
  // The span doesn't write any span data because the sampling decision is alway
  // DROP.
  ASSERT_EQ(0, span_data->GetSpans().size());
}

TEST(Tracer, StartSpanCustomIdGenerator)
{
  IdGenerator *id_generator                   = new MockIdGenerator();
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer =
      initTracer(std::unique_ptr<SpanExporter>{exporter}, new AlwaysOnSampler(), id_generator);

  tracer->StartSpan("span 1")->End();
  auto spans          = span_data->GetSpans();
  auto &cur_span_data = spans.at(0);

  EXPECT_EQ(cur_span_data->GetTraceId(), id_generator->GenerateTraceId());
  EXPECT_EQ(cur_span_data->GetSpanId(), id_generator->GenerateSpanId());
}

TEST(Tracer, StartSpanWithOptionsTime)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  opentelemetry::trace::StartSpanOptions start;
  start.start_system_time = SystemTimestamp(std::chrono::nanoseconds(300));
  start.start_steady_time = SteadyTimestamp(std::chrono::nanoseconds(10));

  opentelemetry::trace::EndSpanOptions end;
  end.end_steady_time = SteadyTimestamp(std::chrono::nanoseconds(40));

  tracer->StartSpan("span 1", start)->End(end);

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(std::chrono::nanoseconds(300), cur_span_data->GetStartTime().time_since_epoch());
  ASSERT_EQ(std::chrono::nanoseconds(30), cur_span_data->GetDuration());
}

TEST(Tracer, StartSpanWithAttributes)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  // Start a span with all supported scalar attribute types.
  tracer
      ->StartSpan("span 1", {{"attr1", "string"},
                             {"attr2", false},
                             {"attr1", 314159},
                             {"attr3", static_cast<unsigned int>(314159)},
                             {"attr4", static_cast<int32_t>(-20)},
                             {"attr5", static_cast<uint32_t>(20)},
                             {"attr6", static_cast<int64_t>(-20)},
                             {"attr7", static_cast<uint64_t>(20)},
                             {"attr8", 3.1},
                             {"attr9", "string"}})
      ->End();

  // Start a span with all supported array attribute types.
  int listInt[]                       = {1, 2, 3};
  unsigned int listUInt[]             = {1, 2, 3};
  int32_t listInt32[]                 = {1, -2, 3};
  uint32_t listUInt32[]               = {1, 2, 3};
  int64_t listInt64[]                 = {1, -2, 3};
  uint64_t listUInt64[]               = {1, 2, 3};
  double listDouble[]                 = {1.1, 2.1, 3.1};
  bool listBool[]                     = {true, false};
  nostd::string_view listStringView[] = {"a", "b"};
  std::map<std::string, common::AttributeValue> m;
  m["attr1"] = nostd::span<int>(listInt);
  m["attr2"] = nostd::span<unsigned int>(listUInt);
  m["attr3"] = nostd::span<int32_t>(listInt32);
  m["attr4"] = nostd::span<uint32_t>(listUInt32);
  m["attr5"] = nostd::span<int64_t>(listInt64);
  m["attr6"] = nostd::span<uint64_t>(listUInt64);
  m["attr7"] = nostd::span<double>(listDouble);
  m["attr8"] = nostd::span<bool>(listBool);
  m["attr9"] = nostd::span<nostd::string_view>(listStringView);

  tracer->StartSpan("span 2", m)->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(2, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(9, cur_span_data->GetAttributes().size());
  ASSERT_EQ(314159, nostd::get<int32_t>(cur_span_data->GetAttributes().at("attr1")));
  ASSERT_EQ(false, nostd::get<bool>(cur_span_data->GetAttributes().at("attr2")));
  ASSERT_EQ(static_cast<uint32_t>(314159),
            nostd::get<uint32_t>(cur_span_data->GetAttributes().at("attr3")));
  ASSERT_EQ(-20, nostd::get<int32_t>(cur_span_data->GetAttributes().at("attr4")));
  ASSERT_EQ(static_cast<uint32_t>(20),
            nostd::get<uint32_t>(cur_span_data->GetAttributes().at("attr5")));
  ASSERT_EQ(-20, nostd::get<int64_t>(cur_span_data->GetAttributes().at("attr6")));
  ASSERT_EQ(static_cast<uint64_t>(20),
            nostd::get<uint64_t>(cur_span_data->GetAttributes().at("attr7")));
  ASSERT_EQ(3.1, nostd::get<double>(cur_span_data->GetAttributes().at("attr8")));
  ASSERT_EQ("string", nostd::get<std::string>(cur_span_data->GetAttributes().at("attr9")));

  auto &cur_span_data2 = spans.at(1);
  ASSERT_EQ(9, cur_span_data2->GetAttributes().size());
  ASSERT_EQ(std::vector<int32_t>({1, 2, 3}),
            nostd::get<std::vector<int32_t>>(cur_span_data2->GetAttributes().at("attr1")));
  ASSERT_EQ(std::vector<uint32_t>({1, 2, 3}),
            nostd::get<std::vector<uint32_t>>(cur_span_data2->GetAttributes().at("attr2")));
  ASSERT_EQ(std::vector<int32_t>({1, -2, 3}),
            nostd::get<std::vector<int32_t>>(cur_span_data2->GetAttributes().at("attr3")));
  ASSERT_EQ(std::vector<uint32_t>({1, 2, 3}),
            nostd::get<std::vector<uint32_t>>(cur_span_data2->GetAttributes().at("attr4")));
  ASSERT_EQ(std::vector<int64_t>({1, -2, 3}),
            nostd::get<std::vector<int64_t>>(cur_span_data2->GetAttributes().at("attr5")));
  ASSERT_EQ(std::vector<uint64_t>({1, 2, 3}),
            nostd::get<std::vector<uint64_t>>(cur_span_data2->GetAttributes().at("attr6")));
  ASSERT_EQ(std::vector<double>({1.1, 2.1, 3.1}),
            nostd::get<std::vector<double>>(cur_span_data2->GetAttributes().at("attr7")));
  ASSERT_EQ(std::vector<bool>({true, false}),
            nostd::get<std::vector<bool>>(cur_span_data2->GetAttributes().at("attr8")));
  ASSERT_EQ(std::vector<std::string>({"a", "b"}),
            nostd::get<std::vector<std::string>>(cur_span_data2->GetAttributes().at("attr9")));
}

TEST(Tracer, StartSpanWithAttributesCopy)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  {
    std::unique_ptr<std::vector<int64_t>> numbers(new std::vector<int64_t>);
    numbers->push_back(1);
    numbers->push_back(2);
    numbers->push_back(3);

    std::unique_ptr<std::vector<nostd::string_view>> strings(new std::vector<nostd::string_view>);
    std::string s1("a");
    std::string s2("b");
    std::string s3("c");
    strings->push_back(s1);
    strings->push_back(s2);
    strings->push_back(s3);
    tracer
        ->StartSpan("span 1",
                    {{"attr1", *numbers}, {"attr2", nostd::span<nostd::string_view>(*strings)}})
        ->End();
  }

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(2, cur_span_data->GetAttributes().size());

  auto numbers = nostd::get<std::vector<int64_t>>(cur_span_data->GetAttributes().at("attr1"));
  ASSERT_EQ(3, numbers.size());
  ASSERT_EQ(1, numbers[0]);
  ASSERT_EQ(2, numbers[1]);
  ASSERT_EQ(3, numbers[2]);

  auto strings = nostd::get<std::vector<std::string>>(cur_span_data->GetAttributes().at("attr2"));
  ASSERT_EQ(3, strings.size());
  ASSERT_EQ("a", strings[0]);
  ASSERT_EQ("b", strings[1]);
  ASSERT_EQ("c", strings[2]);
}

TEST(Tracer, GetSampler)
{
  auto resource = Resource::Create({});
  // Create a Tracer with a default AlwaysOnSampler
  auto tracer_on = initTracer(nullptr);

#ifdef OPENTELEMETRY_RTTI_ENABLED
  auto &t1 = std::dynamic_pointer_cast<Tracer>(tracer_on)->GetSampler();
#else
  auto &t1 = std::static_pointer_cast<Tracer>(tracer_on)->GetSampler();
#endif
  ASSERT_EQ("AlwaysOnSampler", t1.GetDescription());

  // Create a Tracer with a AlwaysOffSampler
  auto tracer_off = initTracer(nullptr, new AlwaysOffSampler());

#ifdef OPENTELEMETRY_RTTI_ENABLED
  auto &t2 = std::dynamic_pointer_cast<Tracer>(tracer_off)->GetSampler();
#else
  auto &t2 = std::static_pointer_cast<Tracer>(tracer_off)->GetSampler();
#endif
  ASSERT_EQ("AlwaysOffSampler", t2.GetDescription());
}

TEST(Tracer, SpanSetAttribute)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  auto span = tracer->StartSpan("span 1");

  span->SetAttribute("abc", 3.1);

  span->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());
  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(3.1, nostd::get<double>(cur_span_data->GetAttributes().at("abc")));
}

TEST(Tracer, TestAfterEnd)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});
  auto span                                   = tracer->StartSpan("span 1");
  span->SetAttribute("abc", 3.1);

  span->End();

  // test after end
  span->SetAttribute("testing null recordable", 3.1);
  span->AddEvent("event 1");
  span->AddEvent("event 2", std::chrono::system_clock::now());
  span->AddEvent("event 3", std::chrono::system_clock::now(), {{"attr1", 1}});
  std::string new_name{"new name"};
  span->UpdateName(new_name);
  span->SetAttribute("attr1", 3.1);
  std::string description{"description"};
  span->SetStatus(opentelemetry::trace::StatusCode::kError, description);
  span->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());
  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(3.1, nostd::get<double>(cur_span_data->GetAttributes().at("abc")));
}

TEST(Tracer, SpanSetEvents)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  auto span = tracer->StartSpan("span 1");
  span->AddEvent("event 1");
  span->AddEvent("event 2", std::chrono::system_clock::now());
  span->AddEvent("event 3", std::chrono::system_clock::now(), {{"attr1", 1}});
  span->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &span_data_events = spans.at(0)->GetEvents();
  ASSERT_EQ(3, span_data_events.size());
  ASSERT_EQ("event 1", span_data_events[0].GetName());
  ASSERT_EQ("event 2", span_data_events[1].GetName());
  ASSERT_EQ("event 3", span_data_events[2].GetName());
  ASSERT_EQ(0, span_data_events[0].GetAttributes().size());
  ASSERT_EQ(0, span_data_events[1].GetAttributes().size());
  ASSERT_EQ(1, span_data_events[2].GetAttributes().size());
}

TEST(Tracer, StartSpanWithDisabledConfig)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  ScopeConfigurator<TracerConfig> disable_tracer =
      ScopeConfigurator<TracerConfig>::Builder(TracerConfig::Disabled()).Build();
  auto tracer = initTracer(std::unique_ptr<SpanExporter>{exporter}, new AlwaysOnSampler(),
                           new RandomIdGenerator(), disable_tracer);
  auto span   = tracer->StartSpan("span 1");

  std::shared_ptr<opentelemetry::trace::Tracer> noop_tracer =
      std::make_shared<opentelemetry::trace::NoopTracer>();
  auto noop_span = noop_tracer->StartSpan("noop");
  EXPECT_TRUE(span.get() == noop_span.get());

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  EXPECT_FALSE(noop_tracer->Enabled());
  EXPECT_FALSE(tracer->Enabled());
#endif
}

TEST(Tracer, StartSpanWithEnabledConfig)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  ScopeConfigurator<TracerConfig> enable_tracer =
      ScopeConfigurator<TracerConfig>::Builder(TracerConfig::Enabled()).Build();
  auto tracer = initTracer(std::unique_ptr<SpanExporter>{exporter}, new AlwaysOnSampler(),
                           new RandomIdGenerator(), enable_tracer);
  auto span   = tracer->StartSpan("span 1");

  std::shared_ptr<opentelemetry::trace::Tracer> noop_tracer =
      std::make_shared<opentelemetry::trace::NoopTracer>();
  auto noop_span = noop_tracer->StartSpan("noop");
  EXPECT_FALSE(span.get() == noop_span.get());

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  EXPECT_FALSE(noop_tracer->Enabled());
  EXPECT_TRUE(tracer->Enabled());
#endif
}

TEST(Tracer, StartSpanWithCustomConfig)
{
  std::shared_ptr<opentelemetry::trace::Tracer> noop_tracer =
      std::make_shared<opentelemetry::trace::NoopTracer>();
  auto noop_span                = noop_tracer->StartSpan("noop");
  auto check_if_version_present = [](const InstrumentationScope &scope_info) {
    return !scope_info.GetVersion().empty();
  };
  ScopeConfigurator<TracerConfig> custom_configurator =
      ScopeConfigurator<TracerConfig>::Builder(TracerConfig::Enabled())
          .AddCondition(check_if_version_present, TracerConfig::Enabled())
          .AddConditionNameEquals("foo_library", TracerConfig::Disabled())
          .AddConditionNameEquals("", TracerConfig::Disabled())
          .Build();

  const auto tracer_default_scope =
      initTracer(std::unique_ptr<SpanExporter>{new InMemorySpanExporter()}, new AlwaysOnSampler(),
                 new RandomIdGenerator(), custom_configurator);
  const auto span_default_scope = tracer_default_scope->StartSpan("span 1");
  EXPECT_TRUE(span_default_scope == noop_span);

  auto foo_scope = InstrumentationScope::Create("foo_library");
  const auto tracer_foo_scope =
      initTracer(std::unique_ptr<SpanExporter>{new InMemorySpanExporter()}, new AlwaysOnSampler(),
                 new RandomIdGenerator(), custom_configurator, std::move(foo_scope));
  const auto span_foo_scope = tracer_foo_scope->StartSpan("span 1");
  EXPECT_TRUE(span_foo_scope == noop_span);

  auto foo_scope_with_version = InstrumentationScope::Create("foo_library", "1.0.0");
  const auto tracer_foo_scope_with_version =
      initTracer(std::unique_ptr<SpanExporter>{new InMemorySpanExporter()}, new AlwaysOnSampler(),
                 new RandomIdGenerator(), custom_configurator, std::move(foo_scope_with_version));
  const auto span_foo_scope_with_version = tracer_foo_scope_with_version->StartSpan("span 1");
  EXPECT_FALSE(span_foo_scope_with_version == noop_span);

  auto bar_scope = InstrumentationScope::Create("bar_library");
  auto tracer_bar_scope =
      initTracer(std::unique_ptr<SpanExporter>{new InMemorySpanExporter()}, new AlwaysOnSampler(),
                 new RandomIdGenerator(), custom_configurator, std::move(bar_scope));
  auto span_bar_scope = tracer_bar_scope->StartSpan("span 1");
  EXPECT_FALSE(span_bar_scope == noop_span);

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  EXPECT_FALSE(noop_tracer->Enabled());
  EXPECT_FALSE(tracer_default_scope->Enabled());
  EXPECT_FALSE(tracer_foo_scope->Enabled());
  EXPECT_TRUE(tracer_foo_scope_with_version->Enabled());
  EXPECT_TRUE(tracer_bar_scope->Enabled());
#endif
}

TEST(Tracer, StartSpanWithCustomConfigDifferingConditionOrder)
{
  std::shared_ptr<opentelemetry::trace::Tracer> noop_tracer =
      std::make_shared<opentelemetry::trace::NoopTracer>();
  auto noop_span                = noop_tracer->StartSpan("noop");
  auto check_if_version_present = [](const InstrumentationScope &scope_info) {
    return !scope_info.GetVersion().empty();
  };
  // 2 configurators with same conditions, but different order
  ScopeConfigurator<TracerConfig> custom_configurator_1 =
      ScopeConfigurator<TracerConfig>::Builder(TracerConfig::Enabled())
          .AddCondition(check_if_version_present, TracerConfig::Enabled())
          .AddConditionNameEquals("foo_library", TracerConfig::Disabled())
          .Build();
  ScopeConfigurator<TracerConfig> custom_configurator_2 =
      ScopeConfigurator<TracerConfig>::Builder(TracerConfig::Enabled())
          .AddConditionNameEquals("foo_library", TracerConfig::Disabled())
          .AddCondition(check_if_version_present, TracerConfig::Enabled())
          .Build();

  auto foo_scope_with_version_1 = InstrumentationScope::Create("foo_library", "1.0.0");
  auto foo_scope_with_version_2 = InstrumentationScope::Create("foo_library", "1.0.0");

  const auto tracer_foo_scope_with_version_1 = initTracer(
      std::unique_ptr<SpanExporter>{new InMemorySpanExporter()}, new AlwaysOnSampler(),
      new RandomIdGenerator(), custom_configurator_1, std::move(foo_scope_with_version_1));

  const auto tracer_foo_scope_with_version_2 = initTracer(
      std::unique_ptr<SpanExporter>{new InMemorySpanExporter()}, new AlwaysOnSampler(),
      new RandomIdGenerator(), custom_configurator_2, std::move(foo_scope_with_version_2));

  // Custom configurator 1 evaluates version first and enables the tracer
  const auto span_foo_scope_with_version_1 = tracer_foo_scope_with_version_1->StartSpan("span 1");
  EXPECT_FALSE(span_foo_scope_with_version_1 == noop_span);

  // Custom configurator 2 evaluates the name first and therefore disables the tracer without
  // evaluating other condition
  const auto span_foo_scope_with_version_2 = tracer_foo_scope_with_version_2->StartSpan("span 1");
  EXPECT_TRUE(span_foo_scope_with_version_2 == noop_span);

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  EXPECT_TRUE(tracer_foo_scope_with_version_1->Enabled());
  EXPECT_FALSE(tracer_foo_scope_with_version_2->Enabled());
#endif
}

TEST(Tracer, SpanSetLinks)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  {

    // Single span link passed through Initialization list
    tracer->StartSpan("efg", {{"attr1", 1}}, {{SpanContext(false, false), {{"attr2", 2}}}})->End();
    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(1, span_data_links.size());
    auto link = span_data_links.at(0);
    ASSERT_EQ(nostd::get<int>(link.GetAttributes().at("attr2")), 2);
  }
  {

    // Multiple span links passed through Initialization list
    tracer
        ->StartSpan("efg", {{"attr1", 1}},
                    {{SpanContext(false, false), {{"attr2", 2}}},
                     {SpanContext(false, false), {{"attr3", 3}}}})
        ->End();
    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(2, span_data_links.size());
    auto link1 = span_data_links.at(0);
    ASSERT_EQ(nostd::get<int>(link1.GetAttributes().at("attr2")), 2);
    auto link2 = span_data_links.at(1);
    ASSERT_EQ(nostd::get<int>(link2.GetAttributes().at("attr3")), 3);
  }

  {

    // Multiple links, each with multiple attributes passed through Initialization list
    tracer
        ->StartSpan("efg", {{"attr1", 1}},
                    {{SpanContext(false, false), {{"attr2", 2}, {"attr3", 3}}},
                     {SpanContext(false, false), {{"attr4", 4}}}})
        ->End();
    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(2, span_data_links.size());
    auto link1 = span_data_links.at(0);
    ASSERT_EQ(nostd::get<int>(link1.GetAttributes().at("attr2")), 2);
    ASSERT_EQ(nostd::get<int>(link1.GetAttributes().at("attr3")), 3);
    auto link2 = span_data_links.at(1);
    ASSERT_EQ(nostd::get<int>(link2.GetAttributes().at("attr4")), 4);
  }

  {
    std::map<std::string, std::string> attrs1 = {{"attr1", "1"}, {"attr2", "2"}};
    std::map<std::string, std::string> attrs2 = {{"attr3", "3"}, {"attr4", "4"}};

    std::vector<std::pair<SpanContext, std::map<std::string, std::string>>> links = {
        {SpanContext(false, false), attrs1}, {SpanContext(false, false), attrs2}};
    tracer->StartSpan("efg", attrs1, links)->End();
    auto spans = span_data->GetSpans();

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(2, span_data_links.size());
    auto link1 = span_data_links.at(0);
    ASSERT_EQ(nostd::get<std::string>(link1.GetAttributes().at("attr1")), "1");
    ASSERT_EQ(nostd::get<std::string>(link1.GetAttributes().at("attr2")), "2");
    auto link2 = span_data_links.at(1);
    ASSERT_EQ(nostd::get<std::string>(link2.GetAttributes().at("attr3")), "3");
    ASSERT_EQ(nostd::get<std::string>(link2.GetAttributes().at("attr4")), "4");
  }
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
TEST(Tracer, SpanAddLinkAbiv2)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  {
    auto span = tracer->StartSpan("span");
    SpanContext target(false, false);
    // Single link attribute passed through Initialization list
    span->AddLink(target, {{"attr2", 2}});
    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(1, span_data_links.size());
    auto link  = span_data_links.at(0);
    auto attrs = link.GetAttributes();
    ASSERT_EQ(nostd::get<int>(attrs.at("attr2")), 2);
  }

  {
    auto span = tracer->StartSpan("span");
    SpanContext target(false, false);
    // Multiple link attributes passed through Initialization list
    span->AddLink(target, {{"attr2", 2}, {"attr3", 3}});
    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(1, span_data_links.size());
    auto link  = span_data_links.at(0);
    auto attrs = link.GetAttributes();
    ASSERT_EQ(nostd::get<int>(attrs.at("attr2")), 2);
    ASSERT_EQ(nostd::get<int>(attrs.at("attr3")), 3);
  }

  {
    std::map<std::string, std::string> attrs_map = {{"attr1", "1"}, {"attr2", "2"}};

    auto span = tracer->StartSpan("span");
    SpanContext target(false, false);
    span->AddLink(target, attrs_map);
    span->End();

    auto spans = span_data->GetSpans();

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(1, span_data_links.size());
    auto link  = span_data_links.at(0);
    auto attrs = link.GetAttributes();
    ASSERT_EQ(nostd::get<std::string>(attrs.at("attr1")), "1");
    ASSERT_EQ(nostd::get<std::string>(attrs.at("attr2")), "2");
  }

  {
    auto span = tracer->StartSpan("span");
    SpanContext target(false, false);

    // Single link attribute passed through Initialization list
    span->AddLink(target, {{"attr1", 1}});

    // Multiple link attributes passed through Initialization list
    span->AddLink(target, {{"attr2", 2}, {"attr3", 3}});

    std::map<std::string, std::string> attrs_map = {{"attr4", "4"}, {"attr5", "5"}};
    span->AddLink(target, attrs_map);

    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(3, span_data_links.size());

    {
      auto link  = span_data_links.at(0);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr1")), 1);
    }

    {
      auto link  = span_data_links.at(1);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr2")), 2);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr3")), 3);
    }

    {
      auto link  = span_data_links.at(2);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attr4")), "4");
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attr5")), "5");
    }
  }
}

TEST(Tracer, SpanAddLinksAbiv2)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  {
    auto span = tracer->StartSpan("span");
    // Single span link passed through Initialization list
    span->AddLinks({{SpanContext(false, false), {{"attr2", 2}}}});
    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(1, span_data_links.size());
    auto link = span_data_links.at(0);
    ASSERT_EQ(nostd::get<int>(link.GetAttributes().at("attr2")), 2);
  }

  {
    auto span = tracer->StartSpan("span");
    // Multiple span links passed through Initialization list
    span->AddLinks(
        {{SpanContext(false, false), {{"attr2", 2}}}, {SpanContext(false, false), {{"attr3", 3}}}});
    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(2, span_data_links.size());
    auto link1 = span_data_links.at(0);
    ASSERT_EQ(nostd::get<int>(link1.GetAttributes().at("attr2")), 2);
    auto link2 = span_data_links.at(1);
    ASSERT_EQ(nostd::get<int>(link2.GetAttributes().at("attr3")), 3);
  }

  {
    auto span = tracer->StartSpan("span");
    // Multiple links, each with multiple attributes passed through Initialization list
    span->AddLinks({{SpanContext(false, false), {{"attr2", 2}, {"attr3", 3}}},
                    {SpanContext(false, false), {{"attr4", 4}}}});
    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(2, span_data_links.size());
    auto link1 = span_data_links.at(0);
    ASSERT_EQ(nostd::get<int>(link1.GetAttributes().at("attr2")), 2);
    ASSERT_EQ(nostd::get<int>(link1.GetAttributes().at("attr3")), 3);
    auto link2 = span_data_links.at(1);
    ASSERT_EQ(nostd::get<int>(link2.GetAttributes().at("attr4")), 4);
  }

  {
    std::map<std::string, std::string> attrs1 = {{"attr1", "1"}, {"attr2", "2"}};
    std::map<std::string, std::string> attrs2 = {{"attr3", "3"}, {"attr4", "4"}};

    std::vector<std::pair<SpanContext, std::map<std::string, std::string>>> links = {
        {SpanContext(false, false), attrs1}, {SpanContext(false, false), attrs2}};

    auto span = tracer->StartSpan("span");
    span->AddLinks(links);
    span->End();

    auto spans = span_data->GetSpans();

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(2, span_data_links.size());
    auto link1 = span_data_links.at(0);
    ASSERT_EQ(nostd::get<std::string>(link1.GetAttributes().at("attr1")), "1");
    ASSERT_EQ(nostd::get<std::string>(link1.GetAttributes().at("attr2")), "2");
    auto link2 = span_data_links.at(1);
    ASSERT_EQ(nostd::get<std::string>(link2.GetAttributes().at("attr3")), "3");
    ASSERT_EQ(nostd::get<std::string>(link2.GetAttributes().at("attr4")), "4");
  }

  {
    auto span = tracer->StartSpan("span");

    // Single span link passed through Initialization list
    span->AddLinks({{SpanContext(false, false), {{"attr10", 10}}}});
    span->AddLinks({{SpanContext(false, false), {{"attr11", 11}}}});

    // Multiple span links passed through Initialization list
    span->AddLinks({{SpanContext(false, false), {{"attr12", 12}}},
                    {SpanContext(false, false), {{"attr13", 13}}}});
    span->AddLinks({{SpanContext(false, false), {{"attr14", 14}}},
                    {SpanContext(false, false), {{"attr15", 15}}}});

    // Multiple links, each with multiple attributes passed through Initialization list
    span->AddLinks({{SpanContext(false, false), {{"attr16", 16}, {"attr17", 17}}},
                    {SpanContext(false, false), {{"attr18", 18}}}});
    span->AddLinks({{SpanContext(false, false), {{"attr19", 19}, {"attr20", 20}}},
                    {SpanContext(false, false), {{"attr21", 21}}}});

    std::map<std::string, std::string> attrsa1 = {{"attra1", "1"}, {"attra2", "2"}};
    std::map<std::string, std::string> attrsa2 = {{"attra3", "3"}, {"attra4", "4"}};

    std::vector<std::pair<SpanContext, std::map<std::string, std::string>>> linksa = {
        {SpanContext(false, false), attrsa1}, {SpanContext(false, false), attrsa2}};

    span->AddLinks(linksa);

    std::map<std::string, std::string> attrsb1 = {{"attrb1", "1"}, {"attrb2", "2"}};
    std::map<std::string, std::string> attrsb2 = {{"attrb3", "3"}, {"attrb4", "4"}};

    std::vector<std::pair<SpanContext, std::map<std::string, std::string>>> linksb = {
        {SpanContext(false, false), attrsb1}, {SpanContext(false, false), attrsb2}};

    span->AddLinks(linksb);

    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(14, span_data_links.size());

    {
      auto link  = span_data_links.at(0);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr10")), 10);
    }

    {
      auto link  = span_data_links.at(1);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr11")), 11);
    }

    {
      auto link  = span_data_links.at(2);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr12")), 12);
    }

    {
      auto link  = span_data_links.at(3);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr13")), 13);
    }

    {
      auto link  = span_data_links.at(4);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr14")), 14);
    }

    {
      auto link  = span_data_links.at(5);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr15")), 15);
    }

    {
      auto link  = span_data_links.at(6);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr16")), 16);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr17")), 17);
    }

    {
      auto link  = span_data_links.at(7);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr18")), 18);
    }

    {
      auto link  = span_data_links.at(8);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr19")), 19);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr20")), 20);
    }

    {
      auto link  = span_data_links.at(9);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr21")), 21);
    }

    {
      auto link  = span_data_links.at(10);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attra1")), "1");
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attra2")), "2");
    }

    {
      auto link  = span_data_links.at(11);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attra3")), "3");
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attra4")), "4");
    }

    {
      auto link  = span_data_links.at(12);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attrb1")), "1");
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attrb2")), "2");
    }

    {
      auto link  = span_data_links.at(13);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 2);
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attrb3")), "3");
      ASSERT_EQ(nostd::get<std::string>(attrs.at("attrb4")), "4");
    }
  }
}

TEST(Tracer, SpanMixLinksAbiv2)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  {
    // Link 1 added at StartSpan
    auto span =
        tracer->StartSpan("span", {{"attr1", 1}}, {{SpanContext(false, false), {{"attr2", 2}}}});

    SpanContext target(false, false);
    // Link 2 added with AddLink
    span->AddLink(target, {{"attr3", 3}});

    // Link 3 added with AddLinks
    span->AddLinks({{SpanContext(false, false), {{"attr4", 4}}}});

    span->End();

    auto spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());

    auto &span_data_links = spans.at(0)->GetLinks();
    ASSERT_EQ(3, span_data_links.size());

    {
      auto link  = span_data_links.at(0);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr2")), 2);
    }

    {
      auto link  = span_data_links.at(1);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr3")), 3);
    }

    {
      auto link  = span_data_links.at(2);
      auto attrs = link.GetAttributes();
      ASSERT_EQ(attrs.size(), 1);
      ASSERT_EQ(nostd::get<int>(attrs.at("attr4")), 4);
    }
  }
}
#endif /* OPENTELEMETRY_ABI_VERSION_NO >= 2 */

TEST(Tracer, TestAlwaysOnSampler)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_on                              = initTracer(std::unique_ptr<SpanExporter>{exporter});

  // Testing AlwaysOn sampler.
  // Create two spans for each tracer. Check the exported result.
  auto span_on_1 = tracer_on->StartSpan("span 1");
  auto span_on_2 = tracer_on->StartSpan("span 2");
  span_on_2->End();
  span_on_1->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(2, spans.size());
  ASSERT_EQ("span 2", spans.at(0)->GetName());  // span 2 ends first.
  ASSERT_EQ("span 1", spans.at(1)->GetName());
}

TEST(Tracer, TestAlwaysOffSampler)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_off = initTracer(std::unique_ptr<SpanExporter>{exporter}, new AlwaysOffSampler());
  auto span_off_1 = tracer_off->StartSpan("span 1");
  auto span_off_2 = tracer_off->StartSpan("span 2");

  span_off_1->SetAttribute("attr1", 3.1);  // Not recorded.

  span_off_2->End();
  span_off_1->End();

  // The tracer export nothing with an AlwaysOff sampler
  ASSERT_EQ(0, span_data->GetSpans().size());
}

TEST(Tracer, TestParentBasedSampler)
{
  // Current ShouldSample always pass an empty ParentContext,
  // so this sampler will work as an AlwaysOnSampler.
  InMemorySpanExporter *exporter                        = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data_parent_on = exporter->GetData();
  auto tracer_parent_on = initTracer(std::unique_ptr<SpanExporter>{exporter},
                                     new ParentBasedSampler(std::make_shared<AlwaysOnSampler>()));

  auto span_parent_on_1 = tracer_parent_on->StartSpan("span 1");
  auto span_parent_on_2 = tracer_parent_on->StartSpan("span 2");

  span_parent_on_1->SetAttribute("attr1", 3.1);

  span_parent_on_2->End();
  span_parent_on_1->End();

  auto spans = span_data_parent_on->GetSpans();
  ASSERT_EQ(2, spans.size());
  ASSERT_EQ("span 2", spans.at(0)->GetName());
  ASSERT_EQ("span 1", spans.at(1)->GetName());

  // Current ShouldSample always pass an empty ParentContext,
  // so this sampler will work as an AlwaysOnSampler.
  InMemorySpanExporter *exporter2                        = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data_parent_off = exporter2->GetData();
  auto tracer_parent_off =
      initTracer(std::unique_ptr<SpanExporter>{exporter2},
                 // Add this to avoid different results for old and new version of clang-format
                 new ParentBasedSampler(std::make_shared<AlwaysOffSampler>()));

  auto span_parent_off_1 = tracer_parent_off->StartSpan("span 1");
  auto span_parent_off_2 = tracer_parent_off->StartSpan("span 2");

  span_parent_off_1->SetAttribute("attr1", 3.1);

  span_parent_off_1->End();
  span_parent_off_2->End();
  ASSERT_EQ(0, span_data_parent_off->GetSpans().size());
}

TEST(Tracer, WithActiveSpan)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});
  auto spans                                  = span_data.get()->GetSpans();

  ASSERT_EQ(0, spans.size());

  {
    auto span_first  = tracer->StartSpan("span 1");
    auto scope_first = tracer->WithActiveSpan(span_first);

    {
      auto span_second  = tracer->StartSpan("span 2");
      auto scope_second = tracer->WithActiveSpan(span_second);

      spans = span_data->GetSpans();
      ASSERT_EQ(0, spans.size());

      span_second->End();
    }

    spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());
    EXPECT_EQ("span 2", spans.at(0)->GetName());
    EXPECT_EQ(spans.at(0).get()->GetParentSpanId(), span_first->GetContext().span_id());
    EXPECT_EQ(spans.at(0).get()->GetTraceId(), span_first->GetContext().trace_id());

    {
      trace_api::StartSpanOptions options;
      opentelemetry::context::Context c1;
      c1             = c1.SetValue(opentelemetry::trace::kIsRootSpanKey, true);
      options.parent = c1;
      auto root_span = tracer->StartSpan("span root", options);

      spans = span_data->GetSpans();
      ASSERT_EQ(0, spans.size());

      root_span->End();
    }
    spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());
    EXPECT_EQ("span root", spans.at(0)->GetName());
    EXPECT_EQ(spans.at(0).get()->GetParentSpanId(), opentelemetry::trace::SpanId());
    EXPECT_NE(spans.at(0).get()->GetTraceId(), span_first->GetContext().trace_id());

    span_first->End();
  }

  spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());
  EXPECT_EQ("span 1", spans.at(0)->GetName());
}

TEST(Tracer, ExpectParent)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});
  auto spans                                  = span_data.get()->GetSpans();

  ASSERT_EQ(0, spans.size());

  auto span_first = tracer->StartSpan("span 1");

  trace_api::StartSpanOptions options;
  options.parent   = span_first->GetContext();
  auto span_second = tracer->StartSpan("span 2", options);

  options.parent  = span_second->GetContext();
  auto span_third = tracer->StartSpan("span 3", options);

  span_third->End();
  span_second->End();
  span_first->End();

  spans = span_data->GetSpans();
  ASSERT_EQ(3, spans.size());
  auto spandata_first  = std::move(spans.at(2));
  auto spandata_second = std::move(spans.at(1));
  auto spandata_third  = std::move(spans.at(0));
  EXPECT_EQ("span 1", spandata_first->GetName());
  EXPECT_EQ("span 2", spandata_second->GetName());
  EXPECT_EQ("span 3", spandata_third->GetName());

  EXPECT_EQ(spandata_first->GetSpanId(), spandata_second->GetParentSpanId());
  EXPECT_EQ(spandata_second->GetSpanId(), spandata_third->GetParentSpanId());
}

TEST(Tracer, ExpectParentAsContext)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});
  auto spans                                  = span_data.get()->GetSpans();

  ASSERT_EQ(0, spans.size());

  auto span_first = tracer->StartSpan("span 1");

  opentelemetry::context::Context c1;
  auto c2 = trace_api::SetSpan(c1, span_first);
  trace_api::StartSpanOptions options;
  options.parent   = c2;
  auto span_second = tracer->StartSpan("span 2", options);

  auto c3         = trace_api::SetSpan(c2, span_second);
  options.parent  = c3;
  auto span_third = tracer->StartSpan("span 3", options);

  span_third->End();
  span_second->End();
  span_first->End();

  spans = span_data->GetSpans();
  ASSERT_EQ(3, spans.size());
  auto spandata_first  = std::move(spans.at(2));
  auto spandata_second = std::move(spans.at(1));
  auto spandata_third  = std::move(spans.at(0));
  EXPECT_EQ("span 1", spandata_first->GetName());
  EXPECT_EQ("span 2", spandata_second->GetName());
  EXPECT_EQ("span 3", spandata_third->GetName());

  EXPECT_EQ(spandata_first->GetSpanId(), spandata_second->GetParentSpanId());
  EXPECT_EQ(spandata_second->GetSpanId(), spandata_third->GetParentSpanId());
}

TEST(Tracer, ValidTraceIdToSampler)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::unique_ptr<SpanExporter>{exporter});

  auto span = tracer->StartSpan("span 1");
  // sampler was fed with valid trace_id, so span shouldn't be NoOp Span.
  EXPECT_TRUE(span->IsRecording());
  EXPECT_TRUE(span->GetContext().IsValid());
}

TEST(Tracer, SpanCleanupWithScope)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer = initTracer(std::unique_ptr<SpanExporter>{exporter}, new MockSampler());
  {
    auto span0 = tracer->StartSpan("Span0");
    auto span1 = tracer->StartSpan("span1");
    {
      trace_api::Scope scope1(span1);
      auto span2 = tracer->StartSpan("span2");
      {
        trace_api::Scope scope2(span2);
        auto span3 = tracer->StartSpan("span3");
      }
    }
  }
  EXPECT_EQ(4, span_data->GetSpans().size());
}
