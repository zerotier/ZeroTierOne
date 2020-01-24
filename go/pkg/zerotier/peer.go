/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
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

// Peer is another ZeroTier node
type Peer struct {
	Address      Address      `json:"address"`
	Identity     *Identity    `json:"identity"`
	IdentityHash string       `json:"identityHash"`
	Version      [3]int       `json:"version"`
	Latency      int          `json:"latency"`
	Root         bool         `json:"root"`
	Bootstrap    *InetAddress `json:"bootstrap,omitempty"`
	Paths        []Path       `json:"paths,omitempty"`
}

// PeerMutableFields contains only the mutable fields of Peer as nullable pointers.
type PeerMutableFields struct {
	Identity  *Identity    `json:"identity"`
	Root      *bool        `json:"root"`
	Bootstrap *InetAddress `json:"bootstrap,omitempty"`
}
