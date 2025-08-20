// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "opentelemetry/sdk/metrics/state/observable_registry.h"

using namespace opentelemetry::sdk::metrics;

#if 0
class MeasurementFetcher
{
public:
  static void Fetcher1(opentelemetry::metrics::ObserverResult &observer_result,
                      void * /*state*/)
  {
    fetch_count1++;
    if (fetch_count1 == 1)
    {
      std::get<observer_result.Observe((int64_t)20, {{"RequestType", "GET"}});
      observer_result.Observe((int64_t)10, {{"RequestType", "PUT"}});
      number_of_get1 += (int64_t)20;
      number_of_put1 += (int64_t)10;
    }
    else if (fetch_count1 == 2)
    {
      observer_result.Observe((int64_t)40, {{"RequestType", "GET"}});
      observer_result.Observe((int64_t)20, {{"RequestType", "PUT"}});
      number_of_get1 += (int64_t)40;
      number_of_put1 += (int64_t)20;
    }
  }

  static void Fetcher2(opentelemetry::metrics::ObserverResult &observer_result,
                      void * /*state*/)
  {
    fetch_count2++;
    if (fetch_count2 == 1)
    {
      observer_result.Observe((int64_t)20, {{"RequestType", "GET"}});
      observer_result.Observe((int64_t)10, {{"RequestType", "PUT"}});
      number_of_get2 += (int64_t)20;
      number_of_put2 += (int64_t)10;
    }
    else if (fetch_count2 == 2)
    {
      observer_result.Observe((int64_t)40, {{"RequestType", "GET"}});
      observer_result.Observe((int64_t)20, {{"RequestType", "PUT"}});
      number_of_get2 += (int64_t)40;
      number_of_put2 += (int64_t)20;
    }
  }

  static void init_values()
  {
    fetch_count1 = fetch_count2   = 0;
    number_of_get1 = number_of_get2 = 0;
    number_of_put1 = number_of_put2 = 0;

  }

  static size_t fetch_count1, fetch_count2;
  static int64_t number_of_get1, number_of_get2;
  static int64_t number_of_put1, number_of_put2;
  static const size_t number_of_attributes = 2;  // GET , PUT
};

#endif

TEST(ObservableRegistry, BasicTests)
{
  ObservableRegistry registry;
  EXPECT_EQ(1, 1);
}
