/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#include "../osdep/WindowsIpInterface.hpp"

#include <stdexcept>
#include <stdio.h>

using namespace ZeroTier;

static const unsigned char TAP_MAC[6] = { 0x12, 0x5c, 0x36, 0xf1, 0x97, 0x1f };

static void check(bool condition, const char* message)
{
	if (! condition)
		throw std::runtime_error(message);
}

static IP_ADAPTER_ADDRESSES adapter(uint64_t luid, const unsigned char* mac, ULONG macLength = 6, IFTYPE type = IF_TYPE_ETHERNET_CSMACD)
{
	IP_ADAPTER_ADDRESSES value;
	memset(&value, 0, sizeof(value));
	value.Luid.Value = luid;
	value.IfType = type;
	value.OperStatus = IfOperStatusUp;
	value.PhysicalAddressLength = macLength;
	if (mac)
		memcpy(value.PhysicalAddress, mac, (macLength <= sizeof(value.PhysicalAddress)) ? macLength : sizeof(value.PhysicalAddress));
	return value;
}

int main()
{
	try {
		NET_LUID tapLuid;
		tapLuid.Value = 3;
		NET_LUID resolved;

		IP_ADAPTER_ADDRESSES direct = adapter(tapLuid.Value, TAP_MAC);
		check(resolveIpInterfaceLuid(tapLuid, TAP_MAC, &direct, resolved), "direct TAP was not resolved");
		check(resolved.Value == tapLuid.Value, "direct TAP resolved to another interface");

		IP_ADAPTER_ADDRESSES management = adapter(18, TAP_MAC);
		check(resolveIpInterfaceLuid(tapLuid, TAP_MAC, &management, resolved), "unique management interface was not resolved");
		check(resolved.Value == 18, "wrong management interface selected");

		IP_ADAPTER_ADDRESSES duplicate = adapter(19, TAP_MAC);
		management.Next = &duplicate;
		check(! resolveIpInterfaceLuid(tapLuid, TAP_MAC, &management, resolved), "ambiguous management interfaces were accepted");

		management.Next = NULL;
		management.IfType = IF_TYPE_SOFTWARE_LOOPBACK;
		check(! resolveIpInterfaceLuid(tapLuid, TAP_MAC, &management, resolved), "non-Ethernet interface was accepted");

		management = adapter(18, TAP_MAC, 5);
		check(! resolveIpInterfaceLuid(tapLuid, TAP_MAC, &management, resolved), "wrong-length MAC was accepted");

		const unsigned char otherMac[6] = { 0, 1, 2, 3, 4, 5 };
		management = adapter(18, otherMac);
		check(! resolveIpInterfaceLuid(tapLuid, TAP_MAC, &management, resolved), "different MAC was accepted");

		management = adapter(18, TAP_MAC);
		management.OperStatus = IfOperStatusNotPresent;
		check(! resolveIpInterfaceLuid(tapLuid, TAP_MAC, &management, resolved), "missing interface was accepted");

		direct.Next = &management;
		check(resolveIpInterfaceLuid(tapLuid, TAP_MAC, &direct, resolved), "direct TAP lost precedence");
		check(resolved.Value == tapLuid.Value, "management interface overrode direct TAP");

		puts("Windows IP interface resolution regression passed");
		return 0;
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "Windows IP interface resolution regression failed: %s\n", ex.what());
		return 1;
	}
}
