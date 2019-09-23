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

// Path is a path to another peer on the network
type Path struct {
	IP                     net.IP
	Port                   int
	LastSend               int64
	LastReceive            int64
	TrustedPathID          uint64
	Latency                float32
	PacketDelayVariance    float32
	ThroughputDisturbCoeff float32
	PacketErrorRatio       float32
	PacketLossRatio        float32
	Stability              float32
	Throughput             uint64
	MaxThroughput          uint64
	Allocation             float32
}
