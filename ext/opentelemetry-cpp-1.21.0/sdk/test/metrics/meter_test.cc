// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "common.h"

#include <functional>
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_config.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/resource/resource.h"

#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>

#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/metrics/sync_instruments.h"  // IWYU pragma: keep
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::sdk::common::internal_log;

namespace
{
nostd::shared_ptr<metrics::Meter> InitMeter(MetricReader **metricReaderPtr,
                                            const std::string &meter_name = "meter_name")
{
  static std::shared_ptr<metrics::MeterProvider> provider(new MeterProvider());
  std::unique_ptr<MetricReader> metric_reader(new MockMetricReader());
  *metricReaderPtr = metric_reader.get();
  auto p           = std::static_pointer_cast<MeterProvider>(provider);
  p->AddMetricReader(std::move(metric_reader));
  auto meter = provider->GetMeter(meter_name);
  return meter;
}

void asyc_generate_measurements_long(opentelemetry::metrics::ObserverResult observer,
                                     void * /* state */)
{
  auto observer_long =
      nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(observer);
  observer_long->Observe(10);
}

void asyc_generate_measurements_double(opentelemetry::metrics::ObserverResult observer,
                                       void * /* state */)
{
  auto observer_double =
      nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer);
  observer_double->Observe(10.2f);
}

std::shared_ptr<metrics::MeterProvider> GetMeterProviderWithScopeConfigurator(
    const ScopeConfigurator<MeterConfig> &meter_configurator,
    MetricReader **metric_reader_ptr)
{
  auto views    = ViewRegistryFactory::Create();
  auto resource = sdk::resource::Resource::Create({});
  std::unique_ptr<MetricReader> metric_reader(new MockMetricReader());
  *metric_reader_ptr = metric_reader.get();
  std::shared_ptr<metrics::MeterProvider> provider(
      new MeterProvider(std::move(views), resource,
                        std::make_unique<ScopeConfigurator<MeterConfig>>(meter_configurator)));
  auto p = std::static_pointer_cast<MeterProvider>(provider);
  p->AddMetricReader(std::move(metric_reader));
  return p;
}

class TestLogHandler : public LogHandler
{
public:
  void Handle(LogLevel level,
              const char * /*file*/,
              int /*line*/,
              const char *msg,
              const sdk::common::AttributeMap & /*attributes*/) noexcept override

  {
    if (LogLevel::Warning == level)
    {
      std::cout << msg << std::endl;
      warnings.push_back(msg);
    }
  }

  bool HasNameCaseConflictWarning() const
  {
    return std::any_of(warnings.begin(), warnings.end(), [](const std::string &warning) {
      return warning.find("WarnOnNameCaseConflict") != std::string::npos;
    });
  }

  bool HasDuplicateInstrumentWarning() const
  {
    return std::any_of(warnings.begin(), warnings.end(), [](const std::string &warning) {
      return warning.find("WarnOnDuplicateInstrument") != std::string::npos;
    });
  }

private:
  std::vector<std::string> warnings;
};

class MeterCreateInstrumentTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    ASSERT_TRUE(log_handler_ != nullptr);
    ASSERT_TRUE(metric_reader_ptr_ != nullptr);
    ASSERT_TRUE(provider_ != nullptr);
    GlobalLogHandler::SetLogHandler(std::static_pointer_cast<LogHandler>(log_handler_));
    GlobalLogHandler::SetLogLevel(LogLevel::Warning);

    provider_->AddMetricReader(metric_reader_ptr_);
    meter_ = provider_->GetMeter("test_meter");
    ASSERT_TRUE(meter_ != nullptr);
  }

  void TearDown() override {}

  void AddNameCorrectionView(const std::string &name,
                             const std::string &unit,
                             InstrumentType type,
                             const std::string &new_name)
  {
    std::unique_ptr<View> corrective_view{new View(new_name)};
    std::unique_ptr<InstrumentSelector> instrument_selector{
        new InstrumentSelector(type, name, unit)};

    std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("test_meter", "", "")};

    provider_->AddView(std::move(instrument_selector), std::move(meter_selector),
                       std::move(corrective_view));
  }

  void AddDescriptionCorrectionView(const std::string &name,
                                    const std::string &unit,
                                    InstrumentType type,
                                    const std::string &new_description)
  {
    std::unique_ptr<View> corrective_view{new View(name, new_description)};
    std::unique_ptr<InstrumentSelector> instrument_selector{
        new InstrumentSelector(type, name, unit)};

    std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("test_meter", "", "")};

    provider_->AddView(std::move(instrument_selector), std::move(meter_selector),
                       std::move(corrective_view));
  }

  std::shared_ptr<sdk::metrics::MeterProvider> provider_{new sdk::metrics::MeterProvider()};
  std::shared_ptr<TestLogHandler> log_handler_{new TestLogHandler()};
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter_{nullptr};

  std::shared_ptr<MetricReader> metric_reader_ptr_{new MockMetricReader()};
};

}  // namespace

