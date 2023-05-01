#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cassert>

#include "prometheus/collectable.h"
#include "prometheus/metric.h"
#include "prometheus/hash.h"

namespace prometheus {

  /// \brief A metric of type T with a set of labeled dimensions.
  ///
  /// One of Prometheus main feature is a multi-dimensional data model with time
  /// series data identified by metric name and key/value pairs, also known as
  /// labels. A time series is a series of data points indexed (or listed or
  /// graphed) in time order (https://en.wikipedia.org/wiki/Time_series).
  ///
  /// An instance of this class is exposed as multiple time series during
  /// scrape, i.e., one time series for each set of labels provided to Add().
  ///
  /// For example it is possible to collect data for a metric
  /// `http_requests_total`, with two time series:
  ///
  /// - all HTTP requests that used the method POST
  /// - all HTTP requests that used the method GET
  ///
  /// The metric name specifies the general feature of a system that is
  /// measured, e.g., `http_requests_total`. Labels enable Prometheus's
  /// dimensional data model: any given combination of labels for the same
  /// metric name identifies a particular dimensional instantiation of that
  /// metric. For example a label for 'all HTTP requests that used the method
  /// POST' can be assigned with `method= "POST"`.
  ///
  /// Given a metric name and a set of labels, time series are frequently
  /// identified using this notation:
  ///
  ///     <metric name> { < label name >= <label value>, ... }
  ///
  /// It is required to follow the syntax of metric names and labels given by:
  /// https://prometheus.io/docs/concepts/data_model/#metric-names-and-labels
  ///
  /// The following metric and label conventions are not required for using
  /// Prometheus, but can serve as both a style-guide and a collection of best
  /// practices: https://prometheus.io/docs/practices/naming/
  ///
  /// tparam T One of the metric types Counter, Gauge, Histogram or Summary.
  class Family : public Collectable {

    public:

      using Hash      = std::size_t;
      using Label     = std::pair<const std::string, const std::string>;
      using Labels    = std::map <const std::string, const std::string>;
      using MetricPtr = std::unique_ptr<Metric>;

      const   Metric::Type                 type;
      const   std::string                  name;
      const   std::string                  help;
      const   Labels                       constant_labels;
      mutable std::mutex                   mutex;

      std::unordered_map<Hash, MetricPtr>  metrics;
      std::unordered_map<Hash, Labels>     labels;
      std::unordered_map<Metric*, Hash>    labels_reverse_lookup;


      /// \brief Compute the hash value of a map of labels.
      ///
      /// \param labels The map that will be computed the hash value.
      ///
      /// \returns The hash value of the given labels.
      static Hash hash_labels (const Labels& labels) {
        size_t seed = 0;
        for (const Label& label : labels)
          detail::hash_combine (&seed, label.first, label.second);

        return seed;
      }

