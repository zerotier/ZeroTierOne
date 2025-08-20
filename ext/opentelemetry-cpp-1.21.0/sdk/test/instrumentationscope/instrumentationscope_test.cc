// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationlibrary/instrumentation_library.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;

TEST(InstrumentationScope, CreateInstrumentationScope)
{
  std::string library_name    = "opentelemetry-cpp";
  std::string library_version = "0.1.0";
  std::string schema_url      = "https://opentelemetry.io/schemas/1.2.0";
  uint32_t attrubite_value3[] = {7, 8, 9};
  auto instrumentation_scope  = InstrumentationScope::Create(
      library_name, library_version, schema_url,
      {{"attribute-key1", "attribute-value"},
        {"attribute-key2", static_cast<int32_t>(123)},
        {"attribute-key3", opentelemetry::nostd::span<uint32_t>(attrubite_value3)}});

  EXPECT_EQ(instrumentation_scope->GetName(), library_name);
  EXPECT_EQ(instrumentation_scope->GetVersion(), library_version);
  EXPECT_EQ(instrumentation_scope->GetSchemaURL(), schema_url);

  auto attribute1 = instrumentation_scope->GetAttributes().find("attribute-key1");
  auto attribute2 = instrumentation_scope->GetAttributes().find("attribute-key2");
  auto attribute3 = instrumentation_scope->GetAttributes().find("attribute-key3");

  ASSERT_FALSE(attribute1 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::string>(attribute1->second));
  EXPECT_EQ(opentelemetry::nostd::get<std::string>(attribute1->second), "attribute-value");

  ASSERT_FALSE(attribute2 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<int32_t>(attribute2->second));
  EXPECT_EQ(opentelemetry::nostd::get<int32_t>(attribute2->second), 123);

  ASSERT_FALSE(attribute3 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::vector<uint32_t>>(attribute3->second));

  const std::vector<uint32_t> &attribute3_values =
      opentelemetry::nostd::get<std::vector<uint32_t>>(attribute3->second);
  EXPECT_EQ(attribute3_values.size(), 3);
  for (std::vector<uint32_t>::size_type i = 0; i < 3; ++i)
  {
    EXPECT_EQ(attribute3_values[i], attrubite_value3[i]);
  }
}

TEST(InstrumentationScope, CreateInstrumentationScopeWithLoopForAttributes)
{
  std::string library_name    = "opentelemetry-cpp";
  std::string library_version = "0.1.0";
  std::string schema_url      = "https://opentelemetry.io/schemas/1.2.0";
  uint32_t attrubite_value3[] = {7, 8, 9};

  std::unordered_map<std::string, opentelemetry::common::AttributeValue> attributes = {
      {"attribute-key1", "attribute-value"},
      {"attribute-key2", static_cast<int32_t>(123)},
      {"attribute-key3", opentelemetry::nostd::span<uint32_t>(attrubite_value3)}};

  auto instrumentation_scope =
      InstrumentationScope::Create(library_name, library_version, schema_url, attributes);

  EXPECT_EQ(instrumentation_scope->GetName(), library_name);
  EXPECT_EQ(instrumentation_scope->GetVersion(), library_version);
  EXPECT_EQ(instrumentation_scope->GetSchemaURL(), schema_url);

  auto attribute1 = instrumentation_scope->GetAttributes().find("attribute-key1");
  auto attribute2 = instrumentation_scope->GetAttributes().find("attribute-key2");
  auto attribute3 = instrumentation_scope->GetAttributes().find("attribute-key3");

  ASSERT_FALSE(attribute1 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::string>(attribute1->second));
  EXPECT_EQ(opentelemetry::nostd::get<std::string>(attribute1->second), "attribute-value");

  ASSERT_FALSE(attribute2 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<int32_t>(attribute2->second));
  EXPECT_EQ(opentelemetry::nostd::get<int32_t>(attribute2->second), 123);

  ASSERT_FALSE(attribute3 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::vector<uint32_t>>(attribute3->second));
  const std::vector<uint32_t> &attribute3_values =
      opentelemetry::nostd::get<std::vector<uint32_t>>(attribute3->second);

  EXPECT_EQ(attribute3_values.size(), 3);
  for (std::vector<uint32_t>::size_type i = 0; i < 3; ++i)
  {
    EXPECT_EQ(attribute3_values[i], attrubite_value3[i]);
  }
}

