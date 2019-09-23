/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

import "net"

// Tap represents an Ethernet tap connecting a virtual network to a device or something else "real"
type Tap interface {
	Type() string
	Error() (int, string)
	SetEnabled(enabled bool)
	Enabled() bool
	AddIP(ip *net.IPNet) error
	RemoveIP(ip *net.IPNet) error
	IPs() ([]net.IPNet, error)
	DeviceName() string
	AddMulticastGroupChangeHandler(func(bool, *MulticastGroup))
	AddRoute(r *Route) error
	RemoveRoute(r *Route) error
}
