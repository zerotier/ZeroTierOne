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

import (
	"net"
	"sync/atomic"
	"time"
)

// NetworkConfig represents the network's current state
type NetworkConfig struct {
	// ID is this network's 64-bit globally unique identifier
	ID uint64

	// MAC is the Ethernet MAC address of this device on this network
	MAC MAC

	// Name is a short human-readable name set by the controller
	Name string

	// Status is a status code indicating this network's authorization status
	Status int

	// LastUpdated is the time this network's configuration was last updated from the controller
	LastUpdated time.Time

	// Type is this network's type
	Type int

	// MTU is the Ethernet MTU for this network
	MTU int

	// CanBridge is true if this network is allowed to bridge in other devices with different Ethernet addresses
	CanBridge bool

	// AllowsBroadcast is true if the broadcast (ff:ff:ff:ff:ff:ff) address works (excluding IPv4 ARP which is handled via a special path)
	AllowsBroadcast bool

	// IPs are static IPs assigned by the network controller to this device
	IPs []net.IPNet

	// Routes are static routes assigned by the network controller to this device
	Routes []Route

	// MulticastSubscriptions are this device's current multicast subscriptions
	MulticastSubscriptions []MulticastGroup

	// PortType is a human-readable description of this port's implementation type or name
	PortType string

	// PortDeviceName is the OS-specific device name (e.g. tun0 or feth1856) for this network's virtual port
	PortDeviceName string

	// PortErrorCode is an OS-specific error code returned by the virtual NIC driver
	PortErrorCode int
}

// Network is a currently joined network
type Network struct {
	config atomic.Value
	tap    atomic.Value
}
