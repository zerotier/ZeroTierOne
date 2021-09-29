/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_FLOW_HPP
#define ZT_FLOW_HPP

#include "Path.hpp"
#include "SharedPtr.hpp"

namespace ZeroTier {

/**
 * A protocol flow that is identified by the origin and destination port.
 */
struct Flow
{
	/**
	 * @param flowId Given flow ID
	 * @param now Current time
	 */
	Flow(int32_t flowId, int64_t now) :
		_flowId(flowId),
		_bytesInPerUnitTime(0),
		_bytesOutPerUnitTime(0),
		_lastActivity(now),
		_lastPathReassignment(0),
		_assignedPath(SharedPtr<Path>())
	{}

	/**
	 * Reset flow statistics
	 */
	void resetByteCounts()
	{
		_bytesInPerUnitTime = 0;
		_bytesOutPerUnitTime = 0;
	}

	/**
	 * @return The Flow's ID
	 */
	int32_t id() { return _flowId; }

	/**
	 * @return Number of incoming bytes processed on this flow per unit time
	 */
	int64_t bytesInPerUnitTime() { return _bytesInPerUnitTime; }

	/**
	 * Record number of incoming bytes on this flow
	 *
	 * @param bytes Number of incoming bytes
	 */
	void recordIncomingBytes(uint64_t bytes) { _bytesInPerUnitTime += bytes; }

	/**
	 * @return Number of outgoing bytes processed on this flow per unit time
	 */
	int64_t bytesOutPerUnitTime() { return _bytesOutPerUnitTime; }

	/**
	 * Record number of outgoing bytes on this flow
	 *
	 * @param bytes
	 */
	void recordOutgoingBytes(uint64_t bytes) { _bytesOutPerUnitTime += bytes; }

	/**
	 * @return The total number of bytes processed on this flow
	 */
	uint64_t totalBytes() { return _bytesInPerUnitTime + _bytesOutPerUnitTime; }

	/**
	 * How long since a packet was sent or received in this flow
	 *
	 * @param now Current time
	 * @return The age of the flow in terms of last recorded activity
	 */
	int64_t age(int64_t now) { return now - _lastActivity; }

	/**
	 * Record that traffic was processed on this flow at the given time.
	 *
	 * @param now Current time
	 */
	void updateActivity(int64_t now) { _lastActivity = now; }

	/**
	 * @return Path assigned to this flow
	 */
	SharedPtr<Path> assignedPath() { return _assignedPath; }

	/**
	 * @param path Assigned path over which this flow should be handled
	 */
	void assignPath(const SharedPtr<Path> &path, int64_t now) {
		_assignedPath = path;
		_lastPathReassignment = now;
	}

	AtomicCounter __refCount;

	int32_t _flowId;
	uint64_t _bytesInPerUnitTime;
	uint64_t _bytesOutPerUnitTime;
	int64_t _lastActivity;
	int64_t _lastPathReassignment;
	SharedPtr<Path> _assignedPath;
	SharedPtr<Path> _previouslyAssignedPath;
};

} // namespace ZeroTier

#endif