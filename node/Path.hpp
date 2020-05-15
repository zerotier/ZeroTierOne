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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <stdexcept>
#include <algorithm>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "Utils.hpp"
#include "Packet.hpp"
#include "RingBuffer.hpp"

#include "../osdep/Slave.hpp"

/**
 * Maximum return value of preferenceRank()
 */
#define ZT_PATH_MAX_PREFERENCE_RANK ((ZT_INETADDRESS_MAX_SCOPE << 1) | 1)

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * A path across the physical network
 */
class Path
{
	friend class SharedPtr<Path>;
	friend class Bond;

public:
	/**
	 * Efficient unique key for paths in a Hashtable
	 */
	class HashKey
	{
	public:
		HashKey() {}

		HashKey(const int64_t l,const InetAddress &r)
		{
			if (r.ss_family == AF_INET) {
				_k[0] = (uint64_t)reinterpret_cast<const struct sockaddr_in *>(&r)->sin_addr.s_addr;
				_k[1] = (uint64_t)reinterpret_cast<const struct sockaddr_in *>(&r)->sin_port;
				_k[2] = (uint64_t)l;
			} else if (r.ss_family == AF_INET6) {
				memcpy(_k,reinterpret_cast<const struct sockaddr_in6 *>(&r)->sin6_addr.s6_addr,16);
				_k[2] = ((uint64_t)reinterpret_cast<const struct sockaddr_in6 *>(&r)->sin6_port << 32) ^ (uint64_t)l;
			} else {
				memcpy(_k,&r,std::min(sizeof(_k),sizeof(InetAddress)));
				_k[2] += (uint64_t)l;
			}
		}

		inline unsigned long hashCode() const { return (unsigned long)(_k[0] + _k[1] + _k[2]); }

		inline bool operator==(const HashKey &k) const { return ( (_k[0] == k._k[0]) && (_k[1] == k._k[1]) && (_k[2] == k._k[2]) ); }
		inline bool operator!=(const HashKey &k) const { return (!(*this == k)); }

	private:
		uint64_t _k[3];
	};

	Path() :
		_lastOut(0),
		_lastIn(0),
		_lastTrustEstablishedPacketReceived(0),
		_localSocket(-1),
		_latency(0xffff),
		_addr(),
		_ipScope(InetAddress::IP_SCOPE_NONE),
		_lastAckReceived(0),
		_lastAckSent(0),
		_lastQoSMeasurement(0),
		_lastThroughputEstimation(0),
		_lastRefractoryUpdate(0),
		_lastAliveToggle(0),
		_lastEligibilityState(false),
		_lastTrialBegin(0),
		_refractoryPeriod(0),
		_monitorInterval(0),
		_upDelay(0),
		_downDelay(0),
		_ipvPref(0),
		_mode(0),
		_onlyPathOnSlave(false),
		_enabled(false),
		_bonded(false),
		_negotiated(false),
		_deprecated(false),
		_shouldReallocateFlows(false),
		_assignedFlowCount(0),
		_latencyMean(0),
		_latencyVariance(0),
		_packetLossRatio(0),
		_packetErrorRatio(0),
		_throughputMean(0),
		_throughputMax(0),
		_throughputVariance(0),
		_allocation(0),
		_byteLoad(0),
		_relativeByteLoad(0),
		_affinity(0),
		_failoverScore(0),
		_unackedBytes(0),
		_packetsReceivedSinceLastAck(0),
		_packetsReceivedSinceLastQoS(0),
		_bytesAckedSinceLastThroughputEstimation(0),
		_packetsIn(0),
		_packetsOut(0),
		_prevEligibility(false)
		{}

