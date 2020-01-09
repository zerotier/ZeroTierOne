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

#ifndef ZT_PATH_HPP
#define ZT_PATH_HPP

#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <stdexcept>
#include <algorithm>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "Utils.hpp"
#include "RingBuffer.hpp"
#include "Packet.hpp"
#include "Mutex.hpp"

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

public:
	/**
	 * Efficient unique key for paths in a Hashtable
	 */
	class HashKey
	{
	public:
		ZT_ALWAYS_INLINE HashKey() {}

		ZT_ALWAYS_INLINE HashKey(const int64_t l,const InetAddress &r)
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

		ZT_ALWAYS_INLINE unsigned long hashCode() const { return (unsigned long)(_k[0] + _k[1] + _k[2]); }

		ZT_ALWAYS_INLINE bool operator==(const HashKey &k) const { return ( (_k[0] == k._k[0]) && (_k[1] == k._k[1]) && (_k[2] == k._k[2]) ); }
		ZT_ALWAYS_INLINE bool operator!=(const HashKey &k) const { return (!(*this == k)); }

	private:
		uint64_t _k[3];
	};

	inline Path() :
		_lastOut(0),
		_lastIn(0),
		_lastPathQualityComputeTime(0),
		_localSocket(-1),
		_latency(0xffff),
		_addr(),
		_ipScope(InetAddress::IP_SCOPE_NONE),
		_lastAck(0),
		_lastThroughputEstimation(0),
		_lastQoSMeasurement(0),
		_lastQoSRecordPurge(0),
		_unackedBytes(0),
		_expectingAckAsOf(0),
		_packetsReceivedSinceLastAck(0),
		_packetsReceivedSinceLastQoS(0),
		_maxLifetimeThroughput(0),
		_lastComputedMeanThroughput(0),
		_bytesAckedSinceLastThroughputEstimation(0),
		_lastComputedMeanLatency(0.0),
		_lastComputedPacketDelayVariance(0.0),
		_lastComputedPacketErrorRatio(0.0),
		_lastComputedPacketLossRatio(0),
		_lastComputedStability(0.0),
		_lastComputedRelativeQuality(0),
		_lastComputedThroughputDistCoeff(0.0),
		_lastAllocation(0)
	{
		memset(_ifname, 0, 16);
		memset(_addrString, 0, sizeof(_addrString));
	}

	inline Path(const int64_t localSocket,const InetAddress &addr) :
		_lastOut(0),
		_lastIn(0),
		_lastPathQualityComputeTime(0),
		_localSocket(localSocket),
		_latency(0xffff),
		_addr(addr),
		_ipScope(addr.ipScope()),
		_lastAck(0),
		_lastThroughputEstimation(0),
		_lastQoSMeasurement(0),
		_lastQoSRecordPurge(0),
		_unackedBytes(0),
		_expectingAckAsOf(0),
		_packetsReceivedSinceLastAck(0),
		_packetsReceivedSinceLastQoS(0),
		_maxLifetimeThroughput(0),
		_lastComputedMeanThroughput(0),
		_bytesAckedSinceLastThroughputEstimation(0),
		_lastComputedMeanLatency(0.0),
		_lastComputedPacketDelayVariance(0.0),
		_lastComputedPacketErrorRatio(0.0),
		_lastComputedPacketLossRatio(0),
		_lastComputedStability(0.0),
		_lastComputedRelativeQuality(0),
		_lastComputedThroughputDistCoeff(0.0),
		_lastAllocation(0)
	{
		memset(_ifname, 0, 16);
		memset(_addrString, 0, sizeof(_addrString));
		if (_localSocket != -1) {
			// TODO: add localInterface alongside localSocket
			//_phy->getIfName((PhySocket *) ((uintptr_t) _localSocket), _ifname, 16);
		}
	}

	/**
	 * Called when a packet is received from this remote path, regardless of content
	 *
	 * @param t Time of receive
	 */
	inline void received(const uint64_t t) { _lastIn = t; }

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
		_latencySamples.push(l);
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
		const long l = (long)_latency;
		const long age = (long)std::min((long)(now - _lastIn),(long)(ZT_PEER_PING_PERIOD * 10)); // set an upper sanity limit to avoid overflow
		return ( ( (age < (ZT_PEER_PING_PERIOD + 5000)) ? l : (l + 65535 + age) ) * (long)((ZT_INETADDRESS_MAX_SCOPE - _ipScope) + 1));
	}

	/**
	 * Record statistics on outgoing packets. Used later to estimate QoS metrics.
	 *
	 * @param now Current time
	 * @param packetId ID of packet
	 * @param payloadLength Length of payload
	 * @param verb Packet verb
	 */
	inline void recordOutgoingPacket(int64_t now, int64_t packetId, uint16_t payloadLength, Packet::Verb verb)
	{
		Mutex::Lock _l(_statistics_m);
		if (verb != Packet::VERB_ACK && verb != Packet::VERB_QOS_MEASUREMENT) {
			if ((packetId & (ZT_PATH_QOS_ACK_PROTOCOL_DIVISOR - 1)) == 0) {
				_unackedBytes += payloadLength;
				// Take note that we're expecting a VERB_ACK on this path as of a specific time
				_expectingAckAsOf = ackAge(now) > ZT_PATH_ACK_INTERVAL ? _expectingAckAsOf : now;
				if (_outQoSRecords.size() < ZT_PATH_MAX_OUTSTANDING_QOS_RECORDS) {
					_outQoSRecords[packetId] = now;
				}
			}
		}
	}

	/**
	 * Record statistics on incoming packets. Used later to estimate QoS metrics.
	 *
	 * @param now Current time
	 * @param packetId ID of packet
	 * @param payloadLength Length of payload
	 * @param verb Packet verb
	 */
	inline void recordIncomingPacket(int64_t now, int64_t packetId, uint16_t payloadLength, Packet::Verb verb)
	{
		Mutex::Lock _l(_statistics_m);
		if (verb != Packet::VERB_ACK && verb != Packet::VERB_QOS_MEASUREMENT) {
			if ((packetId & (ZT_PATH_QOS_ACK_PROTOCOL_DIVISOR - 1)) == 0) {
				_inACKRecords[packetId] = payloadLength;
				_packetsReceivedSinceLastAck++;
				_inQoSRecords[packetId] = now;
				_packetsReceivedSinceLastQoS++;
			}
			_packetValiditySamples.push(true);
		}
	}

	/**
	 * Record that we've received a VERB_ACK on this path, also compute throughput if required.
	 *
	 * @param now Current time
	 * @param ackedBytes Number of bytes acknowledged by other peer
	 */
	inline void receivedAck(int64_t now, int32_t ackedBytes)
	{
		_expectingAckAsOf = 0;
		_unackedBytes = (ackedBytes > _unackedBytes) ? 0 : _unackedBytes - ackedBytes;
		int64_t timeSinceThroughputEstimate = (now - _lastThroughputEstimation);
		if (timeSinceThroughputEstimate >= ZT_PATH_THROUGHPUT_MEASUREMENT_INTERVAL) {
			uint64_t throughput = (uint64_t)((float)(_bytesAckedSinceLastThroughputEstimation * 8) / ((float)timeSinceThroughputEstimate / (float)1000));
			_throughputSamples.push(throughput);
			_maxLifetimeThroughput = throughput > _maxLifetimeThroughput ? throughput : _maxLifetimeThroughput;
			_lastThroughputEstimation = now;
			_bytesAckedSinceLastThroughputEstimation = 0;
		} else {
			_bytesAckedSinceLastThroughputEstimation += ackedBytes;
		}
	}

	/**
	 * @return Number of bytes this peer is responsible for ACKing since last ACK
	 */
	inline int32_t bytesToAck()
	{
		Mutex::Lock _l(_statistics_m);
		int32_t bytesToAck = 0;
		std::map<uint64_t,uint16_t>::iterator it = _inACKRecords.begin();
		while (it != _inACKRecords.end()) {
			bytesToAck += it->second;
			it++;
		}
		return bytesToAck;
	}

	/**
	 * @return Number of bytes thus far sent that have not been acknowledged by the remote peer
	 */
	inline int64_t unackedSentBytes()
	{
		return _unackedBytes;
	}

	/**
	 * Account for the fact that an ACK was just sent. Reset counters, timers, and clear statistics buffers
	 *
	 * @param Current time
	 */
	inline void sentAck(int64_t now)
	{
		Mutex::Lock _l(_statistics_m);
		_inACKRecords.clear();
		_packetsReceivedSinceLastAck = 0;
		_lastAck = now;
	}

	/**
	 * Receive QoS data, match with recorded egress times from this peer, compute latency
	 * estimates.
	 *
	 * @param now Current time
	 * @param count Number of records
	 * @param rx_id table of packet IDs
	 * @param rx_ts table of holding times
	 */
	inline void receivedQoS(int64_t now, int count, uint64_t *rx_id, uint16_t *rx_ts)
	{
		Mutex::Lock _l(_statistics_m);
		// Look up egress times and compute latency values for each record
		std::map<uint64_t,uint64_t>::iterator it;
		for (int j=0; j<count; j++) {
			it = _outQoSRecords.find(rx_id[j]);
			if (it != _outQoSRecords.end()) {
				uint16_t rtt = (uint16_t)(now - it->second);
				uint16_t rtt_compensated = rtt - rx_ts[j];
				uint16_t latency = rtt_compensated / 2;
				updateLatency(latency, now);
				_outQoSRecords.erase(it);
			}
		}
	}

	/**
	 * Generate the contents of a VERB_QOS_MEASUREMENT packet.
	 *
	 * @param now Current time
	 * @param qosBuffer destination buffer
	 * @return Size of payload
	 */
	inline int32_t generateQoSPacket(int64_t now, char *qosBuffer)
	{
		Mutex::Lock _l(_statistics_m);
		int32_t len = 0;
		std::map<uint64_t,uint64_t>::iterator it = _inQoSRecords.begin();
		int i=0;
		while (i<_packetsReceivedSinceLastQoS && it != _inQoSRecords.end()) {
			uint64_t id = it->first;
			memcpy(qosBuffer, &id, sizeof(uint64_t));
			qosBuffer+=sizeof(uint64_t);
			uint16_t holdingTime = (uint16_t)(now - it->second);
			memcpy(qosBuffer, &holdingTime, sizeof(uint16_t));
			qosBuffer+=sizeof(uint16_t);
			len+=sizeof(uint64_t)+sizeof(uint16_t);
			_inQoSRecords.erase(it++);
			i++;
		}
		return len;
	}

	/**
	 * Account for the fact that a VERB_QOS_MEASUREMENT was just sent. Reset timers.
	 *
	 * @param Current time
	 */
	inline void sentQoS(int64_t now) {
		_packetsReceivedSinceLastQoS = 0;
		_lastQoSMeasurement = now;
	}

	/**
	 * @param now Current time
	 * @return Whether an ACK (VERB_ACK) packet needs to be emitted at this time
	 */
	inline bool needsToSendAck(int64_t now) {
		return ((now - _lastAck) >= ZT_PATH_ACK_INTERVAL ||
			(_packetsReceivedSinceLastAck == ZT_PATH_QOS_TABLE_SIZE)) && _packetsReceivedSinceLastAck;
	}

	/**
	 * @param now Current time
	 * @return Whether a QoS (VERB_QOS_MEASUREMENT) packet needs to be emitted at this time
	 */
	inline bool needsToSendQoS(int64_t now) {
		return ((_packetsReceivedSinceLastQoS >= ZT_PATH_QOS_TABLE_SIZE) ||
			((now - _lastQoSMeasurement) > ZT_PATH_QOS_INTERVAL)) && _packetsReceivedSinceLastQoS;
	}

	/**
	 * How much time has elapsed since we've been expecting a VERB_ACK on this path. This value
	 * is used to determine a more relevant path "age". This lets us penalize paths which are no
	 * longer ACKing, but not those that simple aren't being used to carry traffic at the
	 * current time.
	 */
	inline int64_t ackAge(int64_t now) { return _expectingAckAsOf ? now - _expectingAckAsOf : 0; }

	/**
	 * The maximum observed throughput (in bits/s) for this path
	 */
	inline uint64_t maxLifetimeThroughput() { return _maxLifetimeThroughput; }

	/**
	 * @return The mean throughput (in bits/s) of this link
	 */
	inline uint64_t meanThroughput() { return _lastComputedMeanThroughput; }

	/**
	 * Assign a new relative quality value for this path in the aggregate link
	 *
	 * @param rq Quality of this path in comparison to other paths available to this peer
	 */
	inline void updateRelativeQuality(float rq) { _lastComputedRelativeQuality = rq; }

	/**
	 * @return Quality of this path compared to others in the aggregate link
	 */
	inline float relativeQuality() { return _lastComputedRelativeQuality; }

	/**
	 * Assign a new allocation value for this path in the aggregate link
	 *
	 * @param allocation Percentage of traffic to be sent over this path to a peer
	 */
	inline void updateComponentAllocationOfAggregateLink(unsigned char allocation) { _lastAllocation = allocation; }

	/**
	 * @return Percentage of traffic allocated to this path in the aggregate link
	 */
	inline unsigned char allocation() { return _lastAllocation; }

	/**
	 * @return Stability estimates can become expensive to compute, we cache the most recent result.
	 */
	inline float lastComputedStability() { return _lastComputedStability; }

	/**
	 * @return A pointer to a cached copy of the human-readable name of the interface this Path's localSocket is bound to
	 */
	inline char *getName() { return _ifname; }

	/**
	 * @return Packet delay variance
	 */
	inline float packetDelayVariance() { return _lastComputedPacketDelayVariance; }

	/**
	 * @return Previously-computed mean latency
	 */
	inline float meanLatency() { return _lastComputedMeanLatency; }

	/**
	 * @return Packet loss rate (PLR)
	 */
	inline float packetLossRatio() { return _lastComputedPacketLossRatio; }

	/**
	 * @return Packet error ratio (PER)
	 */
	inline float packetErrorRatio() { return _lastComputedPacketErrorRatio; }

	/**
	 * Record an invalid incoming packet. This packet failed MAC/compression/cipher checks and will now
	 * contribute to a Packet Error Ratio (PER).
	 */
	inline void recordInvalidPacket() { _packetValiditySamples.push(false); }

	/**
	 * @return A pointer to a cached copy of the address string for this Path (For debugging only)
	 */
	inline char *getAddressString() { return _addrString; }

	/**
	 * @return The current throughput disturbance coefficient
	 */
	inline float throughputDisturbanceCoefficient() { return _lastComputedThroughputDistCoeff; }

	/**
	 * Compute and cache stability and performance metrics. The resultant stability coefficient is a measure of how "well behaved"
	 * this path is. This figure is substantially different from (but required for the estimation of the path's overall "quality".
	 *
	 * @param now Current time
	 */
	inline void processBackgroundPathMeasurements(const int64_t now)
	{
		if (now - _lastPathQualityComputeTime > ZT_PATH_QUALITY_COMPUTE_INTERVAL) {
			Mutex::Lock _l(_statistics_m);
			_lastPathQualityComputeTime = now;
			address().toString(_addrString);
			_lastComputedMeanLatency = _latencySamples.mean();
			_lastComputedPacketDelayVariance = _latencySamples.stddev(); // Similar to "jitter" (SEE: RFC 3393, RFC 4689)
			_lastComputedMeanThroughput = (uint64_t)_throughputSamples.mean();

			// If no packet validity samples, assume PER==0
			_lastComputedPacketErrorRatio = 1 - (_packetValiditySamples.count() ? _packetValiditySamples.mean() : 1);

			// Compute path stability
			// Normalize measurements with wildly different ranges into a reasonable range
			float normalized_pdv = Utils::normalize(_lastComputedPacketDelayVariance, 0, ZT_PATH_MAX_PDV, 0, 10);
			float normalized_la = Utils::normalize(_lastComputedMeanLatency, 0, ZT_PATH_MAX_MEAN_LATENCY, 0, 10);
			float throughput_cv = _throughputSamples.mean() > 0 ? _throughputSamples.stddev() / _throughputSamples.mean() : 1;

			// Form an exponential cutoff and apply contribution weights
			float pdv_contrib = expf((-1.0f)*normalized_pdv) * (float)ZT_PATH_CONTRIB_PDV;
			float latency_contrib = expf((-1.0f)*normalized_la) * (float)ZT_PATH_CONTRIB_LATENCY;

			// Throughput Disturbance Coefficient
			float throughput_disturbance_contrib = expf((-1.0f)*throughput_cv) * (float)ZT_PATH_CONTRIB_THROUGHPUT_DISTURBANCE;
			_throughputDisturbanceSamples.push(throughput_cv);
			_lastComputedThroughputDistCoeff = _throughputDisturbanceSamples.mean();

			// Obey user-defined ignored contributions
			pdv_contrib = ZT_PATH_CONTRIB_PDV > 0.0 ? pdv_contrib : 1;
			latency_contrib = ZT_PATH_CONTRIB_LATENCY > 0.0 ? latency_contrib : 1;
			throughput_disturbance_contrib = ZT_PATH_CONTRIB_THROUGHPUT_DISTURBANCE > 0.0 ? throughput_disturbance_contrib : 1;

			// Stability
			_lastComputedStability = pdv_contrib + latency_contrib + throughput_disturbance_contrib;
			_lastComputedStability *= 1 - _lastComputedPacketErrorRatio;

			// Prevent QoS records from sticking around for too long
			std::map<uint64_t,uint64_t>::iterator it = _outQoSRecords.begin();
			while (it != _outQoSRecords.end()) {
				// Time since egress of tracked packet
				if ((now - it->second) >= ZT_PATH_QOS_TIMEOUT) {
					_outQoSRecords.erase(it++);
				} else { it++; }
			}
		}
	}

	/**
	 * @return True if this path is alive (receiving data)
	 */
	inline bool alive(const int64_t now) const { return ((now - _lastIn) < ((ZT_PEER_PING_PERIOD * 2) + 5000)); }

	/**
	 * @return Last time we sent something
	 */
	inline int64_t lastOut() const { return _lastOut; }

	/**
	 * @return Last time we received anything
	 */
	inline int64_t lastIn() const { return _lastIn; }

