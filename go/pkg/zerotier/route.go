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

// Route represents a route in a host's routing table
type Route struct {
	// Target for this route
	Target net.IPNet

	// Via is how to reach this target (null/empty if the target IP range is local to this virtual LAN)
	Via net.IP

	// Metric is an interface metric that can affect route priority (behavior can be OS-specific)
	Metric int
}