	Path(const int64_t localSocket,const InetAddress &addr) :
		_lastOut(0),
		_lastIn(0),
		_lastTrustEstablishedPacketReceived(0),
		_localSocket(localSocket),
		_latency(0xffff),
		_addr(addr),
		_ipScope(addr.ipScope()),
		_lastAckReceived(0),
		_lastAckSent(0),
		_lastQoSMeasurement(0),
		_lastThroughputEstimation(0),
		_lastRefractoryUpdate(0),
		_lastAliveToggle(0),
		_lastEligibilityState(false),
		_lastTrialBegin(0),
		_refractoryPeriod(0),
		_monitorInterval(0),
		_upDelay(0),
		_downDelay(0),
		_ipvPref(0),
		_mode(0),
		_onlyPathOnSlave(false),
		_enabled(false),
		_bonded(false),
		_negotiated(false),
		_deprecated(false),
		_shouldReallocateFlows(false),
		_assignedFlowCount(0),
		_latencyMean(0),
		_latencyVariance(0),
		_packetLossRatio(0),
		_packetErrorRatio(0),
		_throughputMean(0),
		_throughputMax(0),
		_throughputVariance(0),
		_allocation(0),
		_byteLoad(0),
		_relativeByteLoad(0),
		_affinity(0),
		_failoverScore(0),
		_unackedBytes(0),
		_packetsReceivedSinceLastAck(0),
		_packetsReceivedSinceLastQoS(0),
		_bytesAckedSinceLastThroughputEstimation(0),
		_packetsIn(0),
		_packetsOut(0),
		_prevEligibility(false)
	{}

	/**
	 * Called when a packet is received from this remote path, regardless of content
	 *
	 * @param t Time of receive
	 */
	inline void received(const uint64_t t) {
		_lastIn = t;
		if (!_prevEligibility) {
			_lastAliveToggle = _lastIn;
		}
	}

	/**
	 * Set time last trusted packet was received (done in Peer::received())
	 */
	inline void trustedPacketReceived(const uint64_t t) { _lastTrustEstablishedPacketReceived = t; }

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
	bool send(const RuntimeEnvironment *RR,void *tPtr,const void *data,unsigned int len,int64_t now);

	/**
	 * Manually update last sent time
	 *
	 * @param t Time of send
	 */
	inline void sent(const int64_t t) { _lastOut = t; }

	/**
	 * Update path latency with a new measurement
	 *
	 * @param l Measured latency
	 */
	inline void updateLatency(const unsigned int l, int64_t now)
	{
		unsigned int pl = _latency;
		if (pl < 0xffff) {
			_latency = (pl + l) / 2;
		}
		else {
			_latency = l;
		}
	}

	/**
	 * @return Local socket as specified by external code
	 */
	inline int64_t localSocket() const { return _localSocket; }

	/**
	 * @return Physical address
	 */
	inline const InetAddress &address() const { return _addr; }

	/**
	 * @return IP scope -- faster shortcut for address().ipScope()
	 */
	inline InetAddress::IpScope ipScope() const { return _ipScope; }

	/**
	 * @return True if path has received a trust established packet (e.g. common network membership) in the past ZT_TRUST_EXPIRATION ms
	 */
	inline bool trustEstablished(const int64_t now) const { return ((now - _lastTrustEstablishedPacketReceived) < ZT_TRUST_EXPIRATION); }

	/**
	 * @return Preference rank, higher == better
	 */
	inline unsigned int preferenceRank() const
	{
		// This causes us to rank paths in order of IP scope rank (see InetAdddress.hpp) but
		// within each IP scope class to prefer IPv6 over IPv4.
		return ( ((unsigned int)_ipScope << 1) | (unsigned int)(_addr.ss_family == AF_INET6) );
	}

