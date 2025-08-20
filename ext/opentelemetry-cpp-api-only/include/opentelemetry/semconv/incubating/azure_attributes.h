/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_attributes-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace azure
{

/**
  The unique identifier of the client instance.
 */
static constexpr const char *kAzureClientId = "azure.client.id";

/**
  Cosmos client connection mode.
 */
static constexpr const char *kAzureCosmosdbConnectionMode = "azure.cosmosdb.connection.mode";

/**
  Account or request <a
  href="https://learn.microsoft.com/azure/cosmos-db/consistency-levels">consistency level</a>.
 */
static constexpr const char *kAzureCosmosdbConsistencyLevel = "azure.cosmosdb.consistency.level";

/**
  List of regions contacted during operation in the order that they were contacted. If there is more
  than one region listed, it indicates that the operation was performed on multiple regions i.e.
  cross-regional call. <p> Region name matches the format of @code displayName @endcode in <a
  href="https://learn.microsoft.com/rest/api/subscription/subscriptions/list-locations?view=rest-subscription-2021-10-01&tabs=HTTP#location">Azure
  Location API</a>
 */
static constexpr const char *kAzureCosmosdbOperationContactedRegions =
    "azure.cosmosdb.operation.contacted_regions";

/**
  The number of request units consumed by the operation.
 */
static constexpr const char *kAzureCosmosdbOperationRequestCharge =
    "azure.cosmosdb.operation.request_charge";

/**
  Request payload size in bytes.
 */
static constexpr const char *kAzureCosmosdbRequestBodySize = "azure.cosmosdb.request.body.size";

/**
  Cosmos DB sub status code.
 */
static constexpr const char *kAzureCosmosdbResponseSubStatusCode =
    "azure.cosmosdb.response.sub_status_code";

namespace AzureCosmosdbConnectionModeValues
{
/**
  Gateway (HTTP) connection.
 */
static constexpr const char *kGateway = "gateway";

/**
  Direct connection.
 */
static constexpr const char *kDirect = "direct";

}  // namespace AzureCosmosdbConnectionModeValues

namespace AzureCosmosdbConsistencyLevelValues
{
/**
  none
 */
static constexpr const char *kStrong = "Strong";

/**
  none
 */
static constexpr const char *kBoundedStaleness = "BoundedStaleness";

/**
  none
 */
static constexpr const char *kSession = "Session";

/**
  none
 */
static constexpr const char *kEventual = "Eventual";

/**
  none
 */
static constexpr const char *kConsistentPrefix = "ConsistentPrefix";

}  // namespace AzureCosmosdbConsistencyLevelValues

}  // namespace azure
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
