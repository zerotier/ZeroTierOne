// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <sstream>
#include <string>
#include <vector>
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/attribute_utils.h"

#pragma once
OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace ostream_common
{
/*
  print_value is used to print out the value of an attribute within a vector.
  These values are held in a variant which makes the process of printing them much more
  complicated.
*/

template <typename T>
void print_value(const T &item, std::ostream &sout)
{
  sout << item;
}

template <typename T>
void print_value(const std::vector<T> &vec, std::ostream &sout)
{
  sout << '[';
  size_t i  = 1;
  size_t sz = vec.size();
  for (auto v : vec)
  {
    sout << v;
    if (i != sz)
      sout << ',';
    i++;
  };
  sout << ']';
}

template <typename T>
void print_value(const nostd::span<T> &vec, std::ostream &sout)
{
  sout << '[';
  size_t i  = 1;
  size_t sz = vec.size();
  for (auto v : vec)
  {
    sout << v;
    if (i != sz)
      sout << ',';
    i++;
  };
  sout << ']';
}

// Prior to C++14, generic lambda is not available so fallback to functor.
#if (!defined(_MSVC_LANG) && (__cplusplus < 201402L)) || \
    (defined(_MSVC_LANG) && (_MSVC_LANG < 201402L))

class OwnedAttributeValueVisitor
{
public:
  OwnedAttributeValueVisitor(std::ostream &sout) : sout_(sout) {}

  template <typename T>
  void operator()(T &&arg)
  {
    print_value(arg, sout_);
  }

private:
  std::ostream &sout_;
};

class AttributeValueVisitor
{
public:
  AttributeValueVisitor(std::ostream &sout) : sout_(sout) {}

  template <typename T>
  void operator()(T &&arg)
  {
    print_value(arg, sout_);
  }

  void operator()(nostd::string_view &&arg) { sout_.write(arg.data(), arg.size()); }

private:
  std::ostream &sout_;
};

#endif

inline void print_value(const opentelemetry::sdk::common::OwnedAttributeValue &value,
                        std::ostream &sout)
{
#if (!defined(_MSVC_LANG) && (__cplusplus < 201402L)) || \
    (defined(_MSVC_LANG) && (_MSVC_LANG < 201402L))
  opentelemetry::nostd::visit(OwnedAttributeValueVisitor(sout), value);
#else
  opentelemetry::nostd::visit(
      [&sout](auto &&arg) {
        /* explicit this is needed by some gcc versions (observed with v5.4.0)*/
        print_value(arg, sout);
      },
      value);
#endif
}

inline void print_value(const opentelemetry::common::AttributeValue &value, std::ostream &sout)
{
#if (!defined(_MSVC_LANG) && (__cplusplus < 201402L)) || \
    (defined(_MSVC_LANG) && (_MSVC_LANG < 201402L))
  opentelemetry::nostd::visit(AttributeValueVisitor(sout), value);
#else
  opentelemetry::nostd::visit(
      [&sout](auto &&arg) {
        /* explicit this is needed by some gcc versions (observed with v5.4.0)*/
        print_value(arg, sout);
      },
      value);
#endif
}

}  // namespace ostream_common
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
