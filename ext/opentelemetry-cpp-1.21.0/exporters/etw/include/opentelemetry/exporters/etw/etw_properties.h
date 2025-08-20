// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "opentelemetry/version.h"

#include "opentelemetry/common/key_value_iterable_view.h"

#include <opentelemetry/nostd/span.h>
#include <opentelemetry/nostd/type_traits.h>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <Windows.h>
#endif

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace etw
{

/**
 * @brief PropertyVariant provides:
 * - a constructor to initialize from initializer lists
 * - an owning wrapper around `common::AttributeValue`
 */
using PropertyVariant =
    nostd::variant<bool,
                   int32_t,
                   int64_t,
                   uint32_t,
                   uint64_t,
                   double,
                   std::string,
                   const char *,
                   // 8-bit byte arrays / binary blobs are not part of OT spec yet!
                   // Ref: https://github.com/open-telemetry/opentelemetry-specification/issues/780
                   std::vector<uint8_t>,
                   std::vector<bool>,
                   std::vector<int32_t>,
                   std::vector<int64_t>,
                   std::vector<uint32_t>,
                   std::vector<uint64_t>,
                   std::vector<double>,
                   std::vector<std::string>>;

enum PropertyType
{
  kTypeBool,
  kTypeInt,
  kTypeInt64,
  kTypeUInt,
  kTypeUInt64,
  kTypeDouble,
  kTypeString,
  kTypeCString,
  kTypeSpanByte,
  kTypeSpanBool,
  kTypeSpanInt,
  kTypeSpanInt64,
  kTypeSpanUInt,
  kTypeSpanUInt64,
  kTypeSpanDouble,
  kTypeSpanString
};

/**
 * @brief PropertyValue class that holds PropertyVariant and
 * provides converter for non-owning common::AttributeValue
 */
class PropertyValue : public PropertyVariant
{

  /**
   * @brief Convert span<T> to vector<T>
   * @tparam T
   * @param source
   * @return
   */
  template <typename T>
  static std::vector<T> to_vector(const nostd::span<const T, nostd::dynamic_extent> &source)
  {
    return std::vector<T>(source.begin(), source.end());
  }

  /**
   * @brief Convert span<string_view> to vector<string>
   * @param source Span of non-owning string views.
   * @return Vector of owned strings.
   */
  std::vector<std::string> static to_vector(const nostd::span<const nostd::string_view> &source)
  {
    std::vector<std::string> result(source.size());
    for (const auto &item : source)
    {
      result.push_back(std::string(item.data()));
    }
    return result;
  }

  /**
   * @brief Convert vector<INTEGRAL> to span<INTEGRAL>.
   * @tparam T Integral type
   * @param vec Vector of integral type primitives to convert to span.
   * @return Span of integral type primitives.
   */
  template <typename T, nostd::enable_if_t<std::is_integral<T>::value, bool> = true>
  static nostd::span<const T> to_span(const std::vector<T> &vec)
  {
    nostd::span<const T> result(vec.data(), vec.size());
    return result;
  }

  /**
   * @brief Convert vector<FLOAT> to span<const FLOAT>.
   * @tparam T Float type
   * @param vec Vector of float type primitives to convert to span.
   * @return Span of float type primitives.
   */
  template <typename T, nostd::enable_if_t<std::is_floating_point<T>::value, bool> = true>
  static nostd::span<const T> to_span(const std::vector<T> &vec)
  {
    nostd::span<const T> result(vec.data(), vec.size());
    return result;
  }

public:
  /**
   * @brief PropertyValue from bool
   * @param v
   * @return
   */
  PropertyValue(bool value) : PropertyVariant(value) {}

  /**
   * @brief PropertyValue from integral.
   * @param v
   * @return
   */
  template <typename TInteger, nostd::enable_if_t<std::is_integral<TInteger>::value, bool> = true>
  PropertyValue(TInteger number) : PropertyVariant(number)
  {}

  /**
   * @brief PropertyValue from floating point.
   * @param v
   * @return
   */
  template <typename TFloat, nostd::enable_if_t<std::is_floating_point<TFloat>::value, bool> = true>
  PropertyValue(TFloat number) : PropertyVariant(double(number))
  {}

  /**
   * @brief Default PropertyValue (int32_t=0)
   * @param v
   * @return
   */
  PropertyValue() : PropertyVariant(int32_t(0)) {}

  /**
   * @brief PropertyValue from array of characters as string.
   *
   * @param v
   * @return
   */
  PropertyValue(char value[]) : PropertyVariant(std::string(value)) {}

  /**
   * @brief PropertyValue from array of characters as string.
   *
   * @param v
   * @return
   */
  PropertyValue(const char *value) : PropertyVariant(std::string(value)) {}

  /**
   * @brief PropertyValue from string.
   *
   * @param v
   * @return
   */
  PropertyValue(const std::string &value) : PropertyVariant(value) {}

  /**
   * @brief PropertyValue from vector as array.
   * @return
   */
  template <typename T>
  PropertyValue(std::vector<T> value) : PropertyVariant(value)
  {}

  /**
   * @brief Convert non-owning common::AttributeValue to owning PropertyValue.
   * @return
   */
  PropertyValue &FromAttributeValue(const opentelemetry::common::AttributeValue &v)
  {
    switch (v.index())
    {
      case opentelemetry::common::AttributeType::kTypeBool:
        PropertyVariant::operator=(nostd::get<bool>(v));
        break;
      case opentelemetry::common::AttributeType::kTypeInt:
        PropertyVariant::operator=(nostd::get<int32_t>(v));
        break;
      case opentelemetry::common::AttributeType::kTypeInt64:
        PropertyVariant::operator=(nostd::get<int64_t>(v));
        break;
      case opentelemetry::common::AttributeType::kTypeUInt:
        PropertyVariant::operator=(nostd::get<uint32_t>(v));
        break;
      case opentelemetry::common::AttributeType::kTypeUInt64:
        PropertyVariant::operator=(nostd::get<uint64_t>(v));
        break;
      case opentelemetry::common::AttributeType::kTypeDouble:
        PropertyVariant::operator=(nostd::get<double>(v));
        break;
      case opentelemetry::common::AttributeType::kTypeCString: {
        PropertyVariant::operator=(nostd::get<const char *>(v));
        break;
      }
      case opentelemetry::common::AttributeType::kTypeString: {
        PropertyVariant::operator=(
            std::string{nostd::string_view(nostd::get<nostd::string_view>(v)).data()});
        break;
      }

      case opentelemetry::common::AttributeType::kTypeSpanByte:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const uint8_t>>(v)));
        break;

      case opentelemetry::common::AttributeType::kTypeSpanBool:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const bool>>(v)));
        break;

      case opentelemetry::common::AttributeType::kTypeSpanInt:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const int32_t>>(v)));
        break;

      case opentelemetry::common::AttributeType::kTypeSpanInt64:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const int64_t>>(v)));
        break;

      case opentelemetry::common::AttributeType::kTypeSpanUInt:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const uint32_t>>(v)));
        break;

      case opentelemetry::common::AttributeType::kTypeSpanUInt64:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const uint64_t>>(v)));
        break;

      case opentelemetry::common::AttributeType::kTypeSpanDouble:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const double>>(v)));
        break;

      case opentelemetry::common::AttributeType::kTypeSpanString:
        PropertyVariant::operator=(to_vector(nostd::get<nostd::span<const nostd::string_view>>(v)));
        break;

      default:
        break;
    }
    return (*this);
  }

  /**
   * @brief Convert owning PropertyValue to non-owning common::AttributeValue
   * @param other
   */
  opentelemetry::common::AttributeValue ToAttributeValue() const
  {
    opentelemetry::common::AttributeValue value;

    switch (this->index())
    {
      case PropertyType::kTypeBool:
        value = nostd::get<bool>(*this);
        break;
      case PropertyType::kTypeInt:
        value = nostd::get<int32_t>(*this);
        break;
      case PropertyType::kTypeInt64:
        value = nostd::get<int64_t>(*this);
        break;
      case PropertyType::kTypeUInt:
        value = nostd::get<uint32_t>(*this);
        break;
      case PropertyType::kTypeUInt64:
        value = nostd::get<uint64_t>(*this);
        break;
      case PropertyType::kTypeDouble:
        value = nostd::get<double>(*this);
        break;
      case PropertyType::kTypeString: {
        const std::string &str = nostd::get<std::string>(*this);
        return nostd::string_view(str.data(), str.size());
        break;
      }
      case PropertyType::kTypeCString: {
        const char *data = nostd::get<const char *>(*this);
        return nostd::string_view(data, (data) ? strlen(data) : 0);
        break;
      }
      case PropertyType::kTypeSpanByte: {
        value = to_span(nostd::get<std::vector<uint8_t>>(*this));
        break;
      }
      case PropertyType::kTypeSpanBool: {
        const auto &vec = nostd::get<std::vector<bool>>(*this);
        // FIXME: sort out how to remap from vector<bool> to span<bool>
        UNREFERENCED_PARAMETER(vec);
        break;
      }
      case PropertyType::kTypeSpanInt:
        value = to_span(nostd::get<std::vector<int32_t>>(*this));
        break;

      case PropertyType::kTypeSpanInt64:
        value = to_span(nostd::get<std::vector<int64_t>>(*this));
        break;

      case PropertyType::kTypeSpanUInt:
        value = to_span(nostd::get<std::vector<uint32_t>>(*this));
        break;

      case PropertyType::kTypeSpanUInt64:
        value = to_span(nostd::get<std::vector<uint64_t>>(*this));
        break;

      case PropertyType::kTypeSpanDouble:
        value = to_span(nostd::get<std::vector<double>>(*this));
        break;

      case PropertyType::kTypeSpanString:
        // FIXME: sort out how to remap from vector<string> to span<string_view>
        // value = to_span(nostd::get<std::vector<std::string>>(self));
        break;

      default:
        break;
    }
    return value;
  }
};

