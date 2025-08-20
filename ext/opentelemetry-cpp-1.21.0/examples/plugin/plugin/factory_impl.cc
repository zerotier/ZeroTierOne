// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <new>
#include <utility>

#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/plugin/factory.h"
#include "opentelemetry/plugin/hook.h"
#include "opentelemetry/plugin/tracer.h"
#include "tracer.h"

namespace nostd  = opentelemetry::nostd;
namespace plugin = opentelemetry::plugin;

class TracerHandle final : public plugin::TracerHandle
{
public:
  explicit TracerHandle(std::shared_ptr<Tracer> &&tracer) noexcept : tracer_{std::move(tracer)} {}

  // opentelemetry::plugin::TracerHandle
  Tracer &tracer() const noexcept override { return *tracer_; }

private:
  std::shared_ptr<Tracer> tracer_;
};

class FactoryImpl final : public plugin::Factory::FactoryImpl
{
public:
  // opentelemetry::plugin::Factory::FactoryImpl
  nostd::unique_ptr<plugin::TracerHandle> MakeTracerHandle(
      nostd::string_view /* tracer_config */,
      nostd::unique_ptr<char[]> & /* error_message */) const noexcept override
  {
    std::shared_ptr<Tracer> tracer{new (std::nothrow) Tracer{""}};
    if (tracer == nullptr)
    {
      return nullptr;
    }
    return nostd::unique_ptr<TracerHandle>{new (std::nothrow) TracerHandle{std::move(tracer)}};
  }
};

static nostd::unique_ptr<plugin::Factory::FactoryImpl> MakeFactoryImpl(
    const plugin::LoaderInfo &loader_info,
    nostd::unique_ptr<char[]> &error_message) noexcept
{
  (void)loader_info;
  (void)error_message;
  return nostd::unique_ptr<plugin::Factory::FactoryImpl>{new (std::nothrow) FactoryImpl{}};
}

OPENTELEMETRY_DEFINE_PLUGIN_HOOK(MakeFactoryImpl)
