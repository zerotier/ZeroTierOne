// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

class foo_library
{
public:
  static void counter_example(const std::string &name);
  static void histogram_example(const std::string &name);
  static void histogram_exp_example(const std::string &name);
  static void observable_counter_example(const std::string &name);
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  static void gauge_example(const std::string &name);
#endif
  static void semconv_counter_example();
  static void semconv_histogram_example();
  static void semconv_observable_counter_example();
};
