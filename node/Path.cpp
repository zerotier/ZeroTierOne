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

#include "Path.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"

namespace ZeroTier {

bool Path::send(const RuntimeEnvironment *RR,void *tPtr,const void *data,unsigned int len,int64_t now)
{
	if (RR->node->putPacket(tPtr,_localSocket,_addr,data,len)) {
		_lastOut = now;
		return true;
	}
	return false;
}

} // namespace ZeroTier