      static bool isLocaleIndependentDigit        (char c) { return '0' <= c && c <= '9'; }
      static bool isLocaleIndependentAlphaNumeric (char c) { return isLocaleIndependentDigit(c) || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'); }

      bool nameStartsValid (const std::string& name) {
        if (name.empty())                           return false; // must not be empty
        if (isLocaleIndependentDigit(name.front())) return false; // must not start with a digit
        if (name.compare(0, 2, "__") == 0)          return false; // must not start with "__"
        return true;
      }

      /// \brief Check if the metric name is valid
      ///
      /// The metric name regex is "[a-zA-Z_:][a-zA-Z0-9_:]*"
      ///
      /// \see https://prometheus.io/docs/concepts/data_model/
      ///
      /// \param name metric name
      /// \return true is valid, false otherwise
      bool CheckMetricName (const std::string& name) {

        if (!nameStartsValid(name))
          return false;

        for (const char& c : name)
          if ( !isLocaleIndependentAlphaNumeric(c) && c != '_' && c != ':' )
            return false;

        return true;

      }

      /// \brief Check if the label name is valid
      ///
      /// The label name regex is "[a-zA-Z_][a-zA-Z0-9_]*"
      ///
      /// \see https://prometheus.io/docs/concepts/data_model/
      ///
      /// \param name label name
      /// \return true is valid, false otherwise
      bool CheckLabelName (const std::string& name) {

        if (!nameStartsValid(name))
          return false;

        for (const char& c : name)
          if (!isLocaleIndependentAlphaNumeric(c) && c != '_')
            return false;

        return true;

      }

      /// \brief Create a new metric.
      ///
      /// Every metric is uniquely identified by its name and a set of key-value
      /// pairs, also known as labels. Prometheus's query language allows filtering
      /// and aggregation based on metric name and these labels.
      ///
      /// This example selects all time series that have the `http_requests_total`
      /// metric name:
      ///
      ///     http_requests_total
      ///
      /// It is possible to assign labels to the metric name. These labels are
      /// propagated to each dimensional data added with Add(). For example if a
      /// label `job= "prometheus"` is provided to this constructor, it is possible
      /// to filter this time series with Prometheus's query language by appending
      /// a set of labels to match in curly braces ({})
      ///
      ///     http_requests_total{job= "prometheus"}
      ///
      /// For further information see: [Quering Basics]
      /// (https://prometheus.io/docs/prometheus/latest/querying/basics/)
      ///
      /// \param name Set the metric name.
      /// \param help Set an additional description.
      /// \param constant_labels Assign a set of key-value pairs (= labels) to the
      /// metric. All these labels are propagated to each time series within the
      /// metric.
      /// \throw std::runtime_exception on invalid metric or label names.
      Family (Metric::Type type_, const std::string& name_, const std::string& help_, const Labels& constant_labels_)
      : type(type_), name(name_), help(help_), constant_labels(constant_labels_) {

        if (!CheckMetricName(name_))
          throw std::invalid_argument("Invalid metric name");

        for (const Label& label_pair : constant_labels) {
          const std::string& label_name = label_pair.first;
          if (!CheckLabelName(label_name))
            throw std::invalid_argument("Invalid label name");
        }

      }

      /// \brief Remove the given dimensional data.
      ///
      /// \param metric Dimensional data to be removed. The function does nothing,
      /// if the given metric was not returned by Add().
      void Remove (Metric* metric) {
        std::lock_guard<std::mutex> lock{ mutex };

        if (labels_reverse_lookup.count(metric) == 0)
          return;

        const Hash hash = labels_reverse_lookup.at(metric);
        metrics.erase(hash);
        labels.erase(hash);
        labels_reverse_lookup.erase(metric);

      }

      /// \brief Returns true if the dimensional data with the given labels exist
      ///
      /// \param labels A set of key-value pairs (= labels) of the dimensional data.
      bool Has (const Labels& labels) const {
        const Hash hash = hash_labels (labels);
        std::lock_guard<std::mutex> lock{ mutex };
        return metrics.find(hash) != metrics.end();
      }

      /// \brief Returns the name for this family.
      ///
      /// \return The family name.
      const std::string& GetName() const {
        return name;
      }

      /// \brief Returns the constant labels for this family.
      ///
      /// \return All constant labels as key-value pairs.
      const Labels& GetConstantLabels() const {
        return constant_labels;
      }

      /// \brief Returns the current value of each dimensional data.
      ///
      /// Collect is called by the Registry when collecting metrics.
      ///
      /// \return Zero or more samples for each dimensional data.
      MetricFamilies Collect() const override {
        std::lock_guard<std::mutex> lock{ mutex };

        if (metrics.empty())
          return {};

        MetricFamily family = MetricFamily{};
        family.type = type;
        family.name = name;
        family.help = help;
        family.metric.reserve(metrics.size());

        for (const std::pair<const Hash, MetricPtr>& metric_pair : metrics) {

          ClientMetric collected = metric_pair.second->Collect();
          for (const Label& constant_label : constant_labels)
            collected.label.emplace_back(ClientMetric::Label(constant_label.first, constant_label.second));

          const Labels& metric_labels = labels.at(metric_pair.first);
          for (const Label& metric_label : metric_labels)
            collected.label.emplace_back(ClientMetric::Label(metric_label.first, metric_label.second));

          family.metric.push_back(std::move(collected));

        }

        return { family };
      }

  };


