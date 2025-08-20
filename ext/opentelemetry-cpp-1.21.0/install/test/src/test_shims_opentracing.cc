// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/opentracingshim/propagation.h>
#include <opentelemetry/opentracingshim/shim_utils.h>
#include <opentelemetry/opentracingshim/span_context_shim.h>
#include <opentelemetry/opentracingshim/span_shim.h>
#include <opentelemetry/opentracingshim/tracer_shim.h>

TEST(ShimsOpenTracingInstall, TracerShim)
{
  auto tracer_shim = opentelemetry::opentracingshim::TracerShim::createTracerShim();
  ASSERT_TRUE(tracer_shim != nullptr);
  auto span_shim = tracer_shim->StartSpan("test");
  ASSERT_TRUE(span_shim != nullptr);
  span_shim->Log({{"event", "test"}});
  span_shim->SetTag("test", "test");
  span_shim->SetBaggageItem("test", "test");
  span_shim->Finish();
  tracer_shim->Close();
}