TEST(InstrumentationScope, CreateInstrumentationScopeWithKeyValueIterableAttributes)
{
  std::string library_name    = "opentelemetry-cpp";
  std::string library_version = "0.1.0";
  std::string schema_url      = "https://opentelemetry.io/schemas/1.2.0";
  uint32_t attrubite_value3[] = {7, 8, 9};

  std::unordered_map<std::string, opentelemetry::common::AttributeValue> attributes = {
      {"attribute-key1", "attribute-value"},
      {"attribute-key2", static_cast<int32_t>(123)},
      {"attribute-key3", opentelemetry::nostd::span<uint32_t>(attrubite_value3)}};

  opentelemetry::common::KeyValueIterableView<
      std::unordered_map<std::string, opentelemetry::common::AttributeValue>>
      attributes_view{attributes};

  auto instrumentation_scope =
      InstrumentationScope::Create(library_name, library_version, schema_url, attributes_view);

  EXPECT_EQ(instrumentation_scope->GetName(), library_name);
  EXPECT_EQ(instrumentation_scope->GetVersion(), library_version);
  EXPECT_EQ(instrumentation_scope->GetSchemaURL(), schema_url);

  auto attribute1 = instrumentation_scope->GetAttributes().find("attribute-key1");
  auto attribute2 = instrumentation_scope->GetAttributes().find("attribute-key2");
  auto attribute3 = instrumentation_scope->GetAttributes().find("attribute-key3");

  ASSERT_FALSE(attribute1 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::string>(attribute1->second));
  EXPECT_EQ(opentelemetry::nostd::get<std::string>(attribute1->second), "attribute-value");

  ASSERT_FALSE(attribute2 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<int32_t>(attribute2->second));
  EXPECT_EQ(opentelemetry::nostd::get<int32_t>(attribute2->second), 123);

  ASSERT_FALSE(attribute3 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::vector<uint32_t>>(attribute3->second));
  const std::vector<uint32_t> &attribute3_values =
      opentelemetry::nostd::get<std::vector<uint32_t>>(attribute3->second);

  EXPECT_EQ(attribute3_values.size(), 3);
  for (std::vector<uint32_t>::size_type i = 0; i < 3; ++i)
  {
    EXPECT_EQ(attribute3_values[i], attrubite_value3[i]);
  }
}

TEST(InstrumentationScope, SetAttribute)
{
  std::string library_name    = "opentelemetry-cpp";
  std::string library_version = "0.1.0";
  std::string schema_url      = "https://opentelemetry.io/schemas/1.2.0";
  uint32_t attrubite_value3[] = {7, 8, 9};
  auto instrumentation_scope =
      InstrumentationScope::Create(library_name, library_version, schema_url);

  EXPECT_EQ(instrumentation_scope->GetName(), library_name);
  EXPECT_EQ(instrumentation_scope->GetVersion(), library_version);
  EXPECT_EQ(instrumentation_scope->GetSchemaURL(), schema_url);
  EXPECT_EQ(instrumentation_scope->GetAttributes().size(), 0);

  instrumentation_scope->SetAttribute("attribute-key1", "attribute-value");
  instrumentation_scope->SetAttribute("attribute-key2", static_cast<int32_t>(123));
  instrumentation_scope->SetAttribute("attribute-key3",
                                      opentelemetry::nostd::span<uint32_t>(attrubite_value3));
  EXPECT_EQ(instrumentation_scope->GetAttributes().size(), 3);

  auto attribute1 = instrumentation_scope->GetAttributes().find("attribute-key1");
  auto attribute2 = instrumentation_scope->GetAttributes().find("attribute-key2");
  auto attribute3 = instrumentation_scope->GetAttributes().find("attribute-key3");

  ASSERT_FALSE(attribute1 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::string>(attribute1->second));
  EXPECT_EQ(opentelemetry::nostd::get<std::string>(attribute1->second), "attribute-value");

  ASSERT_FALSE(attribute2 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<int32_t>(attribute2->second));
  EXPECT_EQ(opentelemetry::nostd::get<int32_t>(attribute2->second), 123);

  ASSERT_FALSE(attribute3 == instrumentation_scope->GetAttributes().end());
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::vector<uint32_t>>(attribute3->second));

  const std::vector<uint32_t> &attribute3_values =
      opentelemetry::nostd::get<std::vector<uint32_t>>(attribute3->second);
  EXPECT_EQ(attribute3_values.size(), 3);
  for (std::vector<uint32_t>::size_type i = 0; i < 3; ++i)
  {
    EXPECT_EQ(attribute3_values[i], attrubite_value3[i]);
  }
}