/**
 * @brief Map of PropertyValue
 */
using PropertyValueMap = std::map<std::string, PropertyValue>;

/**
 * @brief Map of PropertyValue with common::KeyValueIterable interface.
 */
class Properties : public opentelemetry::common::KeyValueIterable, public PropertyValueMap
{

  /**
   * @brief Helper tyoe for map constructor.
   */
  using PropertyValueType = std::pair<const std::string, PropertyValue>;

public:
  /**
   * @brief PropertyValueMap constructor.
   */
  Properties() : PropertyValueMap() {}

  /**
   * @brief PropertyValueMap constructor from initializer list.
   */
  Properties(const std::initializer_list<PropertyValueType> properties) : PropertyValueMap()
  {
    (*this) = (properties);
  }

  /**
   * @brief PropertyValueMap assignment operator from initializer list.
   */
  Properties &operator=(std::initializer_list<PropertyValueType> properties)
  {
    PropertyValueMap::operator=(properties);
    return (*this);
  }

  /**
   * @brief PropertyValueMap constructor from map.
   */
  Properties(const PropertyValueMap &properties) : PropertyValueMap() { (*this) = properties; }

  /**
   * @brief PropertyValueMap assignment operator from map.
   */
  Properties &operator=(const PropertyValueMap &properties)
  {
    PropertyValueMap::operator=(properties);
    return (*this);
  }

