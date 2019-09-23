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
	"net"
	"os"
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
	Physical map[string]*LocalConfigPhysicalPathConfiguration
	Virtual  map[Address]*LocalConfigVirtualAddressConfiguration
	Network  map[NetworkID]*NetworkLocalSettings
	Settings LocalConfigSettings
}

// Read this local config from a file, initializing to defaults if the file does not exist
func (lc *LocalConfig) Read(p string) error {
	if lc.Physical == nil {
		lc.Physical = make(map[string]*LocalConfigPhysicalPathConfiguration)
		lc.Virtual = make(map[Address]*LocalConfigVirtualAddressConfiguration)
		lc.Network = make(map[NetworkID]*NetworkLocalSettings)
		lc.Settings.PrimaryPort = 9993
		lc.Settings.SecondaryPort = 16384 + (rand.Int() % 16384)
		lc.Settings.TertiaryPort = 32768 + (rand.Int() % 16384)
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
	if err != nil && err != os.ErrNotExist {
		return err
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
