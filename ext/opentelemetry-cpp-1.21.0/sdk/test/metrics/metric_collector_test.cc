// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common.h"
#include "gmock/gmock.h"

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/sync_instruments.h"  // IWYU pragma: keep
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_filter.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/state/metric_collector.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"

#if defined(__GNUC__) || defined(__clang__) || defined(__apple_build_version__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;
using namespace testing;

using M = std::map<std::string, std::string>;

namespace
{

MetricFilter::TestMetricFn AcceptAllTestMetricFn()
{
  return [](const InstrumentationScope &scope, nostd::string_view name, const InstrumentType &type,
            nostd::string_view unit) -> MetricFilter::MetricFilterResult {
    return MetricFilter::MetricFilterResult::kAccept;
  };
}
MetricFilter::TestMetricFn DropAllTestMetricFn()
{
  return [](const InstrumentationScope &scope, nostd::string_view name, const InstrumentType &type,
            nostd::string_view unit) -> MetricFilter::MetricFilterResult {
    return MetricFilter::MetricFilterResult::kDrop;
  };
}
MetricFilter::TestMetricFn AcceptPartialAllTestMetricFn()
{
  return [](const InstrumentationScope &scope, nostd::string_view name, const InstrumentType &type,
            nostd::string_view unit) -> MetricFilter::MetricFilterResult {
    return MetricFilter::MetricFilterResult::kAcceptPartial;
  };
}

MetricFilter::TestAttributesFn AcceptAllTestAttributesFn()
{
  return [](const InstrumentationScope &scope, nostd::string_view name, const InstrumentType &type,
            nostd::string_view unit,
            const PointAttributes &attributes) -> MetricFilter::AttributesFilterResult {
    return MetricFilter::AttributesFilterResult::kAccept;
  };
}
MetricFilter::TestAttributesFn DropAllTestAttributesFn()
{
  return [](const InstrumentationScope &scope, nostd::string_view name, const InstrumentType &type,
            nostd::string_view unit,
            const PointAttributes &attributes) -> MetricFilter::AttributesFilterResult {
    return MetricFilter::AttributesFilterResult::kDrop;
  };
}

}  // namespace

class testing::MetricCollectorTest : public Test
{
public:
  std::weak_ptr<MetricCollector> AddMetricReaderToMeterContext(
      std::shared_ptr<MeterContext> context,
      std::shared_ptr<MetricReader> reader,
      std::unique_ptr<MetricFilter> metric_filter = nullptr) noexcept
  {
    auto collector = std::shared_ptr<MetricCollector>{
        new MetricCollector(context.get(), std::move(reader), std::move(metric_filter))};
    context->collectors_.push_back(collector);
    return std::weak_ptr<MetricCollector>(collector);
  }
};

TEST_F(MetricCollectorTest, CollectWithMetricFilterTestMetricTest1)
{
  auto context = std::shared_ptr<MeterContext>(new MeterContext(ViewRegistryFactory::Create()));
  auto scope   = InstrumentationScope::Create("CollectWithMetricFilterTestMetricTest1");
  auto meter   = std::shared_ptr<Meter>(new Meter(context, std::move(scope)));
  context->AddMeter(meter);

  auto filter    = MetricFilter::Create(AcceptAllTestMetricFn(), DropAllTestAttributesFn());
  auto reader    = std::shared_ptr<MetricReader>(new MockMetricReader());
  auto collector = AddMetricReaderToMeterContext(context, reader, std::move(filter)).lock();

  auto instrument_1_name = "instrument_1";
  auto instrument_1      = meter->CreateUInt64Counter(instrument_1_name);

  auto instrument_2_name = "instrument_2";
  auto instrument_2      = meter->CreateUInt64Counter(instrument_2_name);
  M m_2                  = {{"stream", "1"}};
  instrument_2->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_2),
                    opentelemetry::context::Context{});

  auto instrument_3_name = "instrument_3";
  auto instrument_3      = meter->CreateUInt64Counter(instrument_3_name);
  for (int s = 1; s <= 10; ++s)
  {
    for (int i = 0; i < s; ++i)
    {
      M m_3 = {{"stream", std::to_string(s)}};
      instrument_3->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_3),
                        opentelemetry::context::Context{});
    }
  }

  auto resource_metrics = collector->Produce().points_;
  for (const ScopeMetrics &scope_metrics : resource_metrics.scope_metric_data_)
  {
    for (const MetricData &metric : scope_metrics.metric_data_)
    {
      auto instrument_name = metric.instrument_descriptor.name_;
      ASSERT_TRUE(instrument_name == instrument_2_name || instrument_name == instrument_3_name);
      if (instrument_name == instrument_2_name)
      {
        EXPECT_EQ(metric.point_data_attr_.size(), 1);
      }
      else if (instrument_name == instrument_3_name)
      {
        EXPECT_EQ(metric.point_data_attr_.size(), 10);
      }
    }
  }
}

