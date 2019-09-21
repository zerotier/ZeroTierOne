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
	"encoding/json"
	"fmt"
	"net"
	"strconv"
	"sync"
)

// NetworkID is a network's 64-bit unique ID
type NetworkID uint64

// NewNetworkIDFromString parses a network ID in string form
func NewNetworkIDFromString(s string) (NetworkID, error) {
	if len(s) != 16 {
		return NetworkID(0), ErrInvalidZeroTierAddress
	}
	n, err := strconv.ParseUint(s, 16, 64)
	return NetworkID(n), err
}

// String returns this network ID's 16-digit hex identifier
func (n NetworkID) String() string {
	return fmt.Sprintf("%.16x", uint64(n))
}

// MarshalJSON marshals this NetworkID as a string
func (n NetworkID) MarshalJSON() ([]byte, error) {
	return []byte(n.String()), nil
}

// UnmarshalJSON unmarshals this NetworkID from a string
func (n *NetworkID) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}
	tmp, err := NewNetworkIDFromString(s)
	*n = tmp
	return err
}

// NetworkConfig represents the network's current state
type NetworkConfig struct {
	// ID is this network's 64-bit globally unique identifier
	ID NetworkID

	// MAC is the Ethernet MAC address of this device on this network
	MAC MAC

	// Name is a short human-readable name set by the controller
	Name string

	// Status is a status code indicating this network's authorization status
	Status int

	// Type is this network's type
	Type int

	// MTU is the Ethernet MTU for this network
	MTU int

	// CanBridge is true if this network is allowed to bridge in other devices with different Ethernet addresses
	Bridge bool

	// BroadcastEnabled is true if the broadcast (ff:ff:ff:ff:ff:ff) address works (excluding IPv4 ARP which is handled via a special path)
	BroadcastEnabled bool

	// Network configuration revision number according to network controller
	NetconfRevision uint64

	// AssignedAddresses are static IPs assigned by the network controller to this device
	AssignedAddresses []net.IPNet

	// Routes are static routes assigned by the network controller to this device
	Routes []Route
}

// Network is a currently joined network
type Network struct {
	id         NetworkID
	config     NetworkConfig
	tap        Tap
	configLock sync.RWMutex
	tapLock    sync.RWMutex
}

// ID gets this network's unique ID
func (n *Network) ID() NetworkID { return n.id }

// Config returns a copy of this network's current configuration
func (n *Network) Config() NetworkConfig {
	n.configLock.RLock()
	defer n.configLock.RUnlock()
	return n.config
}

// Tap gets this network's tap device
func (n *Network) Tap() Tap {
	n.tapLock.RLock()
	defer n.tapLock.RUnlock()
	return n.tap
}
