#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "prometheus/collectable.h"
#include "prometheus/family.h"

namespace prometheus {

  /// \brief Manages the collection of a number of metrics.
  ///
  /// The Registry is responsible to expose data to a class/method/function
  /// "bridge", which returns the metrics in a format Prometheus supports.
  ///
  /// The key class is the Collectable. This has a method - called Collect() -
  /// that returns zero or more metrics and their samples. The metrics are
  /// represented by the class Family<>, which implements the Collectable
  /// interface. A new metric is registered with BuildCounter(), BuildGauge(),
  /// BuildHistogram() or BuildSummary().
  ///
  /// The class is thread-safe. No concurrent call to any API of this type causes
  /// a data race.
  class Registry : public Collectable {

    public:

      /// \brief How to deal with repeatedly added family names for a type.
      ///
      /// Adding a family with the same name but different types is always an error
      /// and will lead to an exception.
      enum class InsertBehavior {
        /// \brief If a family with the same name and labels already exists return
        /// the existing one. If no family with that name exists create it.
        /// Otherwise throw.
        Merge,
        /// \brief Throws if a family with the same name already exists.
        Throw,
        /// \brief Never merge and always create a new family. This violates the
        /// prometheus specification but was the default behavior in earlier
        /// versions
        NonStandardAppend,
      };

      using FamilyPtr = std::unique_ptr<Family>;
      using Families  = std::vector<FamilyPtr>;

      const InsertBehavior insert_behavior;
      mutable std::mutex   mutex;
      Families             families;

      /// \brief name Create a new registry.
      ///
      /// \param insert_behavior How to handle families with the same name.
      Registry (InsertBehavior insert_behavior_ = InsertBehavior::Merge)
        : insert_behavior(insert_behavior_) {}

      /// \brief Returns a list of metrics and their samples.
      ///
      /// Every time the Registry is scraped it calls each of the metrics Collect
      /// function.
      ///
      /// \return Zero or more metrics and their samples.
      virtual MetricFamilies Collect() const {

        std::lock_guard<std::mutex> lock{ mutex };

        MetricFamilies results;

        for (const FamilyPtr& family_ptr : families) {
          MetricFamilies metrics = family_ptr->Collect();
          results.insert(results.end(), std::make_move_iterator(metrics.begin()), std::make_move_iterator(metrics.end()));
        }

        return results;

      }

      template <typename CustomFamily>
      CustomFamily& Add (const std::string& name, const std::string& help, const Family::Labels& labels) {

        std::lock_guard<std::mutex> lock{ mutex };

        bool found_one_but_not_merge = false;
        for (const FamilyPtr& family_ptr : families) {
          if (family_ptr->GetName() == name) {

            if (family_ptr->type != CustomFamily::static_type) // found family with this name and with different type
              throw std::invalid_argument("Family name already exists with different type");

            else {                                 // found family with this name and the same type
              switch (insert_behavior) {
                case InsertBehavior::Throw:
                  throw std::invalid_argument("Family name already exists");
                case InsertBehavior::Merge:
                  if (family_ptr->GetConstantLabels() == labels)
                    return dynamic_cast<CustomFamily&>(*family_ptr);
                  else                               // this strange rule was in previos version prometheus cpp
                    found_one_but_not_merge = true;
                case InsertBehavior::NonStandardAppend:
                  continue;
              }
            }

          }
        }

        if (found_one_but_not_merge)               // this strange rule was in previos version prometheus cpp
          throw std::invalid_argument("Family name already exists with different labels");

        std::unique_ptr<CustomFamily> new_family_ptr (new CustomFamily(name, help, labels));
        CustomFamily& new_family = *new_family_ptr;
        families.push_back(std::move(new_family_ptr));
        return new_family;

      }

  };

}  // namespace prometheus
