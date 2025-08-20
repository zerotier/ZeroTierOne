// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/macros.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/id_generator.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/random_id_generator.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/sdk/trace/samplers/always_off.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer.h"
#include "opentelemetry/sdk/trace/tracer_context.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/tracer.h"

using namespace opentelemetry::sdk::trace;
using namespace opentelemetry::sdk::resource;

TEST(TracerProvider, GetTracer)
{
  std::unique_ptr<SpanProcessor> processor(new SimpleSpanProcessor(nullptr));
  std::vector<std::unique_ptr<SpanProcessor>> processors;
  processors.push_back(std::move(processor));
  std::unique_ptr<TracerContext> context1(
      new TracerContext(std::move(processors), Resource::Create({})));
  TracerProvider tp1(std::move(context1));
  auto t1 = tp1.GetTracer("test");
  auto t2 = tp1.GetTracer("test");
  auto t3 = tp1.GetTracer("different", "1.0.0");
  auto t4 = tp1.GetTracer("");
  auto t5 = tp1.GetTracer(opentelemetry::nostd::string_view{});
  auto t6 = tp1.GetTracer("different", "1.0.0", "https://opentelemetry.io/schemas/1.2.0");
  ASSERT_NE(nullptr, t1);
  ASSERT_NE(nullptr, t2);
  ASSERT_NE(nullptr, t3);
  ASSERT_NE(nullptr, t6);

  // Should return the same instance each time.
  ASSERT_EQ(t1, t2);
  ASSERT_NE(t1, t3);
  ASSERT_EQ(t4, t5);
  ASSERT_NE(t3, t6);

  // Should be an sdk::trace::Tracer with the processor attached.
#ifdef OPENTELEMETRY_RTTI_ENABLED
  auto sdkTracer1 = dynamic_cast<Tracer *>(t1.get());
#else
  auto sdkTracer1 = static_cast<Tracer *>(t1.get());
#endif
  ASSERT_NE(nullptr, sdkTracer1);
  ASSERT_EQ("AlwaysOnSampler", sdkTracer1->GetSampler().GetDescription());
  std::unique_ptr<SpanProcessor> processor2(new SimpleSpanProcessor(nullptr));
  std::vector<std::unique_ptr<SpanProcessor>> processors2;
  processors2.push_back(std::move(processor2));
  std::unique_ptr<TracerContext> context2(
      new TracerContext(std::move(processors2), Resource::Create({}),
                        std::unique_ptr<Sampler>(new AlwaysOffSampler()),
                        std::unique_ptr<IdGenerator>(new RandomIdGenerator)));
  TracerProvider tp2(std::move(context2));
#ifdef OPENTELEMETRY_RTTI_ENABLED
  auto sdkTracer2 = dynamic_cast<Tracer *>(tp2.GetTracer("test").get());
#else
  auto sdkTracer2 = static_cast<Tracer *>(tp2.GetTracer("test").get());
#endif
  ASSERT_EQ("AlwaysOffSampler", sdkTracer2->GetSampler().GetDescription());

  auto instrumentation_scope1 = sdkTracer1->GetInstrumentationScope();
  ASSERT_EQ(instrumentation_scope1.GetName(), "test");
  ASSERT_EQ(instrumentation_scope1.GetVersion(), "");

  // Should be an sdk::trace::Tracer with the processor attached.
#ifdef OPENTELEMETRY_RTTI_ENABLED
  auto sdkTracer3 = dynamic_cast<Tracer *>(t3.get());
#else
  auto sdkTracer3 = static_cast<Tracer *>(t3.get());
#endif
  auto instrumentation_scope3 = sdkTracer3->GetInstrumentationScope();
  ASSERT_EQ(instrumentation_scope3.GetName(), "different");
  ASSERT_EQ(instrumentation_scope3.GetVersion(), "1.0.0");
}

TEST(TracerProvider, GetTracerEqualityCheck)
{
  auto processor = SimpleSpanProcessorFactory::Create(nullptr);
  auto provider  = TracerProviderFactory::Create(std::move(processor));

  // providing the same scope names should return the same tracer
  auto tracer_1a = provider->GetTracer("library_name");
  auto tracer_1b = provider->GetTracer("library_name");
  EXPECT_EQ(tracer_1a, tracer_1b);

  // providing the same scope name and version should return the same tracer
  auto tracer_version1a = provider->GetTracer("library_name", "v1.0");
  auto tracer_version1b = provider->GetTracer("library_name", "v1.0");
  EXPECT_EQ(tracer_version1a, tracer_version1b);

  // providing the same name, version, and schema urls should return the same tracer
  auto tracer_urla = provider->GetTracer("library_name", "v1.0", "url");
  auto tracer_urlb = provider->GetTracer("library_name", "v1.0", "url");
  EXPECT_EQ(tracer_urla, tracer_urlb);
}

