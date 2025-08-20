// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef _WIN32

#  include <gtest/gtest.h>
#  include <map>
#  include <string>
#  include "opentelemetry//sdk/trace/sampler.h"
#  include "opentelemetry/exporters/etw/etw_tracer_exporter.h"
#  include "opentelemetry/sdk/trace/samplers/always_off.h"
#  include "opentelemetry/sdk/trace/simple_processor.h"

using namespace OPENTELEMETRY_NAMESPACE;

using namespace opentelemetry::exporter::etw;
using namespace opentelemetry::sdk::trace;

const char *kGlobalProviderName = "OpenTelemetry-ETW-TLD";

std::string getTemporaryValue()
{
  return std::string("Value from Temporary std::string");
}

/**
 * A Mock Custom Id Generator
 */
class MockIdGenerator : public sdk::trace::IdGenerator
{
public:
  MockIdGenerator() : sdk::trace::IdGenerator(false) {}

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

/* A Custom Sampler, implementing parent based sampler*/
class MockSampler : public sdk::trace::Sampler
{
public:
  MockSampler(std::shared_ptr<Sampler> delegate_sampler) noexcept
      : delegate_sampler_(delegate_sampler)
  {}
  sdk::trace::SamplingResult ShouldSample(
      const trace_api::SpanContext &parent_context,
      trace_api::TraceId trace_id,
      nostd::string_view name,
      trace_api::SpanKind span_kind,
      const opentelemetry::common::KeyValueIterable &attributes,
      const trace_api::SpanContextKeyValueIterable &links) noexcept
  {
    if (!parent_context.IsValid())
    {
      // If no parent (root span) exists returns the result of the delegateSampler
      return delegate_sampler_->ShouldSample(parent_context, trace_id, name, span_kind, attributes,
                                             links);
    }

    // If parent exists:
    if (parent_context.IsSampled())
    {
      return {Decision::RECORD_AND_SAMPLE, nullptr, parent_context.trace_state()};
    }
    return {Decision::DROP, nullptr, parent_context.trace_state()};
  }