TEST(MeterTest, BasicAsyncTests)
{
  MetricReader *metric_reader_ptr = nullptr;
  auto meter                      = InitMeter(&metric_reader_ptr);
  auto observable_counter         = meter->CreateInt64ObservableCounter("observable_counter");
  observable_counter->AddCallback(asyc_generate_measurements_long, nullptr);

  size_t count = 0;
  metric_reader_ptr->Collect([&count](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    if (metric_data.scope_metric_data_.size())
    {
      EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
      if (metric_data.scope_metric_data_.size())
      {
        count += metric_data.scope_metric_data_[0].metric_data_.size();
        EXPECT_EQ(count, 1);
      }
    }
    return true;
  });
  observable_counter->RemoveCallback(asyc_generate_measurements_long, nullptr);
}

constexpr static unsigned MAX_THREADS       = 25;
constexpr static unsigned MAX_ITERATIONS_MT = 1000;

TEST(MeterTest, StressMultiThread)
{
  MetricReader *metric_reader_ptr = nullptr;
  auto meter                      = InitMeter(&metric_reader_ptr, "stress_test_meter");
  std::atomic<unsigned> threadCount(0);
  std::atomic<size_t> numIterations(MAX_ITERATIONS_MT);
  std::atomic<bool> do_collect{false}, do_sync_create{true}, do_async_create{false};
  std::vector<nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>>
      observable_instruments;
  std::vector<std::thread> meter_operation_threads;
  std::atomic<size_t> instrument_id(0);
  while (numIterations--)
  {
    for (size_t i = 0; i < MAX_THREADS; i++)
    {
      if (threadCount++ < MAX_THREADS)
      {
        auto t = std::thread([&]() {
          std::this_thread::yield();
          if (do_sync_create.exchange(false))
          {
            std::string instrument_name = "test_couter_" + std::to_string(instrument_id);
            meter->CreateUInt64Counter(instrument_name, "", "");
            do_async_create.store(true);
            instrument_id++;
          }
          if (do_async_create.exchange(false))
          {
            std::cout << "\n creating async thread " << std::to_string(numIterations);
            auto observable_instrument = meter->CreateInt64ObservableUpDownCounter(
                "test_gauge_" + std::to_string(instrument_id));
            observable_instrument->AddCallback(asyc_generate_measurements_long, nullptr);
            observable_instruments.push_back(std::move(observable_instrument));
            do_collect.store(true);
            instrument_id++;
          }
          if (do_collect.exchange(false))
          {
            metric_reader_ptr->Collect([](ResourceMetrics & /* metric_data */) { return true; });
            do_sync_create.store(true);
          }
        });
        meter_operation_threads.push_back(std::move(t));
      }
    }
  }
  for (auto &t : meter_operation_threads)
  {
    if (t.joinable())
    {
      t.join();
    }
  }
}

