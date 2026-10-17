/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifndef ZT_WINDOWSTAPDEVICESTATE_HPP
#define ZT_WINDOWSTAPDEVICESTATE_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <string.h>

namespace ZeroTier {

template <typename QueryInterface, typename EnableDevice>
bool enableTapIfAdministrativelyDown(const NET_LUID& luid, QueryInterface queryInterface, EnableDevice enableDevice)
{
	MIB_IF_ROW2 row;
	memset(&row, 0, sizeof(row));
	row.InterfaceLuid = luid;
	if ((queryInterface(&row) == NO_ERROR) && (row.AdminStatus == NET_IF_ADMIN_STATUS_DOWN)) {
		enableDevice();
		return true;
	}
	return false;
}

} // namespace ZeroTier

#endif
