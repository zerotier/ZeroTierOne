/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2018  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

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
#include "RingBuffer.hpp"

#include "../osdep/Phy.hpp"

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
	Phy<Path *> *_phy;

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
				ZT_FAST_MEMCPY(_k,reinterpret_cast<const struct sockaddr_in6 *>(&r)->sin6_addr.s6_addr,16);
				_k[2] = ((uint64_t)reinterpret_cast<const struct sockaddr_in6 *>(&r)->sin6_port << 32) ^ (uint64_t)l;
			} else {
				ZT_FAST_MEMCPY(_k,&r,std::min(sizeof(_k),sizeof(InetAddress)));
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
		_lastPathQualityComputeTime(0),
		_localSocket(-1),
		_latency(0xffff),
		_addr(),
		_ipScope(InetAddress::IP_SCOPE_NONE),
		_currentPacketSampleCounter(0),
		_meanPacketErrorRatio(0.0),
		_meanLatency(0.0),
		_lastLatencyUpdate(0),
		_jitter(0.0),
		_lastPathQualitySampleTime(0),
		_lastComputedQuality(0.0),
		_lastPathQualityEstimate(0),
		_meanAge(0.0),
		_meanThroughput(0.0),
		_packetLossRatio(0)
	{
		memset(_ifname, 0, sizeof(_ifname));
		memset(_addrString, 0, sizeof(_addrString));
		_throughputSamples = new RingBuffer<uint64_t>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
		_ageSamples = new RingBuffer<uint64_t>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
		_latencySamples = new RingBuffer<uint32_t>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
		_errSamples = new RingBuffer<float>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
	}

	Path(const int64_t localSocket,const InetAddress &addr) :
		_lastOut(0),
		_lastIn(0),
		_lastTrustEstablishedPacketReceived(0),
		_lastPathQualityComputeTime(0),
		_localSocket(localSocket),
		_latency(0xffff),
		_addr(addr),
		_ipScope(addr.ipScope()),
		_currentPacketSampleCounter(0),
		_meanPacketErrorRatio(0.0),
		_meanLatency(0.0),
		_lastLatencyUpdate(0),
		_jitter(0.0),
		_lastPathQualitySampleTime(0),
		_lastComputedQuality(0.0),
		_lastPathQualityEstimate(0),
		_meanAge(0.0),
		_meanThroughput(0.0),
		_packetLossRatio(0)
	{
		memset(_ifname, 0, sizeof(_ifname));
		memset(_addrString, 0, sizeof(_addrString));
		_throughputSamples = new RingBuffer<uint64_t>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
		_ageSamples = new RingBuffer<uint64_t>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
		_latencySamples = new RingBuffer<uint32_t>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
		_errSamples = new RingBuffer<float>(ZT_PATH_QUALITY_METRIC_WIN_SZ);
	}

	~Path()
	{
		delete _throughputSamples;
		delete _ageSamples;
		delete _latencySamples;
		delete _errSamples;

		_throughputSamples = NULL;
		_ageSamples = NULL;
		_latencySamples = NULL;
		_errSamples = NULL;
	}

	/**
	 * Called when a packet is received from this remote path, regardless of content
	 *
	 * @param t Time of receive
	 */
	inline void received(const uint64_t t) { _lastIn = t; }

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
		_lastLatencyUpdate = now;
		_latencySamples->push(l);
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
	 * @return An estimate of path quality -- higher is better.
	 */
	inline float computeQuality(const int64_t now)
	{
		float latency_contrib    = _meanLatency ? (float)1.0 / _meanLatency : 0;
		float jitter_contrib     = _jitter ? (float)1.0 / _jitter : 0;
		float throughput_contrib = _meanThroughput ? _meanThroughput / 1000000 : 0; // in Mbps
		float age_contrib        = _meanAge > 0 ? (float)sqrt(_meanAge) : 1;
		float error_contrib      = (float)1.0 - _meanPacketErrorRatio;
		float sum = (latency_contrib + jitter_contrib + throughput_contrib + error_contrib) / age_contrib;
		_lastComputedQuality = sum * (long)((_ipScope) + 1);
		return _lastComputedQuality;
	}

	/**
	 * Since quality estimates can become expensive we should cache the most recent result for traffic allocation
	 * algorithms which may need to reference this value multiple times through the course of their execution.
	 */
	inline float lastComputedQuality() {
		return _lastComputedQuality;
	}

	/**
	 * @return A pointer to a cached copy of the human-readable name of the interface this Path's localSocket is bound to
	 */
	inline char *getName() { return _ifname; }

	/**
	 * @return Estimated throughput in bps of this link
	 */
	inline uint64_t getThroughput() { return _phy->getThroughput((PhySocket *)((uintptr_t)_localSocket)); }

	/**
	 * @return Packet delay varience
	 */
	inline float jitter() { return _jitter; }

	/**
	 * @return Previously-computed mean latency
	 */
	inline float meanLatency() { return _meanLatency; }

	/**
	 * @return Packet loss rate
	 */
	inline float packetLossRatio() { return _packetLossRatio; }

	/**
	 * @return Mean packet error ratio
	 */
	inline float meanPacketErrorRatio() { return _meanPacketErrorRatio; }

	/**
	 * @return Current packet error ratio (possibly incomplete sample set)
	 */
	inline float currentPacketErrorRatio() {
		int errorsPerSample = 0;
		for (int i=0; i<_currentPacketSampleCounter; i++) {
			if (_packetValidity[i] == false) {
				errorsPerSample++;
			}
		}
		return (float)errorsPerSample / (float)ZT_PATH_ERROR_SAMPLE_WIN_SZ;
	}

	/**
	 * @return Whether the Path's local socket is in a CLOSED state
	 */
	inline bool isClosed() { return _phy->isClosed((PhySocket *)((uintptr_t)_localSocket)); }

	/**
	 * @return The state of a Path's local socket
	 */
	inline int getState() { return _phy->getState((PhySocket *)((uintptr_t)_localSocket)); }

	/**
	 * @return Whether this socket may have been erased by the virtual physical link layer
	 */
	inline bool isValidState() { return _phy->isValidState((PhySocket *)((uintptr_t)_localSocket)); }

	/**
	 * @return Whether the path quality monitors have collected enough data to provide a quality value
	 * TODO: expand this
	 */
	inline bool monitorsReady() {
		return _latencySamples->count() && _ageSamples->count() && _throughputSamples->count();
	}

	/**
	 * @return A pointer to a cached copy of the address string for this Path (For debugging only)
	 */
	inline char *getAddressString() { return _addrString; }

	/**
	 * Handle path sampling, computation of quality estimates, and other periodic tasks
	 * @param now Current time
	 */
	inline void measureLink(int64_t now) {
		// Sample path properties and store them in a continuously-revolving buffer
		if (now - _lastPathQualitySampleTime > ZT_PATH_QUALITY_SAMPLE_INTERVAL) {
			_lastPathQualitySampleTime = now;
			_throughputSamples->push(getThroughput()); // Thoughtput in bits/s
			_ageSamples->push(now - _lastIn); // Age (time since last received packet)
			if (now - _lastLatencyUpdate > ZT_PATH_LATENCY_SAMPLE_INTERVAL) {
				_lastLatencyUpdate = now;
				// Record 0 bp/s. Since we're using this to detect possible packet loss
				updateLatency(0, now);
			}
		}
		// Compute statistical values for use in link quality estimates
		if (now - _lastPathQualityComputeTime > ZT_PATH_QUALITY_COMPUTE_INTERVAL) {
			_lastPathQualityComputeTime = now;
			// Cache Path address string
			address().toString(_addrString);
			_phy->getIfName((PhySocket *)((uintptr_t)_localSocket), _ifname, ZT_PATH_INTERFACE_NAME_SZ); // Cache Interface name
			// Derived values
			if (_throughputSamples->count()) {
				_packetLossRatio = (float)_throughputSamples->zeroCount() / (float)_throughputSamples->count();
			}
			_meanThroughput = _throughputSamples->mean();
			_meanAge = _ageSamples->mean();
			_meanLatency = _latencySamples->mean();
			// Jitter
			// SEE: RFC 3393, RFC 4689
			_jitter = _latencySamples->stddev();
			_meanPacketErrorRatio = _errSamples->mean(); // Packet Error Ratio (PER)
		}
		// Periodically compute a path quality estimate
		if (now - _lastPathQualityEstimate > ZT_PATH_QUALITY_ESTIMATE_INTERVAL) {
			computeQuality(now);
		}
	}

	/**
	 * @param buf Buffer to store resultant string
	 * @return Description of path, in ASCII string format
	 */
	inline char *toString(char *buf) {
		sprintf(buf,"%6s, q=%8.3f, %5.3f Mb/s, j=%8.2f, ml=%8.2f, meanAge=%8.2f, addr=%45s",
			getName(),
			lastComputedQuality(),
			(float)meanThroughput() / (float)1000000,
			jitter(),
			meanLatency(),
			meanAge(),
			getAddressString());
		return buf;
	}

	/**
	 * Record whether a packet is considered invalid by MAC/compression/cipher checks. This
	 * could be an indication of a bit error. This function will keep a running counter of
	 * up to a given window size and with each counter overflow it will compute a mean error rate
	 * and store that in a continuously shifting sample window.
	 *
	 * @param isValid Whether the packet in question is considered invalid
	 */
	inline void recordPacket(bool isValid) {
		if (_currentPacketSampleCounter < ZT_PATH_ERROR_SAMPLE_WIN_SZ) {
			_packetValidity[_currentPacketSampleCounter] = isValid;
			_currentPacketSampleCounter++;
		}
		else {
			// Sample array is full, compute an mean and stick it in the ring buffer for trend analysis
			_errSamples->push(currentPacketErrorRatio());
			_currentPacketSampleCounter=0;
		}
	}

	/**
	 * @return The mean age (in ms) of this link
	 */
	inline float meanAge() { return _meanAge; }

	/**
	 * @return The mean throughput (in bits/s) of this link
	 */
	inline float meanThroughput() { return _meanThroughput; }

	/**
	 * @return True if this path is alive (receiving heartbeats)
	 */
	inline bool alive(const int64_t now) const { return ((now - _lastIn) < (ZT_PATH_HEARTBEAT_PERIOD + 5000)); }

	/**
	 * @return True if this path hasn't received a packet in a "significant" amount of time
	 */
	inline bool stale(const int64_t now) const { return ((now - _lastIn) > ZT_LINK_SPEED_TEST_INTERVAL * 10); }

	/**
	 * @return True if this path needs a heartbeat
	 */
	inline bool needsHeartbeat(const int64_t now) const { return ((now - _lastOut) >= ZT_PATH_HEARTBEAT_PERIOD); }

	/**
	 * @return Last time we sent something
	 */
	inline int64_t lastOut() const { return _lastOut; }

	/**
	 * @return Last time we received anything
	 */
	inline int64_t lastIn() const { return _lastIn; }

	/**
	 * @return Time last trust-established packet was received
	 */
	inline int64_t lastTrustEstablishedPacketReceived() const { return _lastTrustEstablishedPacketReceived; }

private:
	volatile int64_t _lastOut;
	volatile int64_t _lastIn;
	volatile int64_t _lastTrustEstablishedPacketReceived;
	volatile int64_t _lastPathQualityComputeTime;
	int64_t _localSocket;
	volatile unsigned int _latency;
	InetAddress _addr;
	InetAddress::IpScope _ipScope; // memoize this since it's a computed value checked often
	AtomicCounter __refCount;

	// Packet Error Ratio (PER)
	int _packetValidity[ZT_PATH_ERROR_SAMPLE_WIN_SZ];
	int _currentPacketSampleCounter;
	volatile float _meanPacketErrorRatio;

	// Latency and Jitter
	volatile float _meanLatency;
	int64_t _lastLatencyUpdate;
	volatile float _jitter;

	int64_t _lastPathQualitySampleTime;
	float _lastComputedQuality;
	int64_t _lastPathQualityEstimate;
	float _meanAge;
	float _meanThroughput;

	// Circular buffers used to efficiently store large time series
	RingBuffer<uint64_t> *_throughputSamples;
	RingBuffer<uint32_t> *_latencySamples;
	RingBuffer<uint64_t> *_ageSamples;
	RingBuffer<float> *_errSamples;

	float _packetLossRatio;

	char _ifname[ZT_PATH_INTERFACE_NAME_SZ];
	char _addrString[256];
};

} // namespace ZeroTier

#endif