TEST(MeterTest, MeterWithDisabledConfig)
{
  ScopeConfigurator<MeterConfig> disable_all_scopes =
      ScopeConfigurator<MeterConfig>::Builder(MeterConfig::Disabled()).Build();
  MetricReader *metric_reader_ptr = nullptr;
  std::shared_ptr<metrics::MeterProvider> meter_provider =
      GetMeterProviderWithScopeConfigurator(disable_all_scopes, &metric_reader_ptr);

  auto meter = meter_provider->GetMeter("foo", "0.1.0", "https://opentelemetry.io/schemas/1.24.0");

  // Test all supported instruments from this meter - create instruments
  auto double_counter         = meter->CreateDoubleCounter("double_counter");
  auto double_histogram       = meter->CreateDoubleHistogram("double_histogram");
  auto double_up_down_counter = meter->CreateDoubleUpDownCounter("double_up_down_counter");
  auto double_obs_counter     = meter->CreateDoubleObservableCounter("double_obs_counter");
  auto double_obs_gauge       = meter->CreateDoubleObservableGauge("double_obs_gauge");
  auto double_obs_up_down_counter =
      meter->CreateDoubleObservableUpDownCounter("double_obs_up_down_counter");

  auto uint64_counter        = meter->CreateUInt64Counter("uint64_counter");
  auto uint64_histogram      = meter->CreateUInt64Histogram("uint64_histogram");
  auto int64_up_down_counter = meter->CreateInt64UpDownCounter("int64_up_down_counter");
  auto int64_obs_counter     = meter->CreateInt64ObservableCounter("int64_obs_counter");
  auto int64_obs_gauge       = meter->CreateInt64ObservableGauge("int64_obs_gauge");
  auto int64_obs_up_down_counter =
      meter->CreateInt64ObservableUpDownCounter("int64_obs_up_down_counter");

  // Invoke the created instruments
  double_counter->Add(1.0f);
  double_histogram->Record(23.2f, {});
  double_up_down_counter->Add(-2.4f);
  double_obs_counter->AddCallback(asyc_generate_measurements_double, nullptr);
  double_obs_gauge->AddCallback(asyc_generate_measurements_double, nullptr);
  double_obs_up_down_counter->AddCallback(asyc_generate_measurements_double, nullptr);

  uint64_counter->Add(1);
  uint64_histogram->Record(23, {});
  int64_up_down_counter->Add(-2);
  int64_obs_counter->AddCallback(asyc_generate_measurements_long, nullptr);
  int64_obs_gauge->AddCallback(asyc_generate_measurements_long, nullptr);
  int64_obs_up_down_counter->AddCallback(asyc_generate_measurements_long, nullptr);

  // No active instruments are expected - since all scopes are disabled.
  metric_reader_ptr->Collect([&](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 0);
    return true;
  });
}

TEST(MeterTest, MeterWithEnabledConfig)
{
  ScopeConfigurator<MeterConfig> enable_all_scopes =
      ScopeConfigurator<MeterConfig>::Builder(MeterConfig::Enabled()).Build();
  MetricReader *metric_reader_ptr = nullptr;
  std::shared_ptr<metrics::MeterProvider> meter_provider =
      GetMeterProviderWithScopeConfigurator(enable_all_scopes, &metric_reader_ptr);

  auto meter = meter_provider->GetMeter("foo", "0.1.0", "https://opentelemetry.io/schemas/1.24.0");

  // Test all supported instruments from this meter - create instruments
  auto double_counter         = meter->CreateDoubleCounter("double_counter");
  auto double_histogram       = meter->CreateDoubleHistogram("double_histogram");
  auto double_up_down_counter = meter->CreateDoubleUpDownCounter("double_up_down_counter");
  auto double_obs_counter     = meter->CreateDoubleObservableCounter("double_obs_counter");
  auto double_obs_gauge       = meter->CreateDoubleObservableGauge("double_obs_gauge");
  auto double_obs_up_down_counter =
      meter->CreateDoubleObservableUpDownCounter("double_obs_up_down_counter");

  auto uint64_counter        = meter->CreateUInt64Counter("uint64_counter");
  auto uint64_histogram      = meter->CreateUInt64Histogram("uint64_histogram");
  auto int64_up_down_counter = meter->CreateInt64UpDownCounter("int64_up_down_counter");
  auto int64_obs_counter     = meter->CreateInt64ObservableCounter("int64_obs_counter");
  auto int64_obs_gauge       = meter->CreateInt64ObservableGauge("int64_obs_gauge");
  auto int64_obs_up_down_counter =
      meter->CreateInt64ObservableUpDownCounter("int64_obs_up_down_counter");

  // Invoke the created instruments
  double_counter->Add(1.0f);
  double_histogram->Record(23.2f, {});
  double_up_down_counter->Add(-2.4f);
  double_obs_counter->AddCallback(asyc_generate_measurements_double, nullptr);
  double_obs_gauge->AddCallback(asyc_generate_measurements_double, nullptr);
  double_obs_up_down_counter->AddCallback(asyc_generate_measurements_double, nullptr);

  uint64_counter->Add(1);
  uint64_histogram->Record(23, {});
  int64_up_down_counter->Add(-2);
  int64_obs_counter->AddCallback(asyc_generate_measurements_long, nullptr);
  int64_obs_gauge->AddCallback(asyc_generate_measurements_long, nullptr);
  int64_obs_up_down_counter->AddCallback(asyc_generate_measurements_long, nullptr);

  // Expected active instruments
  std::vector<std::pair<std::string, std::string>> active_scope_instrument_pairs;
  active_scope_instrument_pairs.emplace_back("foo", "double_counter");
  active_scope_instrument_pairs.emplace_back("foo", "double_histogram");
  active_scope_instrument_pairs.emplace_back("foo", "double_up_down_counter");
  active_scope_instrument_pairs.emplace_back("foo", "double_obs_up_down_counter");
  active_scope_instrument_pairs.emplace_back("foo", "double_obs_counter");
  active_scope_instrument_pairs.emplace_back("foo", "double_obs_gauge");
  active_scope_instrument_pairs.emplace_back("foo", "uint64_counter");
  active_scope_instrument_pairs.emplace_back("foo", "uint64_histogram");
  active_scope_instrument_pairs.emplace_back("foo", "int64_up_down_counter");
  active_scope_instrument_pairs.emplace_back("foo", "int64_obs_up_down_counter");
  active_scope_instrument_pairs.emplace_back("foo", "int64_obs_counter");
  active_scope_instrument_pairs.emplace_back("foo", "int64_obs_gauge");

  metric_reader_ptr->Collect([&](const ResourceMetrics &metric_data) {
    bool unexpected_instrument_found = false;
    std::string curr_scope_name      = metric_data.scope_metric_data_.at(0).scope_->GetName();
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_.at(0).scope_->GetName(), "foo");
    EXPECT_EQ(metric_data.scope_metric_data_.at(0).metric_data_.size(), 12);
    for (const MetricData &md : metric_data.scope_metric_data_.at(0).metric_data_)
    {
      auto found_instrument = std::make_pair(curr_scope_name, md.instrument_descriptor.name_);
      // confirm if the found instrument is expected
      auto it = std::find(active_scope_instrument_pairs.begin(),
                          active_scope_instrument_pairs.end(), found_instrument);
      if (it == active_scope_instrument_pairs.end())
      {
        // found instrument is not expected
        unexpected_instrument_found = true;
        break;
      }
    }
    EXPECT_FALSE(unexpected_instrument_found);
    return true;
  });
}

