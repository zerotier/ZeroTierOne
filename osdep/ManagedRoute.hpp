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

#ifndef ZT_MANAGEDROUTE_HPP
#define ZT_MANAGEDROUTE_HPP

#include "../node/AtomicCounter.hpp"
#include "../node/InetAddress.hpp"
#include "../node/SharedPtr.hpp"
#include "../node/Utils.hpp"

#include <map>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace ZeroTier {

/**
 * A ZT-managed route that used C++ RAII semantics to automatically clean itself up on deallocate
 */
class ManagedRoute {
    friend class SharedPtr<ManagedRoute>;

  public:
    ManagedRoute(const InetAddress& target, const InetAddress& via, const InetAddress& src, const char* device);
    ~ManagedRoute();

    /**
     * Set or update currently set route
     *
     * This must be called periodically for routes that shadow others so that
     * shadow routes can be updated. In some cases it has no effect
     *
     * @return True if route add/update was successful
     */
    bool sync();

    /**
     * Remove and clear this ManagedRoute
     *
     * This does nothing if this ManagedRoute is not set or has already been
     * removed. If this is not explicitly called it is called automatically on
     * destruct.
     */
    void remove();

    inline const InetAddress& target() const
    {
        return _target;
    }
    inline const InetAddress& via() const
    {
        return _via;
    }
    inline const InetAddress& src() const
    {
        return _src;
    }
    inline const char* device() const
    {
        return _device;
    }

  private:
    ManagedRoute(const ManagedRoute&)
    {
    }
    inline ManagedRoute& operator=(const ManagedRoute&)
    {
        return *this;
    }

    InetAddress _target;
    InetAddress _via;
    InetAddress _src;
    InetAddress _systemVia;                 // for route overrides
    std::map<InetAddress, bool> _applied;   // routes currently applied
    char _device[128];
    char _systemDevice[128];   // for route overrides

    AtomicCounter __refCount;
};

}   // namespace ZeroTier

#endif
