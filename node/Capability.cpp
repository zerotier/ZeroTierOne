/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Capability.hpp"

namespace ZeroTier {

bool Capability::sign(const Identity &from,const Address &to)
{
	try {
		for(unsigned int i=0;((i<_maxCustodyChainLength)&&(i<ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH));++i) {
			if (!(_custody[i].to)) {
				Buffer<(sizeof(Capability) * 2)> tmp;
				this->serialize(tmp,true);
				_custody[i].to = to;
				_custody[i].from = from.address();
				_custody[i].signatureLength = from.sign(tmp.data(),tmp.size(),_custody[i].signature,sizeof(_custody[i].signature));
				return true;
			}
		}
	} catch ( ... ) {}
	return false;
}

} // namespace ZeroTier
