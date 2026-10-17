/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#include "../osdep/WindowsTapDeviceState.hpp"

#include <stdexcept>
#include <stdio.h>

using namespace ZeroTier;

static void check(bool condition, const char* message)
{
	if (! condition)
		throw std::runtime_error(message);
}

static void checkState(DWORD queryResult, NET_IF_ADMIN_STATUS adminStatus, bool expectedEnable)
{
	NET_LUID luid;
	luid.Value = 0x123456789abcdef0ULL;
	unsigned int enableCount = 0;
	const bool enableRequested = enableTapIfAdministrativelyDown(
		luid,
		[&](PMIB_IF_ROW2 row) {
			check(row->InterfaceLuid.Value == luid.Value, "query used the wrong interface LUID");
			check(row->InterfaceIndex == 0, "query row was not initialized");
			row->AdminStatus = adminStatus;
			return queryResult;
		},
		[&]() { ++enableCount; });

	check(enableRequested == expectedEnable, "wrong enable decision");
	check(enableCount == (expectedEnable ? 1U : 0U), "wrong number of enable requests");
}

static void checkHealth(DWORD queryResult, IF_OPER_STATUS operStatus, WindowsTapHealth expectedHealth)
{
	NET_LUID luid;
	luid.Value = 0x123456789abcdef0ULL;
	const WindowsTapHealth health = getTapHealth(luid, [&](PMIB_IF_ROW2 row) {
		check(row->InterfaceLuid.Value == luid.Value, "health query used the wrong interface LUID");
		check(row->InterfaceIndex == 0, "health query row was not initialized");
		row->OperStatus = operStatus;
		return queryResult;
	});
	check(health == expectedHealth, "wrong TAP health classification");
}

int main()
{
	try {
		checkState(NO_ERROR, NET_IF_ADMIN_STATUS_UP, false);
		checkState(NO_ERROR, NET_IF_ADMIN_STATUS_DOWN, true);
		checkState(NO_ERROR, NET_IF_ADMIN_STATUS_TESTING, false);
		checkState(ERROR_FILE_NOT_FOUND, NET_IF_ADMIN_STATUS_DOWN, false);
		checkState(ERROR_ACCESS_DENIED, NET_IF_ADMIN_STATUS_DOWN, false);

		checkHealth(NO_ERROR, IfOperStatusUp, WindowsTapHealth::Healthy);
		checkHealth(NO_ERROR, IfOperStatusDown, WindowsTapHealth::Unhealthy);
		checkHealth(NO_ERROR, IfOperStatusTesting, WindowsTapHealth::Unhealthy);
		checkHealth(NO_ERROR, IfOperStatusUnknown, WindowsTapHealth::Unhealthy);
		checkHealth(NO_ERROR, IfOperStatusDormant, WindowsTapHealth::Unhealthy);
		checkHealth(NO_ERROR, IfOperStatusNotPresent, WindowsTapHealth::Unhealthy);
		checkHealth(NO_ERROR, IfOperStatusLowerLayerDown, WindowsTapHealth::Unhealthy);
		checkHealth(ERROR_FILE_NOT_FOUND, IfOperStatusDown, WindowsTapHealth::Unknown);
		checkHealth(ERROR_ACCESS_DENIED, IfOperStatusDown, WindowsTapHealth::Unknown);
		puts("Windows TAP device state regression passed");
		return 0;
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "Windows TAP device state regression failed: %s\n", ex.what());
		return 1;
	}
}
