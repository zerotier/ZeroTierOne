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
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"

namespace ZeroTier {

bool Path::send(const RuntimeEnvironment *const RR, void *const tPtr, const void *const data, const unsigned int len, const int64_t now) noexcept
{
	if (likely(RR->cb.wirePacketSendFunction(reinterpret_cast<ZT_Node *>(RR->node), RR->uPtr, tPtr, m_localSocket, reinterpret_cast<const ZT_InetAddress *>(&m_addr), data, len, 0) == 0)) {
		m_lastOut = now;
		m_outMeter.log(now, len);
		return true;
	}
	return false;
}

} // namespace ZeroTier
