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
	IP                     net.IP  `json:"ip"`
	Port                   int     `json:"port"`
	LastSend               int64   `json:"lastSend"`
	LastReceive            int64   `json:"lastReceive"`
	TrustedPathID          uint64  `json:"trustedPathID"`
	Latency                float32 `json:"latency"`
	PacketDelayVariance    float32 `json:"packetDelayVariance"`
	ThroughputDisturbCoeff float32 `json:"throughputDisturbCoeff"`
	PacketErrorRatio       float32 `json:"packetErrorRatio"`
	PacketLossRatio        float32 `json:"packetLossRatio"`
	Stability              float32 `json:"stability"`
	Throughput             uint64  `json:"throughput"`
	MaxThroughput          uint64  `json:"maxThroughput"`
	Allocation             float32 `json:"allocation"`
}