TEST(MeterTest, MeterWithCustomConfig)
{
  // within the same call
  auto check_if_version_present = [](const InstrumentationScope &scope_info) {
    return !scope_info.GetVersion().empty();
  };

  // custom scope configurator that only disables meters with name "foo_library" or do not have
  // version information
  ScopeConfigurator<MeterConfig> custom_scope_configurator =
      ScopeConfigurator<MeterConfig>::Builder(MeterConfig::Disabled())
          .AddConditionNameEquals("foo_library", MeterConfig::Disabled())
          .AddCondition(check_if_version_present, MeterConfig::Enabled())
          .Build();
  MetricReader *metric_reader_ptr = nullptr;
  std::shared_ptr<metrics::MeterProvider> meter_provider =
      GetMeterProviderWithScopeConfigurator(custom_scope_configurator, &metric_reader_ptr);

  // The meter has version information and name is not "foo_library".
  // All instruments from this meter should be active and recording metrics.
  auto meter_enabled_expected_bar =
      meter_provider->GetMeter("bar_library", "0.1.0", "https://opentelemetry.io/schemas/1.24.0");

  // The meter has version information and name is "foo_library".
  // All instruments from this meter should be noop.
  auto meter_disabled_expected_foo =
      meter_provider->GetMeter("foo_library", "0.1.0", "https://opentelemetry.io/schemas/1.24.0");

  // This meter has no version information.
  // All instruments from this meter should be noop.
  auto meter_disabled_expected_baz =
      meter_provider->GetMeter("baz_library", "", "https://opentelemetry.io/schemas/1.24.0");

  // Create instruments from all meters
  auto double_counter_bar = meter_enabled_expected_bar->CreateDoubleCounter("double_counter");
  auto double_counter_foo = meter_disabled_expected_foo->CreateDoubleCounter("double_counter");
  auto double_counter_baz = meter_disabled_expected_baz->CreateDoubleCounter("double_counter");

  // Invoke created instruments at least once
  double_counter_bar->Add(1.0f);
  double_counter_foo->Add(1.0f);
  double_counter_baz->Add(1.0f);

  std::vector<std::pair<std::string, std::string>> active_scope_instrument_pairs;
  active_scope_instrument_pairs.emplace_back("bar_library", "double_counter");

  metric_reader_ptr->Collect([&](const ResourceMetrics &metric_data) {
    int found_instruments            = 0;
    bool unexpected_instrument_found = false;
    for (const ScopeMetrics &sm : metric_data.scope_metric_data_)
    {
      std::string curr_scope = sm.scope_->GetName();
      for (const MetricData &md : sm.metric_data_)
      {
        found_instruments++;
        auto found_instrument = std::make_pair(curr_scope, md.instrument_descriptor.name_);
        // confirm if the found instrument is expected
        auto it = std::find(active_scope_instrument_pairs.begin(),
                            active_scope_instrument_pairs.end(), found_instrument);
        if (it == active_scope_instrument_pairs.end())
        {
          // found instrument is not expected
          unexpected_instrument_found = true;
          break;
        }
      }
    }
    EXPECT_EQ(found_instruments, active_scope_instrument_pairs.size());
    EXPECT_FALSE(unexpected_instrument_found);
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, IdenticalSyncInstruments)
{
  auto counter1 = meter_->CreateDoubleCounter("my_counter", "desc", "unit");
  auto counter2 = meter_->CreateDoubleCounter("my_counter", "desc", "unit");

  counter1->Add(1.0, {{"key", "value1"}});
  counter2->Add(2.5, {{"key", "value2"}});

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 2);
    auto &point_data1 =
        metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_[0].point_data;
    auto &point_data2 =
        metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_[1].point_data;

    auto sum_point_data1 = nostd::get<sdk::metrics::SumPointData>(point_data1);
    auto sum_point_data2 = nostd::get<sdk::metrics::SumPointData>(point_data2);

    const double sum =
        nostd::get<double>(sum_point_data1.value_) + nostd::get<double>(sum_point_data2.value_);
    EXPECT_DOUBLE_EQ(sum, 3.5);
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, NameCaseConflictSyncInstruments)
{
  auto counter1 = meter_->CreateUInt64Counter("My_CountER", "desc", "unit");
  auto counter2 = meter_->CreateUInt64Counter("my_counter", "desc", "unit");

  counter1->Add(1);
  counter2->Add(2);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    auto &point_data =
        metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_[0].point_data;
    auto sum_point_data = nostd::get<sdk::metrics::SumPointData>(point_data);
    const auto sum      = nostd::get<int64_t>(sum_point_data.value_);
    EXPECT_EQ(sum, 3);
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_TRUE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, ViewCorrectedNameCaseConflictSyncInstruments)
{
  InstrumentDescriptor descriptor{"My_CountER", "desc", "unit", InstrumentType::kCounter,
                                  InstrumentValueType::kLong};

  AddNameCorrectionView(descriptor.name_, descriptor.unit_, descriptor.type_, "my_counter");

  auto counter1 =
      meter_->CreateUInt64Counter("My_CountER", descriptor.description_, descriptor.unit_);
  auto counter2 =
      meter_->CreateUInt64Counter("my_counter", descriptor.description_, descriptor.unit_);

  counter1->Add(1);
  counter2->Add(2);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    auto &point_data =
        metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_[0].point_data;
    auto sum_point_data = nostd::get<sdk::metrics::SumPointData>(point_data);
    const auto sum      = nostd::get<int64_t>(sum_point_data.value_);
    EXPECT_EQ(sum, 3);
    // no warnings expected after correction with the view
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, DuplicateSyncInstrumentsByKind)
{
  auto counter1 = meter_->CreateDoubleCounter("my_counter", "desc", "unit");
  auto counter2 = meter_->CreateUInt64Counter("my_counter", "desc", "unit");

  counter1->Add(1, {{"key", "value1"}});
  counter2->Add(1, {{"key", "value2"}});

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 2);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[1].point_data_attr_.size(), 1);
    EXPECT_TRUE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, DuplicateSyncInstrumentsByUnits)
{
  auto counter1 = meter_->CreateDoubleCounter("my_counter", "desc", "unit");
  auto counter2 = meter_->CreateDoubleCounter("my_counter", "desc", "another_unit");

  counter1->Add(1, {{"key", "value1"}});
  counter2->Add(1, {{"key", "value2"}});

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 2);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[1].point_data_attr_.size(), 1);
    EXPECT_TRUE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, DuplicateSyncInstrumentsByDescription)
{
  auto counter1 = meter_->CreateDoubleCounter("my_counter", "desc", "unit");
  auto counter2 = meter_->CreateDoubleCounter("my_counter", "another_desc", "unit");

  counter1->Add(1, {{"key", "value1"}});
  counter2->Add(1, {{"key", "value2"}});

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 2);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[1].point_data_attr_.size(), 1);
    EXPECT_TRUE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, ViewCorrectedDuplicateSyncInstrumentsByDescription)
{
  InstrumentDescriptor descriptor{"my_counter", "desc", "unit", InstrumentType::kCounter,
                                  InstrumentValueType::kDouble};
  AddDescriptionCorrectionView(descriptor.name_, descriptor.unit_, descriptor.type_,
                               descriptor.description_);

  auto counter1 = meter_->CreateDoubleCounter("my_counter", "desc", "unit");
  auto counter2 = meter_->CreateDoubleCounter("my_counter", "another_desc", "unit");

  counter1->Add(1, {{"key", "value1"}});
  counter2->Add(1, {{"key", "value2"}});

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    // only one metric_data object expected after correction with the view
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 2);
    // no warnings expected after correction with the view
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, IdenticalAsyncInstruments)
{
  auto observable_counter1 =
      meter_->CreateInt64ObservableCounter("observable_counter", "desc", "unit");
  auto observable_counter2 =
      meter_->CreateInt64ObservableCounter("observable_counter", "desc", "unit");

  auto callback1 = [](opentelemetry::metrics::ObserverResult observer, void * /* state */) {
    auto observer_long =
        nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(observer);
    observer_long->Observe(12, {{"key", "value1"}});
  };

  auto callback2 = [](opentelemetry::metrics::ObserverResult observer, void * /* state */) {
    auto observer_long =
        nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(observer);
    observer_long->Observe(2, {{"key", "value2"}});
  };

  observable_counter1->AddCallback(callback1, nullptr);
  observable_counter2->AddCallback(callback2, nullptr);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    auto &point_data_attr = metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_;
    EXPECT_EQ(point_data_attr.size(), 2);

    auto &point_data1 = point_data_attr[0].point_data;
    auto &point_data2 = point_data_attr[1].point_data;

    auto sum_point_data1 = nostd::get<sdk::metrics::SumPointData>(point_data1);
    auto sum_point_data2 = nostd::get<sdk::metrics::SumPointData>(point_data2);

    int64_t sum =
        nostd::get<int64_t>(sum_point_data1.value_) + nostd::get<int64_t>(sum_point_data2.value_);
    EXPECT_EQ(sum, 14);
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, NameCaseConflictAsyncInstruments)
{
  auto observable_counter1 =
      meter_->CreateDoubleObservableCounter("OBServable_CounTER", "desc", "unit");
  auto observable_counter2 =
      meter_->CreateDoubleObservableCounter("observable_counter", "desc", "unit");

  auto callback1 = [](opentelemetry::metrics::ObserverResult observer, void * /* state */) {
    auto observer_double =
        nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer);
    observer_double->Observe(22.22, {{"key", "value1"}});
  };

  auto callback2 = [](opentelemetry::metrics::ObserverResult observer, void * /* state */) {
    auto observer_double =
        nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer);
    observer_double->Observe(55.55, {{"key", "value2"}});
  };

  observable_counter1->AddCallback(callback1, nullptr);
  observable_counter2->AddCallback(callback2, nullptr);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    auto &point_data_attr = metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_;
    EXPECT_EQ(point_data_attr.size(), 2);

    auto &point_data1    = point_data_attr[0].point_data;
    auto &point_data2    = point_data_attr[1].point_data;
    auto sum_point_data1 = nostd::get<sdk::metrics::SumPointData>(point_data1);
    auto sum_point_data2 = nostd::get<sdk::metrics::SumPointData>(point_data2);

    const double sum =
        nostd::get<double>(sum_point_data1.value_) + nostd::get<double>(sum_point_data2.value_);
    EXPECT_DOUBLE_EQ(sum, 77.77);
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_TRUE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, ViewCorrectedNameCaseConflictAsyncInstruments)
{
  AddNameCorrectionView("OBServable_CounTER", "unit", InstrumentType::kObservableCounter,
                        "observable_counter");

  auto observable_counter1 =
      meter_->CreateDoubleObservableCounter("OBServable_CounTER", "desc", "unit");
  auto observable_counter2 =
      meter_->CreateDoubleObservableCounter("observable_counter", "desc", "unit");

  auto callback1 = [](opentelemetry::metrics::ObserverResult observer, void * /* state */) {
    auto observer_double =
        nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer);
    observer_double->Observe(22.22, {{"key", "value1"}});
  };

  auto callback2 = [](opentelemetry::metrics::ObserverResult observer, void * /* state */) {
    auto observer_double =
        nostd::get<nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer);
    observer_double->Observe(55.55, {{"key", "value2"}});
  };

  observable_counter1->AddCallback(callback1, nullptr);
  observable_counter2->AddCallback(callback2, nullptr);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    auto &point_data_attr = metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_;
    EXPECT_EQ(point_data_attr.size(), 2);

    auto &point_data1    = point_data_attr[0].point_data;
    auto &point_data2    = point_data_attr[1].point_data;
    auto sum_point_data1 = nostd::get<sdk::metrics::SumPointData>(point_data1);
    auto sum_point_data2 = nostd::get<sdk::metrics::SumPointData>(point_data2);

    const double sum =
        nostd::get<double>(sum_point_data1.value_) + nostd::get<double>(sum_point_data2.value_);
    EXPECT_DOUBLE_EQ(sum, 77.77);
    // no warnings expected after correction with the view
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, DuplicateAsyncInstrumentsByKind)
{
  auto observable_counter1 = meter_->CreateDoubleObservableCounter("observable_counter");
  auto observable_counter2 = meter_->CreateDoubleObservableGauge("observable_counter");

  observable_counter1->AddCallback(asyc_generate_measurements_double, nullptr);
  observable_counter2->AddCallback(asyc_generate_measurements_double, nullptr);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 2);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    EXPECT_TRUE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, DuplicateAsyncInstrumentsByUnits)
{
  auto observable_counter1 =
      meter_->CreateDoubleObservableCounter("observable_counter", "desc", "unit");
  auto observable_counter2 =
      meter_->CreateDoubleObservableCounter("observable_counter", "desc", "another_unit");

  observable_counter1->AddCallback(asyc_generate_measurements_double, nullptr);
  observable_counter2->AddCallback(asyc_generate_measurements_double, nullptr);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 2);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    EXPECT_TRUE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, DuplicateAsyncInstrumentsByDescription)
{
  auto observable_counter1 =
      meter_->CreateDoubleObservableCounter("observable_counter", "desc", "unit");
  auto observable_counter2 =
      meter_->CreateDoubleObservableCounter("observable_counter", "another_desc", "unit");

  observable_counter1->AddCallback(asyc_generate_measurements_double, nullptr);
  observable_counter2->AddCallback(asyc_generate_measurements_double, nullptr);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 2);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    EXPECT_TRUE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}