	/**
	 * Check whether this address is valid for a ZeroTier path
	 *
	 * This checks the address type and scope against address types and scopes
	 * that we currently support for ZeroTier communication.
	 *
	 * @param a Address to check
	 * @return True if address is good for ZeroTier path use
	 */
	static inline bool isAddressValidForPath(const InetAddress &a)
	{
		if ((a.ss_family == AF_INET)||(a.ss_family == AF_INET6)) {
			switch(a.ipScope()) {
				/* Note: we don't do link-local at the moment. Unfortunately these
				 * cause several issues. The first is that they usually require a
				 * device qualifier, which we don't handle yet and can't portably
				 * push in PUSH_DIRECT_PATHS. The second is that some OSes assign
				 * these very ephemerally or otherwise strangely. So we'll use
				 * private, pseudo-private, shared (e.g. carrier grade NAT), or
				 * global IP addresses. */
				case InetAddress::IP_SCOPE_PRIVATE:
				case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
				case InetAddress::IP_SCOPE_SHARED:
				case InetAddress::IP_SCOPE_GLOBAL:
					if (a.ss_family == AF_INET6) {
						// TEMPORARY HACK: for now, we are going to blacklist he.net IPv6
						// tunnels due to very spotty performance and low MTU issues over
						// these IPv6 tunnel links.
						const uint8_t *ipd = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(&a)->sin6_addr.s6_addr);
						if ((ipd[0] == 0x20)&&(ipd[1] == 0x01)&&(ipd[2] == 0x04)&&(ipd[3] == 0x70))
							return false;
					}
					return true;
				default:
					return false;
			}
		}
		return false;
	}

	/**
	 * @return Latency or 0xffff if unknown
	 */
	inline unsigned int latency() const { return _latency; }

	/**
	 * @return Path quality -- lower is better
	 */
	inline long quality(const int64_t now) const
	{
		const int l = (long)_latency;
		const int age = (long)std::min((now - _lastIn),(int64_t)(ZT_PATH_HEARTBEAT_PERIOD * 10)); // set an upper sanity limit to avoid overflow
		return (((age < (ZT_PATH_HEARTBEAT_PERIOD + 5000)) ? l : (l + 0xffff + age)) * (long)((ZT_INETADDRESS_MAX_SCOPE - _ipScope) + 1));
	}

	/**
	 * @param bonded Whether this path is part of a bond.
	 */
	inline void setBonded(bool bonded) { _bonded = bonded; }

	/**
	 * @return True if this path is currently part of a bond.
	 */
	inline bool bonded() { return _bonded; }

	/**
	 * @return True if this path is alive (receiving heartbeats)
	 */
	inline bool alive(const int64_t now, bool bondingEnabled = false) const {
		return (bondingEnabled && _monitorInterval) ? ((now - _lastIn) < (_monitorInterval * 3)) : ((now - _lastIn) < (ZT_PATH_HEARTBEAT_PERIOD + 5000));
	}

	/**
	 * @return True if this path needs a heartbeat
	 */
	inline bool needsHeartbeat(const int64_t now) const { return ((now - _lastOut) >= ZT_PATH_HEARTBEAT_PERIOD); }

	/**
	 * @return True if this path needs a heartbeat in accordance to the user-specified path monitor frequency
	 */
	inline bool needsGratuitousHeartbeat(const int64_t now) { return allowed() && (_monitorInterval > 0) && ((now - _lastOut) >= _monitorInterval); }

	/**
	 * @return Last time we sent something
	 */
	inline int64_t lastOut() const { return _lastOut; }

	/**
	 * @return Last time we received anything
	 */
	inline int64_t lastIn() const { return _lastIn; }

	/**
	 * @return the age of the path in terms of receiving packets
	 */
	inline int64_t age(int64_t now) { return (now - _lastIn); }

	/**
	 * @return Time last trust-established packet was received
	 */
	inline int64_t lastTrustEstablishedPacketReceived() const { return _lastTrustEstablishedPacketReceived; }

	/**
	 * @return Time since last VERB_ACK was received
	 */
	inline int64_t ackAge(int64_t now) { return _lastAckReceived ? now - _lastAckReceived : 0; }

	/**
	 * Set or update a refractory period for the path.
	 *
	 * @param punishment How much a path should be punished
	 * @param pathFailure Whether this call is the result of a recent path failure
	 */
	inline void adjustRefractoryPeriod(int64_t now, uint32_t punishment, bool pathFailure) {
		if (pathFailure) {
			unsigned int suggestedRefractoryPeriod = _refractoryPeriod ? punishment + (_refractoryPeriod * 2) : punishment;
			_refractoryPeriod = std::min(suggestedRefractoryPeriod, (unsigned int)ZT_MULTIPATH_MAX_REFRACTORY_PERIOD);
			_lastRefractoryUpdate = 0;
		} else {
			uint32_t drainRefractory = 0;
			if (_lastRefractoryUpdate) {
				drainRefractory = (now - _lastRefractoryUpdate);
			} else {
				drainRefractory = (now - _lastAliveToggle);
			}
			_lastRefractoryUpdate = now;
			if (_refractoryPeriod > drainRefractory) {
				_refractoryPeriod -= drainRefractory;
			} else {
				_refractoryPeriod = 0;
				_lastRefractoryUpdate = 0;
			}
		}
	}

	/**
	 * Determine the current state of eligibility of the path.
	 *
	 * @param includeRefractoryPeriod Whether current punishment should be taken into consideration
	 * @return True if this path can be used in a bond at the current time
	 */
	inline bool eligible(uint64_t now, int ackSendInterval, bool includeRefractoryPeriod = false) {
		if (includeRefractoryPeriod && _refractoryPeriod) {
			return false;
		}
		bool acceptableAge    = age(now) < ((_monitorInterval * 4) + _downDelay); // Simple RX age (driven by packets of any type and gratuitous VERB_HELLOs)
		bool acceptableAckAge = ackAge(now) < (ackSendInterval); // Whether the remote peer is actually responding to our outgoing traffic or simply sending stuff to us
		bool notTooEarly      = (now - _lastAliveToggle) >= _upDelay; // Whether we've waited long enough since the link last came online
		bool inTrial          = (now - _lastTrialBegin) < _upDelay; // Whether this path is still in its trial period
		bool currEligibility  = allowed() && (((acceptableAge || acceptableAckAge) && notTooEarly) || inTrial);
		return currEligibility;
	}

	/**
	 * Record when this path first entered the bond. Each path is given a trial period where it is admitted
	 * to the bond without requiring observations to prove its performance or reliability.
	 */
	inline void startTrial(uint64_t now) { _lastTrialBegin = now; }

	/**
	 * @return True if a path is permitted to be used in a bond (according to user pref.)
	 */
	inline bool allowed() {
		return _enabled
			&& (!_ipvPref
				|| ((_addr.isV4() && (_ipvPref == 4 || _ipvPref == 46 || _ipvPref == 64))
				|| ((_addr.isV6() && (_ipvPref == 6 || _ipvPref == 46 || _ipvPref == 64)))));
	}

	/**
	 * @return True if a path is preferred over another on the same physical slave (according to user pref.)
	 */
	inline bool preferred() {
		return _onlyPathOnSlave
			|| (_addr.isV4() && (_ipvPref == 4 || _ipvPref == 46))
			|| (_addr.isV6() && (_ipvPref == 6 || _ipvPref == 64));
	}

	/**
	 * @param now Current time
	 * @return Whether an ACK (VERB_ACK) packet needs to be emitted at this time
	 */
	inline bool needsToSendAck(int64_t now, int ackSendInterval) {
		return ((now - _lastAckSent) >= ackSendInterval ||
			(_packetsReceivedSinceLastAck == ZT_QOS_TABLE_SIZE)) && _packetsReceivedSinceLastAck;
	}

	/**
	 * @param now Current time
	 * @return Whether a QoS (VERB_QOS_MEASUREMENT) packet needs to be emitted at this time
	 */
	inline bool needsToSendQoS(int64_t now, int qosSendInterval) {
		return ((_packetsReceivedSinceLastQoS >= ZT_QOS_TABLE_SIZE) ||
			((now - _lastQoSMeasurement) > qosSendInterval)) && _packetsReceivedSinceLastQoS;
	}

	/**
	 * Reset packet counters
	 */
	inline void resetPacketCounts()
	{
		_packetsIn = 0;
		_packetsOut = 0;
	}

