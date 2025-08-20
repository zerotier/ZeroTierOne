// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <map>

#if defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)
#  include <set>
#endif  // defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/trace/span_id.h"

#include "opentelemetry/exporters/etw/etw_provider.h"
#include "opentelemetry/exporters/etw/etw_tail_sampler.h"
#include "opentelemetry/sdk/trace/id_generator.h"
#include "opentelemetry/sdk/trace/sampler.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace etw
{
/**
 * @brief TelemetryProvider Options passed via SDK API.
 */

#if defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)
using TelemetryProviderOptions = std::map<std::string,
                                          nostd::variant<std::string,
                                                         uint64_t,
                                                         float,
                                                         bool,
                                                         std::map<std::string, std::string>,
                                                         std::set<std::string>>>;

#else
using TelemetryProviderOptions = std::map<
    std::string,
    nostd::variant<std::string, uint64_t, float, bool, std::map<std::string, std::string>>>;

#endif  // defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

/**
 * @brief TelemetryProvider runtime configuration class. Internal representation
 * of TelemetryProviderOptions used by various components of SDK.
 */
typedef struct
{
  bool enableTraceId;            // Set `TraceId` on ETW events
  bool enableSpanId;             // Set `SpanId` on ETW events
  bool enableActivityId;         // Assign `SpanId` to `ActivityId`
  bool enableActivityTracking;   // Emit TraceLogging events for Span/Start and Span/Stop Not used
                                 // for Logs
  bool enableRelatedActivityId;  // Assign parent `SpanId` to `RelatedActivityId`
  bool enableAutoParent;  // Start new spans as children of current active span, Not used for Logs
  ETWProvider::EventFormat
      encoding;  // Event encoding to use for this provider (TLD, MsgPack, XML, etc.).
  bool enableTableNameMappings;  // Map instrumentation scope name to table name with
                                 // `tableNameMappings`
  std::map<std::string, std::string> tableNameMappings;

#if defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

  std::set<std::string> timestampAttributes;  // Attributes to use as timestamp

#endif  // defined(OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW)

} TelemetryProviderConfiguration;

/**
 * @brief Helper template to convert a variant value from TelemetryProviderOptions to
 * LoggerProviderConfiguration
 *
 * @param options           TelemetryProviderOptions passed on API surface
 * @param key               Option name
 * @param value             Reference to destination value
 * @param defaultValue      Default value if option is not supplied
 */
template <typename T>
static inline void GetOption(const TelemetryProviderOptions &options,
                             const char *key,
                             T &value,
                             T defaultValue)
{
  auto it = options.find(key);
  if (it != options.end())
  {
    auto val = it->second;
    value    = nostd::get<T>(val);
  }
  else
  {
    value = defaultValue;
  }
}

/**
 * @brief Helper template to convert encoding config option to EventFormat.
 * Configuration option passed as `options["encoding"] = "MsgPack"`.
 * Default encoding is TraceLogging Dynamic Manifest (TLD).
 *
 * Valid encoding names listed below.
 *
 * For MessagePack encoding:
 * - "MSGPACK"
 * - "MsgPack"
 * - "MessagePack"
 *
 * For XML encoding:
 * - "XML"
 * - "xml"
 *
 * For TraceLogging Dynamic encoding:
 * - "TLD"
 * - "tld"
 *
 */
static inline ETWProvider::EventFormat GetEncoding(const TelemetryProviderOptions &options)
{
  ETWProvider::EventFormat evtFmt = ETWProvider::EventFormat::ETW_MANIFEST;

  auto it = options.find("encoding");
  if (it != options.end())
  {
    auto varValue   = it->second;
    std::string val = nostd::get<std::string>(varValue);

#pragma warning(push)
#pragma warning(disable : 4307) /* Integral constant overflow - OK while computing hash */
    auto h = utils::hashCode(val.c_str());
    switch (h)
    {
      case CONST_HASHCODE(MSGPACK):
        // nobrk
      case CONST_HASHCODE(MsgPack):
        // nobrk
      case CONST_HASHCODE(MessagePack):
        evtFmt = ETWProvider::EventFormat::ETW_MSGPACK;
        break;

      case CONST_HASHCODE(XML):
        // nobrk
      case CONST_HASHCODE(xml):
        evtFmt = ETWProvider::EventFormat::ETW_XML;
        break;

      case CONST_HASHCODE(TLD):
        // nobrk
      case CONST_HASHCODE(tld):
        // nobrk
        evtFmt = ETWProvider::EventFormat::ETW_MANIFEST;
        break;

      default:
        break;
    }
#pragma warning(pop)
  }

  return evtFmt;
}

/**
 * @brief Utility template to obtain etw::TracerProvider._config or etw::LoggerProvider._config
 *
 * @tparam T    etw::TracerProvider
 * @param t     etw::TracerProvider ref
 * @return      TelemetryProviderConfiguration ref
 */
template <class T>
TelemetryProviderConfiguration &GetConfiguration(T &t)
{
  return t.config_;
}

/**
 * @brief Utility function to obtain etw::TracerProvider.id_generator_
 */
template <class T>
sdk::trace::IdGenerator &GetIdGenerator(T &t)
{
  return *t.id_generator_;
}

/**
 * @brief Utility function to obtain etw::TracerProvider.sampler_
 */
template <class T>
sdk::trace::Sampler &GetSampler(T &t)
{
  return *t.sampler_;
}

/**
 * @brief Utility function to obtain etw::TracerProvider.tail_sampler_
 */
template <class T>
TailSampler &GetTailSampler(T &t)
{
  return *t.tail_sampler_;
}

/**
 * @brief Utility template to convert SpanId or TraceId to hex.
 * @param id - value of SpanId or TraceId
 * @return Hexadecimal representation of Id as string.
 */
template <class T>
static inline std::string ToLowerBase16(const T &id)
{
  char buf[2 * T::kSize] = {0};
  id.ToLowerBase16(buf);
  return std::string(buf, sizeof(buf));
}

/**
 * @brief Utility method to convert span_id (8 byte) to ActivityId GUID (16 bytes)
 * @param span OpenTelemetry Span Id object
 * @return GUID struct containing 8-bytes of SpanId + 8 NUL bytes.
 */
static inline bool CopySpanIdToActivityId(const opentelemetry::trace::SpanId &span_id,
                                          GUID &outGuid)
{
  memset(&outGuid, 0, sizeof(outGuid));
  if (!span_id.IsValid())
  {
    return false;
  }
  auto spanId      = span_id.Id().data();
  uint8_t *guidPtr = reinterpret_cast<uint8_t *>(&outGuid);
  for (size_t i = 0; i < 8; i++)
  {
    guidPtr[i] = spanId[i];
  }
  return true;
}

}  // namespace etw
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
