/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifdef ZT_USE_MINIUPNPC

#ifndef ZT_PORTMAPPER_HPP
#define ZT_PORTMAPPER_HPP

#include "../node/Constants.hpp"
#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"
#include "Thread.hpp"

#include <vector>

/**
 * How frequently should we refresh our UPNP/NAT-PnP/whatever state?
 */
#define ZT_PORTMAPPER_REFRESH_DELAY 300000

namespace ZeroTier {

class PortMapperImpl;

/**
 * UPnP/NAT-PnP port mapping "daemon"
 */
class PortMapper {
    friend class PortMapperImpl;

  public:
    /**
     * Create and start port mapper service
     *
     * @param localUdpPortToMap Port we want visible to the outside world
     * @param name Unique name of this endpoint (based on ZeroTier address)
     */
    PortMapper(int localUdpPortToMap, const char* uniqueName);

    ~PortMapper();

    /**
     * @return All current external mappings for our port
     */
    std::vector<InetAddress> get() const;

  private:
    PortMapperImpl* _impl;
};

}   // namespace ZeroTier

#endif

#endif   // ZT_USE_MINIUPNPC
