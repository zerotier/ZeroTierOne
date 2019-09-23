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
	"io/ioutil"
	rand "math/rand"
	"os"
	"runtime"
)

// LocalConfigPhysicalPathConfiguration contains settings for physical paths
type LocalConfigPhysicalPathConfiguration struct {
	// Blacklist flags this path as unusable for ZeroTier traffic
	Blacklist bool

	// TrustedPathID identifies a path for unencrypted/unauthenticated traffic
	TrustedPathID uint64
}

// LocalConfigVirtualAddressConfiguration contains settings for virtual addresses
type LocalConfigVirtualAddressConfiguration struct {
	// Try is a list of IPs/ports to try for this peer in addition to anything learned from roots or direct path push
	Try []*InetAddress
}

// LocalConfigSettings contains node settings
type LocalConfigSettings struct {
	// PrimaryPort is the main UDP port and must be set (defaults to 9993)
	PrimaryPort int

	// SecondaryPort is the secondary UDP port, set to 0 to disbale (picked at random by default)
	SecondaryPort int

	// TertiaryPort is a third UDP port, set to 0 to disable (picked at random by default)
	TertiaryPort int

	// PortAutoSearch causes ZeroTier to try other ports automatically if it can't bind to configured ports
	PortAutoSearch bool

	// PortMappingEnabled enables uPnP and NAT-PMP support
	PortMappingEnabled bool

	// MultipathMode sets the multipath link aggregation mode
	MuiltipathMode int

	// InterfacePrefixBlacklist are prefixes of physical network interface names that won't be used by ZeroTier (e.g. "lo" or "utun")
	InterfacePrefixBlacklist []string

	// ExplicitAddresses are explicit IP/port addresses to advertise to other nodes, such as externally mapped ports on a router
	ExplicitAddresses []*InetAddress
}

// LocalConfig is the local.conf file and stores local settings for the node.
type LocalConfig struct {
	// Physical path configurations by CIDR IP/bits
	Physical map[string]*LocalConfigPhysicalPathConfiguration

	// Virtual node specific configurations by 10-digit hex ZeroTier address
	Virtual map[Address]*LocalConfigVirtualAddressConfiguration

	// Network local configurations by 16-digit hex ZeroTier network ID
	Network map[NetworkID]*NetworkLocalSettings

	// LocalConfigSettings contains other local settings for this node
	Settings LocalConfigSettings
}

// Read this local config from a file, initializing to defaults if the file does not exist
func (lc *LocalConfig) Read(p string, saveDefaultsIfNotExist bool) error {
	if lc.Physical == nil {
		lc.Physical = make(map[string]*LocalConfigPhysicalPathConfiguration)
		lc.Virtual = make(map[Address]*LocalConfigVirtualAddressConfiguration)
		lc.Network = make(map[NetworkID]*NetworkLocalSettings)
		lc.Settings.PrimaryPort = 9993
		lc.Settings.SecondaryPort = 16384 + (rand.Int() % 16384)
		lc.Settings.TertiaryPort = 32768 + (rand.Int() % 16384)
		lc.Settings.PortAutoSearch = true
		lc.Settings.PortMappingEnabled = true
		lc.Settings.MuiltipathMode = 0
		switch runtime.GOOS {
		case "darwin":
			lc.Settings.InterfacePrefixBlacklist = []string{"utun", "tun", "tap", "feth", "lo", "zt"}
		case "linux":
			lc.Settings.InterfacePrefixBlacklist = []string{"tun", "tap", "lo", "zt"}
		case "freebsd", "openbsd", "netbsd", "illumos", "solaris", "dragonfly":
			lc.Settings.InterfacePrefixBlacklist = []string{"tun", "tap", "zt"}
		case "android":
			lc.Settings.InterfacePrefixBlacklist = []string{"tun", "tap"}
		}
	}

	data, err := ioutil.ReadFile(p)
	if err != nil {
		if err != os.ErrNotExist {
			return err
		}
		if saveDefaultsIfNotExist {
			err = lc.Write(p)
			if err != nil {
				return err
			}
		}
		return nil
	}
	return json.Unmarshal(data, lc)
}

// Write this local config to a file
func (lc *LocalConfig) Write(p string) error {
	data, err := json.MarshalIndent(lc, "", "\t")
	if err != nil {
		return err
	}
	return ioutil.WriteFile(p, data, 0644)
}