  /**
   * @brief PropertyValueMap constructor from KeyValueIterable
   * allows to convert non-Owning KeyValueIterable to owning
   * container.
   *
   */
  Properties(const opentelemetry::common::KeyValueIterable &other) { (*this) = other; }

  /**
   * @brief PropertyValueMap assignment operator.
   */
  Properties &operator=(const opentelemetry::common::KeyValueIterable &other)
  {
    clear();
    other.ForEachKeyValue(
        [&](nostd::string_view key, opentelemetry::common::AttributeValue value) noexcept {
          std::string k(key.data(), key.length());
          (*this)[k].FromAttributeValue(value);
          return true;
        });
    return (*this);
  }

  /**
   * @brief PropertyValueMap property accessor.
   */
  PropertyValue &operator[](const std::string &k) { return PropertyValueMap::operator[](k); }

  /**
   * Iterate over key-value pairs
   * @param callback a callback to invoke for each key-value. If the callback returns false,
   * the iteration is aborted.
   * @return true if every key-value pair was iterated over
   */
  bool ForEachKeyValue(
      nostd::function_ref<bool(nostd::string_view, opentelemetry::common::AttributeValue)> callback)
      const noexcept override
  {
    for (const auto &kv : (*this))
    {
      const opentelemetry::common::AttributeValue &value = kv.second.ToAttributeValue();
      if (!callback(nostd::string_view{kv.first}, value))
      {
        return false;
      }
    }
    return true;
  }

  /**
   * @return the number of key-value pairs
   */
  size_t size() const noexcept override { return PropertyValueMap::size(); }
};

}  // namespace etw
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