  nostd::string_view GetDescription() const noexcept { return "Custom Sampler"; }

private:
  std::shared_ptr<Sampler> delegate_sampler_;
};

class AlwaysOffTailSampler : public TailSampler
{
public:
  opentelemetry::sdk::trace::SamplingResult ShouldSample(
      const opentelemetry::trace::Span &span) noexcept override
  {
    return {opentelemetry::sdk::trace::Decision::DROP};
  }
};

/* clang-format off */
TEST(ETWTracer, TracerCheck)
{
  // SDK customer specifies their unique ETW ProviderName. Every component or library
  // is assumed to have its own instrumentation name. Traces are routed to dedicated
  // provider. Standard hash function maps from ProviderName to ProviderGUID.
  //
  // Prominent naming examples from `logman query providers` :
  //
  // [Docker]                                 {a3693192-9ed6-46d2-a981-f8226c8363bd}
  // ...
  // Intel-Autologger-iclsClient              {B8D7E9A0-65D5-40BE-AFEA-83593FC0164E}
  // Intel-Autologger-iclsProxy                {301B773F-50F3-4C8E-83F0-53BA9590A13E}
  // Intel-Autologger-PTTEKRecertification    {F33E9E07-8792-47E8-B3FA-2C92AB32C5B3}
  // ...
  // NodeJS-ETW-provider                      {77754E9B-264B-4D8D-B981-E4135C1ECB0C}
  // ...
  // OpenSSH                                  {C4B57D35-0636-4BC3-A262-370F249F9802}
  // ...
  // Windows Connect Now                      {C100BECE-D33A-4A4B-BF23-BBEF4663D017}
  // Windows Defender Firewall API            {28C9F48F-D244-45A8-842F-DC9FBC9B6E92}
  // Windows Defender Firewall API - GP       {0EFF663F-8B6E-4E6D-8182-087A8EAA29CB}
  // Windows Defender Firewall Driver         {D5E09122-D0B2-4235-ADC1-C89FAAAF1069}

  std::string providerName = kGlobalProviderName; // supply unique instrumentation name here
  exporter::etw::TracerProvider tp;

  auto tracer = tp.GetTracer(providerName);

  // Span attributes
  Properties outer_attribs =
  {
    {"attrib1", 1},
    {"attrib2", 2}
  };
  // copy the outer attributes
  Properties inner_attribs = outer_attribs;

  {
    auto topSpan = tracer->StartSpan("MySpanTop");
    auto topScope = tracer->WithActiveSpan(topSpan);
    {
      auto outerSpan = tracer->StartSpan("MySpanL2", outer_attribs);
      auto outerScope = tracer->WithActiveSpan(outerSpan);

      // Create nested span. Note how we share the attributes here.
      // It is Okay to either reuse/share or have your own attributes.
      {
        auto innerSpan = tracer->StartSpan("MySpanL3", inner_attribs);
        auto innerScope = tracer->WithActiveSpan(innerSpan);

        // Add span attribute
        EXPECT_NO_THROW(outerSpan->SetAttribute("AttrName1", "AttrValue1"));

        // Add first event
        std::string eventName1 = "MyEvent1";
        Properties event1 =
        {
          {"uint32Key", (uint32_t)1234},
          {"uint64Key", (uint64_t)1234567890},
          {"strKey", "someValue"}
        };
        EXPECT_NO_THROW(outerSpan->AddEvent(eventName1, event1));

        // Add second event
        std::string eventName2 = "MyEvent2";
        Properties event2 =
        {
          {"uint32Key", (uint32_t)9876},
          {"uint64Key", (uint64_t)987654321},
          {"strKey", "anotherValue"}
        };
        EXPECT_NO_THROW(outerSpan->AddEvent(eventName2, event2));

        std::string eventName3= "MyEvent3";
        Properties event3 =
        {
          /* Extra metadata that allows event to flow to A.I. pipeline */
          {"metadata", "ai_event"},
          {"uint32Key", (uint32_t)9876},
          {"uint64Key", (uint64_t)987654321},
          // {"int32array", {{-1,0,1,2,3}} },
          {"tempString", getTemporaryValue() }
        };
        EXPECT_NO_THROW(innerSpan->AddEvent(eventName3, event3));
        EXPECT_NO_THROW(innerSpan->End());

      }
      EXPECT_NO_THROW(outerSpan->End());

    }
    EXPECT_NO_THROW(topSpan->End());
  }
#  if OPENTELEMETRY_ABI_VERSION_NO == 1
  EXPECT_NO_THROW(tracer->CloseWithMicroseconds(0));
#  endif
}

// Lowest decoration level -> smaller ETW event size.
// Expected output in C# listener on the other side:
// no ActivityID GUID, no SpanId, no TraceId.
/*
{
  "Timestamp": "2021-03-19T21:04:38.411193-07:00",
  "ProviderName": "OpenTelemetry-ETW-TLD",
  "Id": 13,
  "Message": null,
  "ProcessId": 15120,
  "Level": "Always",
  "Keywords": "0x0000000000000000",
  "EventName": "C.min/Stop",
  "ActivityID": null,
  "RelatedActivityID": null,
  "Payload": {}
}
*/
TEST(ETWTracer, TracerCheckMinDecoration)
{
  std::string providerName = kGlobalProviderName;
  exporter::etw::TracerProvider tp
  ({
      {"enableTraceId", false},
      {"enableSpanId", false},
      {"enableActivityId", false},
      {"enableActivityTracking", true},
      {"enableRelatedActivityId", false},
      {"enableAutoParent", false}
  });
  auto tracer = tp.GetTracer(providerName);
  {
    auto aSpan = tracer->StartSpan("A.min");
    auto aScope = tracer->WithActiveSpan(aSpan);
    {
      auto bSpan = tracer->StartSpan("B.min");
      auto bScope = tracer->WithActiveSpan(bSpan);
      {
        auto cSpan = tracer->StartSpan("C.min");
        auto cScope = tracer->WithActiveSpan(cSpan);
        EXPECT_NO_THROW(cSpan->End());
      }
      EXPECT_NO_THROW(bSpan->End());
    }
    EXPECT_NO_THROW(aSpan->End());
}
#  if OPENTELEMETRY_ABI_VERSION_NO == 1
  tracer->CloseWithMicroseconds(0);
#  endif
}

// Highest decoration level -> larger ETW event size
// Expected output in C# listener on the other side:
// ActivityID GUID (==SpanId), SpanId, TraceId.
/*
{
  "Timestamp": "2021-03-19T21:04:38.4120274-07:00",
  "ProviderName": "OpenTelemetry-ETW-TLD",
  "Id": 21,
  "Message": null,
  "ProcessId": 15120,
  "Level": "Always",
  "Keywords": "0x0000000000000000",
  "EventName": "C.max/Stop",
  "ActivityID": "d55a2c25-8033-40ab-0000-000000000000",
  "RelatedActivityID": null,
  "Payload": {
    "SpanId": "252c5ad53380ab40",
    "TraceId": "4dea2a63c188894ea5ab979e5cd7ec36"
  }
}
*/
TEST(ETWTracer, TracerCheckMaxDecoration)
{
  std::string providerName = kGlobalProviderName;
  exporter::etw::TracerProvider tp
  ({
      {"enableTraceId", true},
      {"enableSpanId", true},
      {"enableActivityId", true},
      {"enableRelatedActivityId", true},
      {"enableAutoParent", true}
  });
  auto tracer = tp.GetTracer(providerName);
  {
    auto aSpan = tracer->StartSpan("A.max");
    auto aScope = tracer->WithActiveSpan(aSpan);
    {
      auto bSpan = tracer->StartSpan("B.max");
      auto bScope = tracer->WithActiveSpan(bSpan);
      {
        auto cSpan = tracer->StartSpan("C.max");
        auto cScope = tracer->WithActiveSpan(cSpan);
        EXPECT_NO_THROW(cSpan->End());
      }
      EXPECT_NO_THROW(bSpan->End());
    }
    EXPECT_NO_THROW(aSpan->End());
  }
#  if OPENTELEMETRY_ABI_VERSION_NO == 1
  tracer->CloseWithMicroseconds(0);
#  endif
}

TEST(ETWTracer, TracerCheckMsgPack)
{
  std::string providerName = "OpenTelemetry-ETW-MsgPack";
  exporter::etw::TracerProvider tp
  ({
      {"enableTraceId", true},
      {"enableSpanId", true},
      {"enableActivityId", true},
      {"enableRelatedActivityId", true},
      {"enableAutoParent", true}
  });
  auto tracer = tp.GetTracer(providerName);
  {
      auto aSpan = tracer->StartSpan("A.max");
      auto aScope = tracer->WithActiveSpan(aSpan);
      {
          auto bSpan = tracer->StartSpan("B.max");
          auto bScope = tracer->WithActiveSpan(bSpan);
          {
              auto cSpan = tracer->StartSpan("C.max");
              auto cScope = tracer->WithActiveSpan(cSpan);
              std::string eventName = "MyMsgPackEvent";
              Properties event =
              {
                  {"uint32Key", (uint32_t)1234},
                  {"uint64Key", (uint64_t)1234567890},
                  {"strKey", "someValue"}
              };
              cSpan->AddEvent(eventName, event);
              EXPECT_NO_THROW(cSpan->End());
          }
          EXPECT_NO_THROW(bSpan->End());
      }
      EXPECT_NO_THROW(aSpan->End());
  }
#  if OPENTELEMETRY_ABI_VERSION_NO == 1
  tracer->CloseWithMicroseconds(0);
#  endif
}

/**
 * @brief Global Tracer singleton may be placed in .h header and
 * shared across different compilation units. All would get the
 * same object.
 *
 * @return Single global tracer instance.
*/
static OPENTELEMETRY_NAMESPACE::trace::TracerProvider& GetGlobalTracerProvider()
{
    static exporter::etw::TracerProvider tp
    ({
      {"enableTraceId", true},
      {"enableSpanId", true},
      {"enableActivityId", true},
      {"enableRelatedActivityId", true},
      {"enableAutoParent", true}
     });
    return tp;
}

static OPENTELEMETRY_NAMESPACE::trace::Tracer& GetGlobalTracer()
{
    static auto tracer = GetGlobalTracerProvider().GetTracer(kGlobalProviderName);
    return (*tracer.get());
}

TEST(ETWTracer, GlobalSingletonTracer)
{
  // Obtain a global tracer using C++11 magic static.
  auto& globalTracer = GetGlobalTracer();
  auto s1 = globalTracer.StartSpan("Span1");
  auto traceId1 = s1->GetContext().trace_id();
  s1->End();
/* === Span 1 - "TraceId": "182a64258fb1864ca4e1a542eecbd9bf"
{
  "Timestamp": "2021-05-10T11:45:27.028827-07:00",
  "ProviderName": "OpenTelemetry-ETW-TLD",
  "Id": 5,
  "Message": null,
  "ProcessId": 23712,
  "Level": "Always",
  "Keywords": "0x0000000000000000",
  "EventName": "Span",
  "ActivityID": "6ed94703-6b0a-4e76-0000-000000000000",
  "RelatedActivityID": null,
  "Payload": {
    "Duration": 23456,
    "Kind": 1,
    "Name": "Span1",
    "SpanId": "0347d96e0a6b764e",
    "StartTime": "2021-05-10T18:45:27.066411500Z",
    "StatusCode": 0,
    "StatusMessage": "",
    "Success": "True",
    "TraceId": "182a64258fb1864ca4e1a542eecbd9bf",
    "_name": "Span"
  }
}
*/

  // Obtain a different tracer withs its own trace-id.
  auto localTracer = GetGlobalTracerProvider().GetTracer(kGlobalProviderName);
  auto s2 = localTracer->StartSpan("Span2");
  auto traceId2 = s2->GetContext().trace_id();
  s2->End();
/* === Span 2 - "TraceId": "334bf9a1eed98d40a873a606295a9368"
{
  "Timestamp": "2021-05-10T11:45:27.0289654-07:00",
  "ProviderName": "OpenTelemetry-ETW-TLD",
  "Id": 5,
  "Message": null,
  "ProcessId": 23712,
  "Level": "Always",
  "Keywords": "0x0000000000000000",
  "EventName": "Span",
  "ActivityID": "3b7b2ecb-2e84-4903-0000-000000000000",
  "RelatedActivityID": null,
  "Payload": {
    "Duration": 03434,
    "Kind": 1,
    "Name": "Span2",
    "SpanId": "cb2e7b3b842e0349",
    "StartTime": "2021-05-10T18:45:27.066411500Z",
    "StatusCode": 0,
    "StatusMessage": "",
    "Success": "True",
    "TraceId": "334bf9a1eed98d40a873a606295a9368",
    "_name": "Span"
  }
}
*/

  // Obtain the same global tracer with the same trace-id as before.
  auto& globalTracer2 = GetGlobalTracer();
  auto s3 = globalTracer2.StartSpan("Span3");
  auto traceId3 = s3->GetContext().trace_id();
  s3->End();
/* === Span 3 - "TraceId": "182a64258fb1864ca4e1a542eecbd9bf"
{
  "Timestamp": "2021-05-10T11:45:27.0290936-07:00",
  "ProviderName": "OpenTelemetry-ETW-TLD",
  "Id": 5,
  "Message": null,
  "ProcessId": 23712,
  "Level": "Always",
  "Keywords": "0x0000000000000000",
  "EventName": "Span",
  "ActivityID": "0a970247-ba0e-4d4b-0000-000000000000",
  "RelatedActivityID": null,
  "Payload": {
    "Duration": 12323,
    "Kind": 1,
    "Name": "Span3",
    "SpanId": "4702970a0eba4b4d",
    "StartTime": "2021-05-10T18:45:27.066411500Z",
    "StatusCode": 0,
    "StatusMessage": "",
    "Success": "True",
    "TraceId": "182a64258fb1864ca4e1a542eecbd9bf",
    "_name": "Span"
  }
}
*/
  EXPECT_NE(traceId1, traceId2);
  EXPECT_EQ(traceId1, traceId3);

#  if OPENTELEMETRY_ABI_VERSION_NO == 1
  localTracer->CloseWithMicroseconds(0);
  globalTracer.CloseWithMicroseconds(0);
#  endif
}

TEST(ETWTracer, AlwayOffSampler)
{
  std::string providerName = kGlobalProviderName; // supply unique instrumentation name here
  std::unique_ptr<sdk::trace::Sampler> always_off{new sdk::trace::AlwaysOffSampler()};
  exporter::etw::TracerProvider tp
    ({
      {"enableTraceId", true},
      {"enableSpanId", true},
      {"enableActivityId", true},
      {"enableRelatedActivityId", true},
      {"enableAutoParent", true}
     },
     std::move(always_off));
  auto tracer = tp.GetTracer(providerName);
  auto span = tracer->StartSpan("span_off");
  EXPECT_EQ(span->GetContext().IsValid(), true);
  EXPECT_EQ(span->GetContext().IsSampled(), false);
}

TEST(ETWTracer, AlwayOffTailSampler)
{
  std::string providerName = kGlobalProviderName; // supply unique instrumentation name here
  std::unique_ptr<sdk::trace::Sampler> always_on{new sdk::trace::AlwaysOnSampler()};
  sdk::trace::IdGenerator *id_generator = new MockIdGenerator();
  std::unique_ptr<TailSampler> always_off_tail{new AlwaysOffTailSampler()};
  exporter::etw::TracerProvider tp
    ({
      {"enableTraceId", true},
      {"enableSpanId", true},
      {"enableActivityId", true},
      {"enableRelatedActivityId", true},
      {"enableAutoParent", true}
     },
     std::move(always_on),
     std::unique_ptr<sdk::trace::IdGenerator>(id_generator),
     std::move(always_off_tail));
  auto tracer = tp.GetTracer(providerName);
}

TEST(ETWTracer, CustomIdGenerator)
{
  std::string providerName = kGlobalProviderName; // supply unique instrumentation name here
  sdk::trace::IdGenerator *id_generator = new MockIdGenerator();
  std::unique_ptr<sdk::trace::Sampler> always_on{new sdk::trace::AlwaysOnSampler()};
  exporter::etw::TracerProvider tp
    ({
      {"enableTraceId", true},
      {"enableSpanId", true},
      {"enableActivityId", true},
      {"enableRelatedActivityId", true},
      {"enableAutoParent", true}
     },
     std::move(always_on),
     std::unique_ptr<sdk::trace::IdGenerator>(id_generator));
  auto tracer = tp.GetTracer(providerName);
  auto span = tracer->StartSpan("span_on");
  EXPECT_EQ(span->GetContext().trace_id(), id_generator->GenerateTraceId());
}

TEST(ETWTracer, CustomSampler)
{
  std::string providerName = kGlobalProviderName; // supply unique instrumentation name here
  auto parent_off = std::unique_ptr<Sampler>(new MockSampler(std::make_shared<AlwaysOnSampler>()));
  exporter::etw::TracerProvider tp
    ({
      {"enableTraceId", true},
      {"enableSpanId", true},
      {"enableActivityId", true},
      {"enableRelatedActivityId", true},
      {"enableAutoParent", true}
     },
     std::move(parent_off));
  auto tracer = tp.GetTracer(providerName);
  {
    auto span = tracer->StartSpan("span_off");
    EXPECT_EQ(span->GetContext().IsValid(), true);
    EXPECT_EQ(span->GetContext().IsSampled(), true);
    auto scope = tracer->WithActiveSpan(span);
    auto trace_id = span->GetContext().trace_id();
    {
      auto child_span = tracer->StartSpan("span on");
      EXPECT_EQ(child_span->GetContext().IsValid(), true);
      EXPECT_EQ(child_span->GetContext().IsSampled(), true);
      EXPECT_EQ(child_span->GetContext().trace_id(), trace_id);
    }
  }
}

TEST(ETWTracer, EndWithCustomTime)
{
  // Obtain a global tracer using C++11 magic static.
  auto& globalTracer = GetGlobalTracer();
  auto s1 = globalTracer.StartSpan("Span1");
  auto traceId1 = s1->GetContext().trace_id();
  opentelemetry::trace::EndSpanOptions end;
  end.end_steady_time =  opentelemetry::common::SteadyTimestamp(std::chrono::nanoseconds(40));
  s1->End(end);
  auto end_time = static_cast<opentelemetry::exporter::etw::Span *>(s1.get())->GetEndTime();
  EXPECT_EQ(end.end_steady_time.time_since_epoch(), end_time.time_since_epoch());

}

/* clang-format on */

#endif