TEST_F(MetricCollectorTest, CollectWithMetricFilterTestMetricTest2)
{
  auto context = std::shared_ptr<MeterContext>(new MeterContext(ViewRegistryFactory::Create()));
  auto scope   = InstrumentationScope::Create("CollectWithMetricFilterTestMetricTest2");
  auto meter   = std::shared_ptr<Meter>(new Meter(context, std::move(scope)));
  context->AddMeter(meter);

  auto filter    = MetricFilter::Create(DropAllTestMetricFn(), AcceptAllTestAttributesFn());
  auto reader    = std::shared_ptr<MetricReader>(new MockMetricReader());
  auto collector = AddMetricReaderToMeterContext(context, reader, std::move(filter)).lock();

  auto instrument_1_name = "instrument_1";
  auto instrument_1      = meter->CreateUInt64Counter(instrument_1_name);

  auto instrument_2_name = "instrument_2";
  auto instrument_2      = meter->CreateUInt64Counter(instrument_2_name);
  M m_2                  = {{"stream", "1"}};
  instrument_2->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_2),
                    opentelemetry::context::Context{});

  auto instrument_3_name = "instrument_3";
  auto instrument_3      = meter->CreateUInt64Counter(instrument_3_name);
  for (int s = 1; s <= 10; ++s)
  {
    for (int i = 0; i < s; ++i)
    {
      M m_3 = {{"stream", std::to_string(s)}};
      instrument_3->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_3),
                        opentelemetry::context::Context{});
    }
  }

  auto resource_metrics = collector->Produce().points_;
  EXPECT_EQ(resource_metrics.scope_metric_data_.size(), 0);
}

TEST_F(MetricCollectorTest, CollectWithMetricFilterTestMetricTest3)
{
  auto context = std::shared_ptr<MeterContext>(new MeterContext(ViewRegistryFactory::Create()));
  auto scope   = InstrumentationScope::Create("CollectWithMetricFilterTestMetricTest3");
  auto meter   = std::shared_ptr<Meter>(new Meter(context, std::move(scope)));
  context->AddMeter(meter);

  auto test_metric_fn = [](const InstrumentationScope &scope, nostd::string_view name,
                           const InstrumentType &type,
                           nostd::string_view unit) -> MetricFilter::MetricFilterResult {
    std::string name_copy = {name.begin(), name.end()};
    if (name_copy.find("_accept") != std::string::npos)
    {
      return MetricFilter::MetricFilterResult::kAccept;
    }
    return MetricFilter::MetricFilterResult::kDrop;
  };
  auto filter    = MetricFilter::Create(test_metric_fn, DropAllTestAttributesFn());
  auto reader    = std::shared_ptr<MetricReader>(new MockMetricReader());
  auto collector = AddMetricReaderToMeterContext(context, reader, std::move(filter)).lock();

  auto instrument_1_name = "instrument_1";
  auto instrument_1      = meter->CreateUInt64Counter(instrument_1_name);

  auto instrument_2_name = "instrument_2";
  auto instrument_2      = meter->CreateUInt64Counter(instrument_2_name);
  M m_2                  = {{"stream", "1"}};
  instrument_2->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_2),
                    opentelemetry::context::Context{});

  auto instrument_3_name = "instrument_3_accept";
  auto instrument_3      = meter->CreateUInt64Counter(instrument_3_name);
  for (int s = 1; s <= 10; ++s)
  {
    for (int i = 0; i < s; ++i)
    {
      M m_3 = {{"stream", std::to_string(s)}};
      instrument_3->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_3),
                        opentelemetry::context::Context{});
    }
  }

  auto resource_metrics = collector->Produce().points_;
  for (const ScopeMetrics &scope_metrics : resource_metrics.scope_metric_data_)
  {
    for (const MetricData &metric : scope_metrics.metric_data_)
    {
      auto instrument_name = metric.instrument_descriptor.name_;
      ASSERT_EQ(instrument_name, instrument_3_name);
      EXPECT_EQ(metric.point_data_attr_.size(), 10);
    }
  }
}

