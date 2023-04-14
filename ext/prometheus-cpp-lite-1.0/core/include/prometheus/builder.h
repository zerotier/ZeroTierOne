#pragma once

#include <string>
#include <map>
#include "registry.h"

namespace prometheus {

  template <typename CustomMetric>
  class Builder {

    Family::Labels labels_;
    std::string name_;
    std::string help_;

  public:
    Builder& Labels(const std::map<const std::string, const std::string>& labels) {
      labels_ = labels;
      return *this;
    }
    Builder& Name(const std::string& name) {
      name_ = name;
      return *this;
    }
    Builder& Help(const std::string& help) {
      help_ = help;
      return *this;
    }
    CustomFamily<CustomMetric>& Register(Registry& registry) {
      return registry.Add<CustomFamily<CustomMetric>>(name_, help_, labels_);
    }

  };

}