TEST(TracerProvider, GetTracerInequalityCheck)
{
  auto processor = SimpleSpanProcessorFactory::Create(nullptr);
  auto provider  = TracerProviderFactory::Create(std::move(processor));

  auto tracer_library_1 = provider->GetTracer("library_1");
  auto tracer_library_2 = provider->GetTracer("library_2");
  auto tracer_version_1 = provider->GetTracer("library_1", "v1.0");
  auto tracer_version_2 = provider->GetTracer("library_1", "v2.0");
  auto tracer_url_1     = provider->GetTracer("library_1", "v1.0", "url_1");
  auto tracer_url_2     = provider->GetTracer("library_1", "v1.0", "url_2");

  // different scope names should return distinct tracers
  EXPECT_NE(tracer_library_1, tracer_library_2);

  // different scope versions should return distinct tracers
  EXPECT_NE(tracer_version_1, tracer_library_1);
  EXPECT_NE(tracer_version_1, tracer_version_2);

  // different scope schema urls should return distinct tracers
  EXPECT_NE(tracer_url_1, tracer_library_1);
  EXPECT_NE(tracer_url_1, tracer_version_1);
  EXPECT_NE(tracer_url_1, tracer_url_2);
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2

TEST(TracerProvider, GetTracerEqualityCheckAbiv2)
{
  auto processor = SimpleSpanProcessorFactory::Create(nullptr);
  auto provider  = TracerProviderFactory::Create(std::move(processor));

  auto tracer_attribute1a = provider->GetTracer("library_name", "v1.0", "url", {{"key", "one"}});
  auto tracer_attribute1b = provider->GetTracer("library_name", "v1.0", "url", {{"key", "one"}});

  // providing the same name, version, schema url and attributes should return the same tracer
  EXPECT_EQ(tracer_attribute1a, tracer_attribute1b);
}

TEST(TracerProvider, GetTracerInequalityCheckAbiv2)
{
  auto processor = SimpleSpanProcessorFactory::Create(nullptr);
  auto provider  = TracerProviderFactory::Create(std::move(processor));

  auto tracer_1          = provider->GetTracer("library_name", "v1.0", "url");
  auto tracer_attribute1 = provider->GetTracer("library_name", "v1.0", "url", {{"key", "one"}});
  auto tracer_attribute2 = provider->GetTracer("library_name", "v1.0", "url", {{"key", "two"}});

  // different scope attributes should return distinct tracers
  EXPECT_NE(tracer_attribute1, tracer_1);
  EXPECT_NE(tracer_attribute1, tracer_attribute2);
}

TEST(TracerProvider, GetTracerAbiv2)
{
  std::unique_ptr<SpanProcessor> processor(new SimpleSpanProcessor(nullptr));
  std::vector<std::unique_ptr<SpanProcessor>> processors;
  processors.push_back(std::move(processor));
  std::unique_ptr<TracerContext> context1(
      new TracerContext(std::move(processors), Resource::Create({})));
  TracerProvider tp(std::move(context1));

  auto t1 = tp.GetTracer("name1", "version1", "url1");
  ASSERT_NE(nullptr, t1);

  auto t2 = tp.GetTracer("name2", "version2", "url2", nullptr);
  ASSERT_NE(nullptr, t2);

  auto t3 = tp.GetTracer("name3", "version3", "url3", {{"accept_single_attr", true}});
  ASSERT_NE(nullptr, t3);
  {
    auto tracer = static_cast<opentelemetry::sdk::trace::Tracer *>(t3.get());
    auto scope  = tracer->GetInstrumentationScope();
    auto attrs  = scope.GetAttributes();
    ASSERT_EQ(attrs.size(), 1);
    auto attr = attrs.find("accept_single_attr");
    ASSERT_FALSE(attr == attrs.end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<bool>(attr->second));
    EXPECT_EQ(opentelemetry::nostd::get<bool>(attr->second), true);
  }

  std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> attr4 = {
      "accept_single_attr", true};
  auto t4 = tp.GetTracer("name4", "version4", "url4", {attr4});
  ASSERT_NE(nullptr, t4);
  {
    auto tracer = static_cast<opentelemetry::sdk::trace::Tracer *>(t4.get());
    auto scope  = tracer->GetInstrumentationScope();
    auto attrs  = scope.GetAttributes();
    ASSERT_EQ(attrs.size(), 1);
    auto attr = attrs.find("accept_single_attr");
    ASSERT_FALSE(attr == attrs.end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<bool>(attr->second));
    EXPECT_EQ(opentelemetry::nostd::get<bool>(attr->second), true);
  }

  auto t5 = tp.GetTracer("name5", "version5", "url5", {{"foo", "1"}, {"bar", "2"}});
  ASSERT_NE(nullptr, t5);
  {
    auto tracer = static_cast<opentelemetry::sdk::trace::Tracer *>(t5.get());
    auto scope  = tracer->GetInstrumentationScope();
    auto attrs  = scope.GetAttributes();
    ASSERT_EQ(attrs.size(), 2);
    auto attr = attrs.find("bar");
    ASSERT_FALSE(attr == attrs.end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::string>(attr->second));
    EXPECT_EQ(opentelemetry::nostd::get<std::string>(attr->second), "2");
  }

  std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>
      attrs6 = {{"foo", "1"}, {"bar", 42}};

  auto t6 = tp.GetTracer("name6", "version6", "url6", attrs6);
  ASSERT_NE(nullptr, t6);
  {
    auto tracer = static_cast<opentelemetry::sdk::trace::Tracer *>(t6.get());
    auto scope  = tracer->GetInstrumentationScope();
    auto attrs  = scope.GetAttributes();
    ASSERT_EQ(attrs.size(), 2);
    auto attr = attrs.find("bar");
    ASSERT_FALSE(attr == attrs.end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<int>(attr->second));
    EXPECT_EQ(opentelemetry::nostd::get<int>(attr->second), 42);
  }

  typedef std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> KV;

  std::initializer_list<KV> attrs7 = {{"foo", 3.14}, {"bar", "2"}};
  auto t7                          = tp.GetTracer("name7", "version7", "url7", attrs7);
  ASSERT_NE(nullptr, t7);
  {
    auto tracer = static_cast<opentelemetry::sdk::trace::Tracer *>(t7.get());
    auto scope  = tracer->GetInstrumentationScope();
    auto attrs  = scope.GetAttributes();
    ASSERT_EQ(attrs.size(), 2);
    auto attr = attrs.find("foo");
    ASSERT_FALSE(attr == attrs.end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<double>(attr->second));
    EXPECT_EQ(opentelemetry::nostd::get<double>(attr->second), 3.14);
  }

  auto t8 = tp.GetTracer("name8", "version8", "url8",
                         {{"a", "string"},
                          {"b", false},
                          {"c", 314159},
                          {"d", static_cast<unsigned int>(314159)},
                          {"e", static_cast<int32_t>(-20)},
                          {"f", static_cast<uint32_t>(20)},
                          {"g", static_cast<int64_t>(-20)},
                          {"h", static_cast<uint64_t>(20)},
                          {"i", 3.1},
                          {"j", "string"}});
  ASSERT_NE(nullptr, t8);
  {
    auto tracer = static_cast<opentelemetry::sdk::trace::Tracer *>(t8.get());
    auto scope  = tracer->GetInstrumentationScope();
    auto attrs  = scope.GetAttributes();
    ASSERT_EQ(attrs.size(), 10);
    auto attr = attrs.find("e");
    ASSERT_FALSE(attr == attrs.end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<int32_t>(attr->second));
    EXPECT_EQ(opentelemetry::nostd::get<int32_t>(attr->second), -20);
  }

  std::map<std::string, opentelemetry::common::AttributeValue> attr9{
      {"a", "string"},
      {"b", false},
      {"c", 314159},
      {"d", static_cast<unsigned int>(314159)},
      {"e", static_cast<int32_t>(-20)},
      {"f", static_cast<uint32_t>(20)},
      {"g", static_cast<int64_t>(-20)},
      {"h", static_cast<uint64_t>(20)},
      {"i", 3.1},
      {"j", "string"}};

  auto t9 = tp.GetTracer("name9", "version9", "url9", attr9);
  ASSERT_NE(nullptr, t9);
  {
    auto tracer = static_cast<opentelemetry::sdk::trace::Tracer *>(t9.get());
    auto scope  = tracer->GetInstrumentationScope();
    auto attrs  = scope.GetAttributes();
    ASSERT_EQ(attrs.size(), 10);
    auto attr = attrs.find("h");
    ASSERT_FALSE(attr == attrs.end());
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<uint64_t>(attr->second));
    EXPECT_EQ(opentelemetry::nostd::get<uint64_t>(attr->second), 20);
  }

  // cleanup properly without crash
  tp.ForceFlush();
  tp.Shutdown();
}
#endif /* OPENTELEMETRY_ABI_VERSION_NO >= 2 */

TEST(TracerProvider, Shutdown)
{
  std::unique_ptr<SpanProcessor> processor(new SimpleSpanProcessor(nullptr));
  std::vector<std::unique_ptr<SpanProcessor>> processors;
  processors.push_back(std::move(processor));

  std::unique_ptr<TracerContext> context1(new TracerContext(std::move(processors)));
  TracerProvider tp1(std::move(context1));

  EXPECT_TRUE(tp1.Shutdown());

  // It's safe to shutdown again
  EXPECT_TRUE(tp1.Shutdown());
}

TEST(TracerProvider, ForceFlush)
{
  std::unique_ptr<SpanProcessor> processor1(new SimpleSpanProcessor(nullptr));

  TracerProvider tp1(std::move(processor1));

  EXPECT_TRUE(tp1.ForceFlush());
}
