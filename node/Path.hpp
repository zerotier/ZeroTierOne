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

#ifndef ZT_PATH_HPP
#define ZT_PATH_HPP

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "SharedPtr.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"
#include "Meter.hpp"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <set>

namespace ZeroTier {

class RuntimeEnvironment;

template<unsigned int MF,unsigned int GCT,unsigned int GCS>
class Defragmenter;

/**
 * A path across the physical network
 */
class Path
{
	friend class SharedPtr<Path>;

	// Allow defragmenter to access fragment in flight info stored in Path for performance reasons.
	template<unsigned int MF,unsigned int GCT,unsigned int GCS>
	friend class Defragmenter;

public:
	ZT_INLINE Path(const int64_t l,const InetAddress &r) noexcept :
		_localSocket(l),
		_lastIn(0),
		_lastOut(0),
		_addr(r)
	{
	}

	/**
	 * Send a packet via this path (last out time is also updated)
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param data Packet data
	 * @param len Packet length
	 * @param now Current time
	 * @return True if transport reported success
	 */
	bool send(const RuntimeEnvironment *RR,void *tPtr,const void *data,unsigned int len,int64_t now) noexcept;

	/**
	 * Explicitly update last sent time
	 *
	 * @param now Time of send
	 * @param bytes Bytes sent
	 */
	ZT_INLINE void sent(const int64_t now,const unsigned int bytes) noexcept
	{
		_lastOut.store(now);
		_outMeter.log(now,bytes);
	}

	/**
	 * Called when a packet is received from this remote path, regardless of content
	 *
	 * @param now Time of receive
	 * @param bytes Bytes received
	 */
	ZT_INLINE void received(const int64_t now,const unsigned int bytes) noexcept
	{
		_lastIn.store(now);
		_inMeter.log(now,bytes);
	}

	/**
	 * Check path aliveness
	 *
	 * @param now Current time
	 */
	ZT_INLINE bool alive(const int64_t now) const noexcept { return ((now - _lastIn.load()) < ZT_PATH_ALIVE_TIMEOUT); }

	/**
	 * @return Physical address
	 */
	ZT_INLINE const InetAddress &address() const noexcept { return _addr; }

	/**
	 * @return Local socket as specified by external code
	 */
	ZT_INLINE int64_t localSocket() const noexcept { return _localSocket; }

	/**
	 * @return Last time we received anything
	 */
	ZT_INLINE int64_t lastIn() const noexcept { return _lastIn.load(); }

	/**
	 * @return Last time we sent something
	 */
	ZT_INLINE int64_t lastOut() const noexcept { return _lastOut.load(); }

private:
	const int64_t _localSocket;
	std::atomic<int64_t> _lastIn;
	std::atomic<int64_t> _lastOut;
	const InetAddress _addr;
	Meter<> _inMeter;
	Meter<> _outMeter;

	// These fields belong to Defragmenter but are kept in Path for performance
	// as it's much faster this way than having Defragmenter maintain another
	// mapping from paths to inbound message IDs.
	std::set<uint64_t> _inboundFragmentedMessages;
	Mutex _inboundFragmentedMessages_l;

	std::atomic<int> __refCount;
};

} // namespace ZeroTier

#endif
