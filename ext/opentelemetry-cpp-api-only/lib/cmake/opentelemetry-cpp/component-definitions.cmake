# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Configured from opentelemetry-cpp/cmake/component-definitions.cmake.in

# ----------------------------------------------------------------------
# opentelmetry-cpp Built COMPONENT list
# ----------------------------------------------------------------------
set(OTEL_BUILT_COMPONENTS_LIST api)

# ----------------------------------------------------------------------
# COMPONENT to TARGET lists
# ----------------------------------------------------------------------

# COMPONENT api
set(COMPONENT_api_TARGETS
    opentelemetry-cpp::api
)



#-----------------------------------------------------------------------
# COMPONENT to COMPONENT dependencies
#-----------------------------------------------------------------------

# COMPONENT api internal dependencies
set(COMPONENT_api_COMPONENT_DEPENDS
)




#-----------------------------------------------------------------------
# COMPONENT to THIRDPARTY dependencies
#-----------------------------------------------------------------------

# COMPONENT api thirdparty dependencies
set(COMPONENT_api_THIRDPARTY_DEPENDS
)


