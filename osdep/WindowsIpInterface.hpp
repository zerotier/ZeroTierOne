/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifndef ZT_WINDOWSIPINTERFACE_HPP
#define ZT_WINDOWSIPINTERFACE_HPP

#include "WindowsTapDeviceState.hpp"

#include <string.h>

namespace ZeroTier {

inline bool resolveIpInterfaceLuid(
	const NET_LUID& tapLuid,
	const unsigned char* tapMac,
	PIP_ADAPTER_ADDRESSES adapters,
	NET_LUID& ipLuid)
{
	NET_LUID candidate;
	candidate.Value = 0;
	unsigned int matches = 0;
	for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter; adapter = adapter->Next) {
		if (adapter->Luid.Value == tapLuid.Value) {
			ipLuid = tapLuid;
			return true;
		}
		if ((adapter->IfType != IF_TYPE_ETHERNET_CSMACD) || (adapter->PhysicalAddressLength != 6) ||
			(adapter->OperStatus == IfOperStatusNotPresent) || memcmp(adapter->PhysicalAddress, tapMac, 6)) {
			continue;
		}
		candidate = adapter->Luid;
		++matches;
	}
	if (matches == 1) {
		ipLuid = candidate;
		return true;
	}
	return false;
}

} // namespace ZeroTier

#endif
