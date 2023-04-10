#include "prometheus/simpleapi.h"

#include <memory>

namespace prometheus {
  namespace simpleapi {

    std::shared_ptr<Registry> registry_ptr = std::make_shared<Registry>();
    Registry&                 registry = *registry_ptr;
    SaveToFile saver(registry_ptr, std::chrono::seconds(5), std::string("./metrics.prom"));

  }
}
