/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
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

package zerotier

import "net"

// Tap represents an Ethernet tap connecting a virtual network to a device or something else "real"
type Tap interface {
	// Close is called when this tap is being shut down
	Close()

	// Type is a string describing what kind of tap this is e.g. "native" for OS-native
	Type() string

	// Error returns the most recent error experienced by this tap
	Error() (int, string)

	// SetEnabled sets whether this tap will accept and process packets
	SetEnabled(enabled bool)

	// Enabled returns the current enabled status
	Enabled() bool

	// AddIP assigns an IP address to this tap device
	AddIP(ip *net.IPNet) error

	// RemoveIP removes an IP address from this tap
	RemoveIP(ip *net.IPNet) error

	// IPs returns an array of all IPs currently assigned to this tap including those not assigned by ZeroTier
	IPs() ([]net.IPNet, error)

	// DeviceName gets the OS-specific device name for this tap or an empty string if none
	DeviceName() string

	// AddMulticastGroupChangeHandler registers a function to be called on multicast group subscribe or unsubscribe (first argument)
	AddMulticastGroupChangeHandler(func(bool, *MulticastGroup))
}
