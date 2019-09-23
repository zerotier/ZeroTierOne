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
	"runtime"
)

// LocalConfigPhysicalPathConfiguration contains settings for physical paths
type LocalConfigPhysicalPathConfiguration struct {
	Blacklist     bool
	TrustedPathID uint64
}

// LocalConfigVirtualAddressConfiguration contains settings for virtual addresses
type LocalConfigVirtualAddressConfiguration struct {
	Try []net.Addr
}

// LocalConfigSettings contains node settings
type LocalConfigSettings struct {
	PrimaryPort              int
	SecondaryPort            int
	TertiaryPort             int
	PortMappingEnabled       bool
	MuiltipathMode           int
	InterfacePrefixBlacklist []string
}

// LocalConfig is the local.conf file and stores local settings for the node.
type LocalConfig struct {
	Physical map[string]LocalConfigPhysicalPathConfiguration
	Virtual  map[Address]LocalConfigVirtualAddressConfiguration
	Settings LocalConfigSettings
}

// NewLocalConfig creates a new local.conf file with defaults
func NewLocalConfig() *LocalConfig {
	lc := &LocalConfig{
		Physical: make(map[string]LocalConfigPhysicalPathConfiguration),
		Virtual:  make(map[Address]LocalConfigVirtualAddressConfiguration),
		Settings: LocalConfigSettings{
			PrimaryPort:        9993,
			SecondaryPort:      0,
			TertiaryPort:       0,
			PortMappingEnabled: true,
			MuiltipathMode:     0,
		},
	}
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
	return lc
}
