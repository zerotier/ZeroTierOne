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
namespace dns
{

/**
  The name being queried.
  <p>
  If the name field contains non-printable characters (below 32 or above 126), those characters
  should be represented as escaped base 10 integers (\DDD). Back slashes and quotes should be
  escaped. Tabs, carriage returns, and line feeds should be converted to \t, \r, and \n
  respectively.
 */
static constexpr const char *kDnsQuestionName = "dns.question.name";

}  // namespace dns
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
