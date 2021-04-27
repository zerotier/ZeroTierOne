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

#include "Path.hpp"

#include "Context.hpp"
#include "Node.hpp"

namespace ZeroTier {

bool Path::send(const Context& ctx, const CallContext& cc, const void* const data, const unsigned int len) noexcept
{
    if (likely(ctx.cb.wirePacketSendFunction(reinterpret_cast<ZT_Node*>(ctx.node), ctx.uPtr, cc.tPtr, m_localSocket, reinterpret_cast<const ZT_InetAddress*>(&m_addr), data, len, 0) == 0)) {
        m_lastOut = cc.ticks;
        m_outMeter.log(cc.ticks, len);
        return true;
    }
    return false;
}

}   // namespace ZeroTier