TEST(InstrumentationScope, LegacyInstrumentationLibrary)
{

  std::string library_name    = "opentelemetry-cpp";
  std::string library_version = "0.1.0";
  std::string schema_url      = "https://opentelemetry.io/schemas/1.2.0";
  auto instrumentation_library =
      opentelemetry::sdk::instrumentationlibrary::InstrumentationLibrary::Create(
          library_name, library_version, schema_url);

  EXPECT_EQ(instrumentation_library->GetName(), library_name);
  EXPECT_EQ(instrumentation_library->GetVersion(), library_version);
  EXPECT_EQ(instrumentation_library->GetSchemaURL(), schema_url);
}

TEST(InstrumentationScope, Equal)
{
  using Attributes = std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>;
  Attributes attributes_empty = {};
  Attributes attributes_match = {
      {"key0", "some value"}, {"key1", 1}, {"key2", 2.0}, {"key3", true}};
  Attributes attributes_different = {{"key42", "some other"}};

  auto kv_iterable_empty =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_empty);
  auto kv_iterable_match =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_match);
  auto kv_iterable_different =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_different);

  // try with no attributes added to the instrumentation scope
  auto instrumentation_scope =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name", "library_version", "schema_url");

  // the instrumentation scope is equal if all parameters are equal (must handle nullptr attributes
  // or empty attributes)
  EXPECT_TRUE(instrumentation_scope->equal("library_name", "library_version", "schema_url"));
  EXPECT_TRUE(
      instrumentation_scope->equal("library_name", "library_version", "schema_url", nullptr));
  EXPECT_TRUE(instrumentation_scope->equal("library_name", "library_version", "schema_url",
                                           &kv_iterable_empty));

  // the instrumentation scope is not equal if any parameter is different
  EXPECT_FALSE(instrumentation_scope->equal("library_name", ""));
  EXPECT_FALSE(instrumentation_scope->equal("library_name", "library_version", ""));
  EXPECT_FALSE(instrumentation_scope->equal("library_name", "library_version", "schema_url",
                                            &kv_iterable_different));

  // try with attributes added to the instrumentation scope
  auto instrumentation_scope_w_attributes =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name", "library_version", "schema_url", attributes_match);

  // the instrumentation scope is equal if all parameters including all attribute keys, types, and
  // values are equal
  EXPECT_TRUE(instrumentation_scope_w_attributes->equal("library_name", "library_version",
                                                        "schema_url", &kv_iterable_match));
  EXPECT_FALSE(instrumentation_scope_w_attributes->equal("library_name", "library_version",
                                                         "schema_url", nullptr));
  EXPECT_FALSE(instrumentation_scope_w_attributes->equal("library_name", "library_version",
                                                         "schema_url", &kv_iterable_empty));
  EXPECT_FALSE(instrumentation_scope_w_attributes->equal("library_name", "library_version",
                                                         "schema_url", &kv_iterable_different));
}

TEST(InstrumentationScope, OperatorEqual)
{
  using Attributes = std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>;
  Attributes attributes_empty = {};
  Attributes attributes_match = {
      {"key0", "some value"}, {"key1", 1}, {"key2", 2.0}, {"key3", true}};
  Attributes attributes_different = {{"key42", "some other"}};

  auto instrumentation_scope_1a =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name", "library_version", "schema_url");

  auto instrumentation_scope_1b =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name", "library_version", "schema_url");

  auto instrumentation_scope_2a =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name_2", "library_version", "schema_url");

  auto instrumentation_scope_2b =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name_2", "library_version", "schema_url", attributes_empty);

  auto instrumentation_scope_3a =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name_2", "library_version", "schema_url", attributes_match);

  auto instrumentation_scope_3b =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name_2", "library_version", "schema_url", attributes_match);

  auto instrumentation_scope_4 =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "library_name_2", "library_version", "schema_url", attributes_different);

  EXPECT_EQ(*instrumentation_scope_1a, *instrumentation_scope_1b);
  EXPECT_FALSE(*instrumentation_scope_1a == *instrumentation_scope_2a);
  EXPECT_EQ(*instrumentation_scope_2a, *instrumentation_scope_2b);
  EXPECT_EQ(*instrumentation_scope_3a, *instrumentation_scope_3b);
  EXPECT_FALSE(*instrumentation_scope_3a == *instrumentation_scope_4);
}