TEST_F(MeterCreateInstrumentTest, ViewCorrectedDuplicateAsyncInstrumentsByDescription)
{
  InstrumentDescriptor descriptor{"observable_counter", "desc", "unit",
                                  InstrumentType::kObservableCounter, InstrumentValueType::kDouble};

  AddDescriptionCorrectionView(descriptor.name_, descriptor.unit_, descriptor.type_,
                               descriptor.description_);

  auto observable_counter1 = meter_->CreateDoubleObservableCounter(
      descriptor.name_, descriptor.description_, descriptor.unit_);
  auto observable_counter2 =
      meter_->CreateDoubleObservableCounter(descriptor.name_, "another_desc", descriptor.unit_);

  observable_counter1->AddCallback(asyc_generate_measurements_double, nullptr);
  observable_counter2->AddCallback(asyc_generate_measurements_double, nullptr);

  metric_reader_ptr_->Collect([this](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_.size(), 1);
    EXPECT_EQ(metric_data.scope_metric_data_[0].metric_data_[0].point_data_attr_.size(), 1);
    // no warnings expected after correction with the view
    EXPECT_FALSE(log_handler_->HasDuplicateInstrumentWarning());
    EXPECT_FALSE(log_handler_->HasNameCaseConflictWarning());
    return true;
  });
}