private:

	volatile int64_t _lastOut;
	volatile int64_t _lastIn;
	volatile int64_t _lastTrustEstablishedPacketReceived;
	int64_t _localSocket;
	volatile unsigned int _latency;
	InetAddress _addr;
	InetAddress::IpScope _ipScope; // memoize this since it's a computed value checked often
	AtomicCounter __refCount;

	std::map<uint64_t,uint64_t> qosStatsOut; // id:egress_time
	std::map<uint64_t,uint64_t> qosStatsIn; // id:now
	std::map<uint64_t,uint16_t> ackStatsIn; // id:len

	RingBuffer<int,ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> qosRecordSize;
	RingBuffer<float,ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> qosRecordLossSamples;
	RingBuffer<uint64_t,ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> throughputSamples;
	RingBuffer<bool,ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> packetValiditySamples;
	RingBuffer<float,ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> _throughputVarianceSamples;
	RingBuffer<uint16_t,ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> latencySamples;

	/**
	 * Last time that a VERB_ACK was received on this path.
	 */
	uint64_t _lastAckReceived;

	/**
	 * Last time that a VERB_ACK was sent out on this path.
	 */
	uint64_t _lastAckSent;

	/**
	 * Last time that a VERB_QOS_MEASUREMENT was sent out on this path.
	 */
	uint64_t _lastQoSMeasurement;

	/**
	 * Last time that a the path's throughput was estimated.
	 */
	uint64_t _lastThroughputEstimation;

	/**
	 * The last time that the refractory period was updated.
	 */
	uint64_t _lastRefractoryUpdate;

	/**
	 * The last time that the path was marked as "alive".
	 */
	uint64_t _lastAliveToggle;

	/**
	 * State of eligibility at last check. Used for determining state changes.
	 */
	bool _lastEligibilityState;

	/**
	 * Timestamp indicating when this path's trial period began.
	 */
	uint64_t _lastTrialBegin;

	/**
	 * Amount of time that this path is prevented from becoming a member of a bond.
	 */
	uint32_t _refractoryPeriod;

	/**
	 * Monitor interval specific to this path or that was inherited from the bond controller.
	 */
	int32_t _monitorInterval;

	/**
	 * Up delay interval specific to this path or that was inherited from the bond controller.
	 */
	uint32_t _upDelay;

	/**
	 * Down delay interval specific to this path or that was inherited from the bond controller.
	 */
	uint32_t _downDelay;

	/**
	 * IP version preference inherited from the physical slave.
	 */
	uint8_t _ipvPref;

	/**
	 * Mode inherited from the physical slave.
	 */
	uint8_t _mode;

	/**
	 * IP version preference inherited from the physical slave.
	 */
	bool _onlyPathOnSlave;

	/**
	 * Enabled state inherited from the physical slave.
	 */
	bool _enabled;

	/**
	 * Whether this path is currently part of a bond.
	 */
	bool _bonded;

	/**
	 * Whether this path was intentionally _negotiated by either peer.
	 */
	bool _negotiated;

	/**
	 * Whether this path has been deprecated due to performance issues. Current traffic flows
	 * will be re-allocated to other paths in the most non-disruptive manner (if possible),
	 * and new traffic will not be allocated to this path.
	 */
	bool _deprecated;

	/**
	 * Whether flows should be moved from this path. Current traffic flows will be re-allocated
	 * immediately.
	 */
	bool _shouldReallocateFlows;

	/**
	 * The number of flows currently assigned to this path.
	 */
	uint16_t _assignedFlowCount;

	/**
	 * The mean latency (computed from a sliding window.)
	 */
	float _latencyMean;

	/**
	 * Packet delay variance (computed from a sliding window.)
	 */
	float _latencyVariance;

	/**
	 * The ratio of lost packets to received packets.
	 */
	float _packetLossRatio;

	/**
	 * The ratio of packets that failed their MAC/CRC checks to those that did not.
	 */
	float _packetErrorRatio;

	/**
	 * The estimated mean throughput of this path.
	 */
	uint64_t _throughputMean;

	/**
	 * The maximum observed throughput of this path.
	 */
	uint64_t _throughputMax;

	/**
	 * The variance in the estimated throughput of this path.
	 */
	float _throughputVariance;

	/**
	 * The relative quality of this path to all others in the bond, [0-255].
	 */
	uint8_t _allocation;

	/**
	 * How much load this path is under.
	 */
	uint64_t _byteLoad;

	/**
	 * How much load this path is under (relative to other paths in the bond.)
	 */
	uint8_t _relativeByteLoad;

	/**
	 * Relative value expressing how "deserving" this path is of new traffic.
	 */
	uint8_t _affinity;

	/**
	 * Score that indicates to what degree this path is preferred over others that
	 * are available to the bonding policy. (specifically for active-backup)
	 */
	uint32_t _failoverScore;

	/**
	 * Number of bytes thus far sent that have not been acknowledged by the remote peer.
	 */
	int64_t _unackedBytes;

	/**
	 * Number of packets received since the last VERB_ACK was sent to the remote peer.
	 */
	int32_t _packetsReceivedSinceLastAck;

	/**
	 * Number of packets received since the last VERB_QOS_MEASUREMENT was sent to the remote peer.
	 */
	int32_t _packetsReceivedSinceLastQoS;

	/**
	 * Bytes acknowledged via incoming VERB_ACK since the last estimation of throughput.
	 */
	uint64_t _bytesAckedSinceLastThroughputEstimation;

	/**
	 * Counters used for tracking path load.
	 */
	int _packetsIn;
	int _packetsOut;

	// TODO: Remove

	bool _prevEligibility;
};

} // namespace ZeroTier

#endif