TEST_F(MetricCollectorTest, CollectWithMetricFilterTestAttributesTest1)
{
  auto context = std::shared_ptr<MeterContext>(new MeterContext(ViewRegistryFactory::Create()));
  auto scope   = InstrumentationScope::Create("CollectWithMetricFilterTestAttributesTest1");
  auto meter   = std::shared_ptr<Meter>(new Meter(context, std::move(scope)));
  context->AddMeter(meter);

  auto test_attributes_fn =
      [](const InstrumentationScope &scope, nostd::string_view name, const InstrumentType &type,
         nostd::string_view unit,
         const PointAttributes &attributes) -> MetricFilter::AttributesFilterResult {
    if (attributes.GetAttributes().find("stream") != attributes.GetAttributes().end())
    {
      return MetricFilter::AttributesFilterResult::kAccept;
    }
    return MetricFilter::AttributesFilterResult::kDrop;
  };
  auto filter    = MetricFilter::Create(AcceptPartialAllTestMetricFn(), test_attributes_fn);
  auto reader    = std::shared_ptr<MetricReader>(new MockMetricReader());
  auto collector = AddMetricReaderToMeterContext(context, reader, std::move(filter)).lock();

  auto instrument_1_name = "instrument_1";
  auto instrument_1      = meter->CreateUInt64Counter(instrument_1_name);
  M m_1                  = {{"ocean", "1"}};
  instrument_1->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_1),
                    opentelemetry::context::Context{});

  auto instrument_2_name = "instrument_2";
  auto instrument_2      = meter->CreateUInt64Counter(instrument_2_name);
  M m_2                  = {{"stream", "1"}, {"river", "10"}};
  instrument_2->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_2),
                    opentelemetry::context::Context{});

  auto instrument_3_name = "instrument_3";
  auto instrument_3      = meter->CreateUInt64Counter(instrument_3_name);
  for (int s = 1; s <= 10; ++s)
  {
    for (int i = 0; i < s; ++i)
    {
      M m_3 = {{"stream", std::to_string(s)}, {"river", std::to_string(s * 10)}};
      instrument_3->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_3),
                        opentelemetry::context::Context{});
    }
  }

  auto resource_metrics = collector->Produce().points_;
  for (const ScopeMetrics &scope_metrics : resource_metrics.scope_metric_data_)
  {
    for (const MetricData &metric : scope_metrics.metric_data_)
    {
      auto instrument_name = metric.instrument_descriptor.name_;
      ASSERT_TRUE(instrument_name == instrument_2_name || instrument_name == instrument_3_name);
      if (instrument_name == instrument_2_name)
      {
        EXPECT_EQ(metric.point_data_attr_.size(), 1);
      }
      else if (instrument_name == instrument_3_name)
      {
        EXPECT_EQ(metric.point_data_attr_.size(), 10);
        for (const PointDataAttributes &pda : metric.point_data_attr_)
        {
          auto sum_point_data = nostd::get<SumPointData>(pda.point_data);
          auto value          = nostd::get<int64_t>(sum_point_data.value_);
          auto stream =
              std::stoi(nostd::get<std::string>(pda.attributes.GetAttributes().at("stream")));
          auto river =
              std::stoi(nostd::get<std::string>(pda.attributes.GetAttributes().at("river")));
          std::vector<int64_t> stream_v = {value, stream};
          std::vector<int64_t> river_v  = {value, river};
          EXPECT_THAT(stream_v, AnyOf(ElementsAre(1, 1), ElementsAre(2, 2), ElementsAre(3, 3),
                                      ElementsAre(4, 4), ElementsAre(5, 5), ElementsAre(6, 6),
                                      ElementsAre(7, 7), ElementsAre(8, 8), ElementsAre(9, 9),
                                      ElementsAre(10, 10)));
          EXPECT_THAT(river_v, AnyOf(ElementsAre(1, 10), ElementsAre(2, 20), ElementsAre(3, 30),
                                     ElementsAre(4, 40), ElementsAre(5, 50), ElementsAre(6, 60),
                                     ElementsAre(7, 70), ElementsAre(8, 80), ElementsAre(9, 90),
                                     ElementsAre(10, 100)));
        }
      }
    }
  }
}

