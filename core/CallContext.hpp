/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
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

#ifndef ZT_CALLCONTEXT_HPP
#define ZT_CALLCONTEXT_HPP

#include "Constants.hpp"

namespace ZeroTier {

/**
 * A per-API-call equivalent to the general context.
 *
 * This is created when external C API calls are made and follows the call
 * graph around from function to function as needed. It's cleaner and probably
 * faster than passing clock, ticks, and tPtr around everywhere.
 */
class CallContext {
  public:
    ZT_INLINE CallContext(const int64_t c, const int64_t t, void* const p) : clock(c), ticks(t), tPtr(p)
    {
    }

    /**
     * Real world time in milliseconds since Unix epoch or -1 if unknown.
     *
     * This is used for things like checking certificate expiration. If it's
     * not known then the value may be inferred from peers/roots or some
     * features may be disabled.
     */
    const int64_t clock;

    /**
     * Monotonic process or system clock in milliseconds since an arbitrary point.
     *
     * This is never -1 or undefined and is used for most timings.
     */
    const int64_t ticks;

    /**
     * An arbitrary pointer users pass into calls that follows the call chain
     *
     * By passing this back to callbacks state can be kept by the caller using
     * a mechanism that is faster (on most platforms) than thread-local storage.
     */
    void* const tPtr;
};

}   // namespace ZeroTier

#endif
