#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

#include "prometheus/detail/ckms_quantiles.h"

namespace prometheus {
  namespace detail {

    class TimeWindowQuantiles {

      using Clock = std::chrono::steady_clock;

       public:
        TimeWindowQuantiles(const std::vector<CKMSQuantiles::Quantile>& quantiles,
                            const Clock::duration max_age, const int age_buckets)
          : quantiles_(quantiles),
            ckms_quantiles_(age_buckets, CKMSQuantiles(quantiles_)),
            current_bucket_(0),
            last_rotation_(Clock::now()),
            rotation_interval_(max_age / age_buckets) {}

        double get(double q) const {
          CKMSQuantiles& current_bucket = rotate();
          return current_bucket.get(q);
        }

        void insert(double value) {
          rotate();
          for (auto& bucket : ckms_quantiles_) {
            bucket.insert(value);
          }
        }

       private:
        CKMSQuantiles& rotate() const {
          auto delta = Clock::now() - last_rotation_;
          while (delta > rotation_interval_) {
            ckms_quantiles_[current_bucket_].reset();

            if (++current_bucket_ >= ckms_quantiles_.size()) {
              current_bucket_ = 0;
            }

            delta -= rotation_interval_;
            last_rotation_ += rotation_interval_;
          }
          return ckms_quantiles_[current_bucket_];
        }

        const std::vector<CKMSQuantiles::Quantile>& quantiles_;
        mutable std::vector<CKMSQuantiles> ckms_quantiles_;
        mutable std::size_t current_bucket_;

        mutable Clock::time_point last_rotation_;
        const Clock::duration rotation_interval_;
      };

  }  // namespace detail
}  // namespace prometheus