TEST_F(MetricCollectorTest, CollectWithMetricFilterTestAttributesTest2)
{
  auto context = std::shared_ptr<MeterContext>(new MeterContext(ViewRegistryFactory::Create()));
  auto scope   = InstrumentationScope::Create("CollectWithMetricFilterTestAttributesTest2");
  auto meter   = std::shared_ptr<Meter>(new Meter(context, std::move(scope)));
  context->AddMeter(meter);

  auto test_attributes_fn =
      [](const InstrumentationScope &scope, nostd::string_view name, const InstrumentType &type,
         nostd::string_view unit,
         const PointAttributes &attributes) -> MetricFilter::AttributesFilterResult {
    if (attributes.GetAttributes().find("stream") != attributes.GetAttributes().end())
    {
      auto stream = nostd::get<std::string>(attributes.GetAttributes().at("stream"));
      if (std::stoi(stream) >= 4 && std::stoi(stream) <= 6)
      {
        return MetricFilter::AttributesFilterResult::kAccept;
      }
    }
    return MetricFilter::AttributesFilterResult::kDrop;
  };
  auto filter    = MetricFilter::Create(AcceptPartialAllTestMetricFn(), test_attributes_fn);
  auto reader    = std::shared_ptr<MetricReader>(new MockMetricReader());
  auto collector = AddMetricReaderToMeterContext(context, reader, std::move(filter)).lock();

  auto instrument_1_name = "instrument_1";
  auto instrument_1      = meter->CreateUInt64Counter(instrument_1_name);
  M m_1                  = {{"ocean", "1"}};
  instrument_1->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_1),
                    opentelemetry::context::Context{});

  auto instrument_2_name = "instrument_2";
  auto instrument_2      = meter->CreateUInt64Counter(instrument_2_name);
  M m_2                  = {{"stream", "1"}, {"river", "10"}};
  instrument_2->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_2),
                    opentelemetry::context::Context{});

  auto instrument_3_name = "instrument_3";
  auto instrument_3      = meter->CreateUInt64Counter(instrument_3_name);
  for (int s = 1; s <= 10; ++s)
  {
    for (int i = 0; i < s; ++i)
    {
      M m_3 = {{"stream", std::to_string(s)}, {"river", std::to_string(s * 10)}};
      instrument_3->Add(1, opentelemetry::common::KeyValueIterableView<M>(m_3),
                        opentelemetry::context::Context{});
    }
  }

  auto resource_metrics = collector->Produce().points_;
  for (const ScopeMetrics &scope_metrics : resource_metrics.scope_metric_data_)
  {
    for (const MetricData &metric : scope_metrics.metric_data_)
    {
      auto instrument_name = metric.instrument_descriptor.name_;
      ASSERT_EQ(instrument_name, instrument_3_name);
      EXPECT_EQ(metric.point_data_attr_.size(), 3);
      for (const PointDataAttributes &pda : metric.point_data_attr_)
      {
        auto sum_point_data = nostd::get<SumPointData>(pda.point_data);
        auto value          = nostd::get<int64_t>(sum_point_data.value_);
        auto stream =
            std::stoi(nostd::get<std::string>(pda.attributes.GetAttributes().at("stream")));
        auto river = std::stoi(nostd::get<std::string>(pda.attributes.GetAttributes().at("river")));
        std::vector<int64_t> stream_v = {value, stream};
        std::vector<int64_t> river_v  = {value, river};
        EXPECT_THAT(stream_v, AnyOf(ElementsAre(4, 4), ElementsAre(5, 5), ElementsAre(6, 6)));
        EXPECT_THAT(river_v, AnyOf(ElementsAre(4, 40), ElementsAre(5, 50), ElementsAre(6, 60)));
      }
    }
  }
}

#if defined(__GNUC__) || defined(__clang__) || defined(__apple_build_version__)
#  pragma GCC diagnostic pop
#endif
