/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
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
#include "Containers.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

template< unsigned int MF, unsigned int MFP, unsigned int GCT, unsigned int GCS, typename P >
class Defragmenter;

/**
 * A path across the physical network
 */
class Path
{
	friend class SharedPtr< Path >;

	// Allow defragmenter to access fragment-in-flight info stored in Path for performance reasons.
	template< unsigned int MF, unsigned int MFP, unsigned int GCT, unsigned int GCS, typename P >
	friend
	class Defragmenter;

public:
	/**
	 * Map key for paths designed for very fast lookup
	 */
	class Key
	{
	public:
		/**
		 * Construct key with undefined value
		 */
		ZT_INLINE Key() noexcept
		{}

		ZT_INLINE Key(const Key &k) noexcept: m_hashCode(k.m_hashCode), m_v664(k.m_v664), m_port(k.m_port)
		{}

		ZT_INLINE Key(const InetAddress &ip) noexcept
		{
			const unsigned int family = ip.as.sa.sa_family;
			if (family == AF_INET) {
				const uint16_t p = (uint16_t)ip.as.sa_in.sin_port;
				m_hashCode = Utils::hash64((((uint64_t)ip.as.sa_in.sin_addr.s_addr) << 16U) ^ ((uint64_t)p) ^ Utils::s_mapNonce);
				m_v664 = 0; // IPv6 /64 is 0 for IPv4
				m_port = p;
			} else {
				if (likely(family == AF_INET6)) {
					const uint64_t a = Utils::loadMachineEndian< uint64_t >(reinterpret_cast<const uint8_t *>(ip.as.sa_in6.sin6_addr.s6_addr));
					const uint64_t b = Utils::loadMachineEndian< uint64_t >(reinterpret_cast<const uint8_t *>(ip.as.sa_in6.sin6_addr.s6_addr) + 8);
					const uint16_t p = ip.as.sa_in6.sin6_port;
					m_hashCode = Utils::hash64(a ^ b ^ ((uint64_t)p) ^ Utils::s_mapNonce);
					m_v664 = a; // IPv6 /64
					m_port = p;
				} else {
					// This isn't reachable since only IPv4 and IPv6 are used with InetAddress, but implement
					// something here for technical completeness.
					m_hashCode = Utils::fnv1a32(&ip, sizeof(InetAddress));
					m_v664 = Utils::fnv1a32(ip.as.sa.sa_data, sizeof(ip.as.sa.sa_data));
					m_port = (uint16_t)family;
				}
			}
		}

		ZT_INLINE Key &operator=(const Key &k) noexcept
		{
			m_hashCode = k.m_hashCode;
			m_v664 = k.m_v664;
			m_port = k.m_port;
			return *this;
		}

		ZT_INLINE unsigned long hashCode() const noexcept
		{ return (unsigned long)m_hashCode; }

		ZT_INLINE bool operator==(const Key &k) const noexcept
		{ return (m_hashCode == k.m_hashCode) && (m_v664 == k.m_v664) && (m_port == k.m_port); }

		ZT_INLINE bool operator!=(const Key &k) const noexcept
		{ return (!(*this == k)); }

		ZT_INLINE bool operator<(const Key &k) const noexcept
		{
			if (m_hashCode < k.m_hashCode) {
				return true;
			} else if (m_hashCode == k.m_hashCode) {
				if (m_v664 < k.m_v664) {
					return true;
				} else if (m_v664 == k.m_v664) {
					return (m_port < k.m_port);
				}
			}
			return false;
		}

		ZT_INLINE bool operator>(const Key &k) const noexcept
		{ return (k < *this); }

		ZT_INLINE bool operator<=(const Key &k) const noexcept
		{ return !(k < *this); }

		ZT_INLINE bool operator>=(const Key &k) const noexcept
		{ return !(*this < k); }

	private:
		uint64_t m_hashCode;
		uint64_t m_v664;
		uint16_t m_port;
	};

	ZT_INLINE Path(const int64_t l, const InetAddress &r) noexcept:
		m_localSocket(l),
		m_lastIn(0),
		m_lastOut(0),
		m_latency(-1),
		m_addr(r)
	{}

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
	bool send(const RuntimeEnvironment *RR, void *tPtr, const void *data, unsigned int len, int64_t now) noexcept;

	/**
	 * Explicitly update last sent time
	 *
	 * @param now Time of send
	 * @param bytes Bytes sent
	 */
	ZT_INLINE void sent(const int64_t now, const unsigned int bytes) noexcept
	{
		m_lastOut.store(now);
		m_outMeter.log(now, bytes);
	}

	/**
	 * Called when a packet is received from this remote path, regardless of content
	 *
	 * @param now Time of receive
	 * @param bytes Bytes received
	 */
	ZT_INLINE void received(const int64_t now, const unsigned int bytes) noexcept
	{
		m_lastIn.store(now);
		m_inMeter.log(now, bytes);
	}

	/**
	 * Update latency with a new measurement
	 *
	 * @param newMeasurement New latency measurement in milliseconds
	 */
	ZT_INLINE void updateLatency(const unsigned int newMeasurement) noexcept
	{
		int lat = m_latency;
		if (likely(lat > 0)) {
			m_latency = (lat + newMeasurement) / 2;
		} else {
			m_latency = newMeasurement;
		}
	}

	/**
	 * @return Latency in milliseconds or -1 if unknown
	 */
	ZT_INLINE int latency() const noexcept
	{ return m_latency; }

	/**
	 * Check path aliveness
	 *
	 * @param now Current time
	 */
	ZT_INLINE bool alive(const int64_t now) const noexcept
	{ return ((now - m_lastIn.load()) < ZT_PATH_ALIVE_TIMEOUT); }

	/**
	 * @return Physical address
	 */
	ZT_INLINE const InetAddress &address() const noexcept
	{ return m_addr; }

	/**
	 * @return Local socket as specified by external code
	 */
	ZT_INLINE int64_t localSocket() const noexcept
	{ return m_localSocket; }

	/**
	 * @return Last time we received anything
	 */
	ZT_INLINE int64_t lastIn() const noexcept
	{ return m_lastIn.load(); }

	/**
	 * @return Last time we sent something
	 */
	ZT_INLINE int64_t lastOut() const noexcept
	{ return m_lastOut.load(); }

private:
	const int64_t m_localSocket;
	std::atomic< int64_t > m_lastIn;
	std::atomic< int64_t > m_lastOut;
	std::atomic< int > m_latency;
	const InetAddress m_addr;
	Meter<> m_inMeter;
	Meter<> m_outMeter;

	// These fields belong to Defragmenter but are kept in Path for performance
	// as it's much faster this way than having Defragmenter maintain another
	// mapping from paths to inbound message IDs.
	Set< uint64_t > m_inboundFragmentedMessages;
	Mutex m_inboundFragmentedMessages_l;

	std::atomic< int > __refCount;
};

} // namespace ZeroTier

#endif