  template <typename CustomMetric>
  class CustomFamily : public Family {

    public:

      static const Metric::Type static_type = CustomMetric::static_type;

      CustomFamily(const std::string& name, const std::string& help, const Family::Labels& constant_labels)
        : Family(static_type, name, help, constant_labels) {}

      /// \brief Add a new dimensional data.
      ///
      /// Each new set of labels adds a new dimensional data and is exposed in
      /// Prometheus as a time series. It is possible to filter the time series
      /// with Prometheus's query language by appending a set of labels to match in
      /// curly braces ({})
      ///
      ///     http_requests_total{job= "prometheus",method= "POST"}
      ///
      /// \param labels Assign a set of key-value pairs (= labels) to the
      /// dimensional data. The function does nothing, if the same set of labels
      /// already exists.
      /// \param args Arguments are passed to the constructor of metric type T. See
      /// Counter, Gauge, Histogram or Summary for required constructor arguments.
      /// \return Return the newly created dimensional data or - if a same set of
      /// labels already exists - the already existing dimensional data.
      /// \throw std::runtime_exception on invalid label names.
      template <typename... Args>
      CustomMetric& Add (const Labels& new_labels, Args&&... args) {
        const Hash hash = hash_labels (new_labels);
        std::lock_guard<std::mutex> lock{ mutex };

        // try to find existing one
        auto metrics_iter = metrics.find(hash);
        if (metrics_iter != metrics.end()) {
          #ifndef NDEBUG
            // check that we have stored labels for this existing metric
            auto labels_iter = labels.find(hash);
            assert(labels_iter != labels.end());
            const Labels& stored_labels = labels_iter->second;
            assert(new_labels == stored_labels);
          #endif
          return dynamic_cast<CustomMetric&>(*metrics_iter->second);
        }

        // check labels before create the new one
        for (const Label& label_pair : new_labels) {
          const std::string& label_name = label_pair.first;
          if (!CheckLabelName(label_name))
            throw std::invalid_argument("Invalid label name");
          if (constant_labels.count(label_name))
            throw std::invalid_argument("Label name already present in constant labels");
        }

        // create new one
        std::unique_ptr<CustomMetric> metric_ptr (new CustomMetric(std::forward<Args>(args)...));
        CustomMetric& metric = *metric_ptr;

        const auto stored_metric = metrics.insert(std::make_pair(hash, std::move(metric_ptr)));
        assert(stored_metric.second);
        labels.insert({ hash, new_labels });
        labels_reverse_lookup.insert({ stored_metric.first->second.get(), hash });
        
        return metric;
      }

      /// \brief Return a builder to configure and register a Counter metric.
      ///
      /// @copydetails family_base_t<>::family_base_t()
      ///
      /// Example usage:
      ///
      /// \code
      /// auto registry = std::make_shared<Registry>();
      /// auto& counter_family = prometheus::Counter_family::build("some_name", "Additional description.", {{"key", "value"}}, *registry);
      ///
      /// ...
      /// \endcode
      ///
      /// \return An object of unspecified type T, i.e., an implementation detail
      /// except that it has the following members:
      ///
      /// - Name(const std::string&) to set the metric name,
      /// - Help(const std::string&) to set an additional description.
      /// - Label(const std::map<std::string, std::string>&) to assign a set of
      ///   key-value pairs (= labels) to the metric.
      ///
      /// To finish the configuration of the Counter metric, register it with
      /// Register(Registry&).
      template <typename Registry>
      static CustomFamily& Build(Registry& registry, const std::string& name, const std::string& help, const Family::Labels& labels = Family::Labels()) {
        return registry.template Add<CustomFamily>(name, help, labels);
      }

  };


}  // namespace prometheus
