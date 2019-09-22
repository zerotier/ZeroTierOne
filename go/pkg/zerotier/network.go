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
	return []byte("\"" + n.String() + "\""), nil
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

	// Bridge is true if this network is allowed to bridge in other devices with different Ethernet addresses
	Bridge bool

	// BroadcastEnabled is true if the broadcast (ff:ff:ff:ff:ff:ff) address works (excluding IPv4 ARP which is handled via a special path)
	BroadcastEnabled bool

	// NetconfRevision is the revision number reported by the controller
	NetconfRevision uint64

	// AssignedAddresses are static IPs assigned by the network controller to this device
	AssignedAddresses []net.IPNet

	// Routes are static routes assigned by the network controller to this device
	Routes []Route
}

// NetworkLocalSettings is settings for this network that can be changed locally
type NetworkLocalSettings struct {
	// AllowManagedIPs determines whether managed IP assignment is allowed
	AllowManagedIPs bool

	// AllowGlobalIPs determines if managed IPs that overlap with public Internet addresses are allowed
	AllowGlobalIPs bool

	// AllowManagedRoutes determines whether managed routes can be set
	AllowManagedRoutes bool

	// AllowGlobalRoutes determines if managed routes can overlap with public Internet addresses
	AllowGlobalRoutes bool

	// AllowDefaultRouteOverride determines if the default (0.0.0.0 or ::0) route on the system can be overridden ("full tunnel" mode)
	AllowDefaultRouteOverride bool
}

// Network is a currently joined network
type Network struct {
	id         NetworkID
	tap        Tap
	config     NetworkConfig
	settings   NetworkLocalSettings // locked by configLock
	configLock sync.RWMutex
}

// NewNetwork creates a new network with default settings
func NewNetwork(id NetworkID, t Tap) (*Network, error) {
	return &Network{
		id:  id,
		tap: t,
		config: NetworkConfig{
			ID:     id,
			Status: NetworkStatusRequestConfiguration,
		},
		settings: NetworkLocalSettings{
			AllowManagedIPs:           true,
			AllowGlobalIPs:            false,
			AllowManagedRoutes:        true,
			AllowGlobalRoutes:         false,
			AllowDefaultRouteOverride: false,
		},
	}, nil
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
func (n *Network) Tap() Tap { return n.tap }

// SetLocalSettings modifies this network's local settings
func (n *Network) SetLocalSettings(ls *NetworkLocalSettings) { n.updateConfig(nil, ls) }

func (n *Network) networkConfigRevision() uint64 {
	n.configLock.RLock()
	defer n.configLock.RUnlock()
	return n.config.NetconfRevision
}

func networkManagedIPAllowed(ip net.IP, ls *NetworkLocalSettings) bool {
	if !ls.AllowManagedIPs {
		return false
	}
	switch ipClassify(ip) {
	case ipClassificationNone, ipClassificationLoopback, ipClassificationLinkLocal, ipClassificationMulticast:
		return false
	case ipClassificationGlobal:
		return ls.AllowGlobalIPs
	}
	return true
}

func networkManagedRouteAllowed(r *Route, ls *NetworkLocalSettings) bool {
	if !ls.AllowManagedRoutes {
		return false
	}
	bits, _ := r.Target.Mask.Size()
	if len(r.Target.IP) > 0 && allZero(r.Target.IP) && bits == 0 {
		return ls.AllowDefaultRouteOverride
	}
	switch ipClassify(r.Target.IP) {
	case ipClassificationNone, ipClassificationLoopback, ipClassificationLinkLocal, ipClassificationMulticast:
		return false
	case ipClassificationGlobal:
		return ls.AllowGlobalRoutes
	}
	return true
}

func (n *Network) updateConfig(nc *NetworkConfig, ls *NetworkLocalSettings) {
	n.configLock.Lock()
	defer n.configLock.Unlock()

	if n.tap == nil { // sanity check, should never happen
		return
	}

	if nc == nil {
		nc = &n.config
	}
	if ls == nil {
		ls = &n.settings
	}

	// Add IPs to tap that are newly assigned in this config update,
	// and remove any IPs from the tap that were assigned that are no
	// longer wanted. IPs assigned to the tap externally (e.g. by an
	// "ifconfig" command) are left alone.
	haveAssignedIPs := make(map[[3]uint64]*net.IPNet)
	wantAssignedIPs := make(map[[3]uint64]bool)
	if n.settings.AllowManagedIPs {
		for _, ip := range n.config.AssignedAddresses {
			if networkManagedIPAllowed(ip.IP, &n.settings) { // was it allowed?
				haveAssignedIPs[ipNetToKey(&ip)] = &ip
			}
		}
	}
	if ls.AllowManagedIPs {
		for _, ip := range nc.AssignedAddresses {
			if networkManagedIPAllowed(ip.IP, ls) { // should it be allowed now?
				k := ipNetToKey(&ip)
				wantAssignedIPs[k] = true
				if _, have := haveAssignedIPs[k]; !have {
					n.tap.AddIP(&ip)
				}
			}
		}
	}
	for k, ip := range haveAssignedIPs {
		if _, want := wantAssignedIPs[k]; !want {
			n.tap.RemoveIP(ip)
		}
	}

	// Do the same for managed routes
	haveManagedRoutes := make(map[[6]uint64]*Route)
	wantManagedRoutes := make(map[[6]uint64]bool)
	if n.settings.AllowManagedRoutes {
		for _, r := range n.config.Routes {
			if networkManagedRouteAllowed(&r, &n.settings) { // was it allowed?
				haveManagedRoutes[r.key()] = &r
			}
		}
	}
	if ls.AllowManagedRoutes {
		for _, r := range nc.Routes {
			if networkManagedRouteAllowed(&r, ls) { // should it be allowed now?
				k := r.key()
				wantManagedRoutes[k] = true
				if _, have := haveManagedRoutes[k]; !have {
					n.tap.AddRoute(&r)
				}
			}
		}
	}
	for k, r := range haveManagedRoutes {
		if _, want := wantManagedRoutes[k]; !want {
			n.tap.RemoveRoute(r)
		}
	}

	if nc != &n.config {
		n.config = *nc
	}
	if ls != &n.settings {
		n.settings = *ls
	}
}
