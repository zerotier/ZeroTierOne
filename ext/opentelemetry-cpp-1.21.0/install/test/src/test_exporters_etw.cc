// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef _WIN32

#  include <gtest/gtest.h>
#  include <map>
#  include <string>

#  include "opentelemetry/exporters/etw/etw_logger_exporter.h"
#  include "opentelemetry/exporters/etw/etw_tracer_exporter.h"
#  include "opentelemetry/sdk/trace/sampler.h"
#  include "opentelemetry/sdk/trace/simple_processor.h"

using namespace OPENTELEMETRY_NAMESPACE;

using namespace opentelemetry::exporter::etw;

TEST(ExportersEtwInstall, LoggerProvider)
{
  std::string providerName = "OpenTelemetry-ETW-TLD";
  exporter::etw::LoggerProvider lp;

  const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
  auto logger = lp.GetLogger(providerName, schema_url);
  ASSERT_TRUE(logger != nullptr);

  // Log attributes
  Properties attribs = {{"attrib1", 1}, {"attrib2", 2}};
  EXPECT_NO_THROW(logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug,
                                        opentelemetry::common::MakeAttributes(attribs)));
}

TEST(ExportersEtwInstall, TracerProvider)
{
  std::string providerName = "OpenTelemetry-ETW-TLD";
  exporter::etw::TracerProvider tp({{"enableTraceId", false},
                                    {"enableSpanId", false},
                                    {"enableActivityId", false},
                                    {"enableActivityTracking", true},
                                    {"enableRelatedActivityId", false},
                                    {"enableAutoParent", false}});
  auto tracer = tp.GetTracer(providerName);
  ASSERT_TRUE(tracer != nullptr);
  {
    auto aSpan  = tracer->StartSpan("A.min");
    auto aScope = tracer->WithActiveSpan(aSpan);
    {
      auto bSpan  = tracer->StartSpan("B.min");
      auto bScope = tracer->WithActiveSpan(bSpan);
      {
        auto cSpan  = tracer->StartSpan("C.min");
        auto cScope = tracer->WithActiveSpan(cSpan);
        EXPECT_NO_THROW(cSpan->End());
      }
      EXPECT_NO_THROW(bSpan->End());
    }
    EXPECT_NO_THROW(aSpan->End());
  }
#  if OPENTELEMETRY_ABI_VERSION_NO == 1
  EXPECT_NO_THROW(tracer->CloseWithMicroseconds(0));
#  endif
}

#endif  // _WIN32