private:
	Mutex _statistics_m;

	volatile int64_t _lastOut;
	volatile int64_t _lastIn;
	volatile int64_t _lastPathQualityComputeTime;
	int64_t _localSocket;
	volatile unsigned int _latency;
	InetAddress _addr;
	InetAddress::IpScope _ipScope; // memoize this since it's a computed value checked often
	AtomicCounter __refCount;

	std::map<uint64_t,uint64_t> _outQoSRecords; // id:egress_time
	std::map<uint64_t,uint64_t> _inQoSRecords; // id:now
	std::map<uint64_t,uint16_t> _inACKRecords; // id:len

	int64_t _lastAck;
	int64_t _lastThroughputEstimation;
	int64_t _lastQoSMeasurement;
	int64_t _lastQoSRecordPurge;

	int64_t _unackedBytes;
	int64_t _expectingAckAsOf;
	int16_t _packetsReceivedSinceLastAck;
	int16_t _packetsReceivedSinceLastQoS;

	uint64_t _maxLifetimeThroughput;
	uint64_t _lastComputedMeanThroughput;
	uint64_t _bytesAckedSinceLastThroughputEstimation;

	float _lastComputedMeanLatency;
	float _lastComputedPacketDelayVariance;

	float _lastComputedPacketErrorRatio;
	float _lastComputedPacketLossRatio;

	// cached estimates
	float _lastComputedStability;
	float _lastComputedRelativeQuality;
	float _lastComputedThroughputDistCoeff;
	unsigned char _lastAllocation;

	// cached human-readable strings for tracing purposes
	char _ifname[16];
	char _addrString[256];

	RingBuffer<uint64_t,ZT_PATH_QUALITY_METRIC_WIN_SZ> _throughputSamples;
	RingBuffer<uint32_t,ZT_PATH_QUALITY_METRIC_WIN_SZ> _latencySamples;
	RingBuffer<bool,ZT_PATH_QUALITY_METRIC_WIN_SZ> _packetValiditySamples;
	RingBuffer<float,ZT_PATH_QUALITY_METRIC_WIN_SZ> _throughputDisturbanceSamples;
};

} // namespace ZeroTier

#endif
