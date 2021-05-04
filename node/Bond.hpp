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

#ifndef ZT_BOND_HPP
#define ZT_BOND_HPP

#include "Flow.hpp"
#include "Path.hpp"
#include "Peer.hpp"
#include "Packet.hpp"

#include <list>
#include <map>

namespace ZeroTier {

class RuntimeEnvironment;
class Link;
class Peer;

class Bond {
	friend class SharedPtr<Bond>;
	friend class Peer;
	friend class BondController;

	struct PathQualityComparator {
		bool operator()(const SharedPtr<Path>& a, const SharedPtr<Path>& b)
		{
			if (a->_failoverScore == b->_failoverScore) {
				return a < b;
			}
			return a->_failoverScore > b->_failoverScore;
		}
	};

  public:
	// TODO: Remove
	bool _header;
	int64_t _lastLogTS;
	int64_t _lastPrintTS;
	void dumpInfo(const int64_t now);
	bool relevant();

	SharedPtr<Link> getLink(const SharedPtr<Path>& path);

	/**
	 * Constructor. Creates a bond based off of ZT defaults
	 *
	 * @param renv Runtime environment
	 * @param policy Bonding policy
	 * @param peer
	 */
	Bond(const RuntimeEnvironment* renv, int policy, const SharedPtr<Peer>& peer);

	/**
	 * Constructor. For use when user intends to manually specify parameters
	 *
	 * @param basePolicy
	 * @param policyAlias
	 * @param peer
	 */
	Bond(const RuntimeEnvironment* renv, std::string& basePolicy, std::string& policyAlias, const SharedPtr<Peer>& peer);

	/**
	 * Constructor. Creates a bond based off of a user-defined bond template
	 *
	 * @param renv Runtime environment
	 * @param original
	 * @param peer
	 */
	Bond(const RuntimeEnvironment* renv, SharedPtr<Bond> originalBond, const SharedPtr<Peer>& peer);

	/**
	 * @return The human-readable name of the bonding policy
	 */
	std::string policyAlias()
	{
		return _policyAlias;
	}

	/**
	 * Inform the bond about the path that its peer (owning object) just learned about.
	 * If the path is allowed to be used, it will be inducted into the bond on a trial
	 * period where link statistics will be collected to judge its quality.
	 *
	 * @param path Newly-learned Path which should now be handled by the Bond
	 * @param now Current time
	 */
	void nominatePath(const SharedPtr<Path>& path, int64_t now);

	/**
	 * Propagate and memoize often-used bonding preferences for each path
	 */
	void applyUserPrefs();

	/**
	 * Check path states and perform bond rebuilds if needed.
	 *
	 * @param now Current time
	 * @param rebuild Whether or not the bond should be reconstructed.
	 */
	void curateBond(const int64_t now, bool rebuild);

	/**
	 * Periodically perform statistical summaries of quality metrics for all paths.
	 *
	 * @param now Current time
	 */
	void estimatePathQuality(int64_t now);

	/**
	 * Record an invalid incoming packet. This packet failed
	 * MAC/compression/cipher checks and will now contribute to a
	 * Packet Error Ratio (PER).
	 *
	 * @param path Path over which packet was received
	 */
	void recordIncomingInvalidPacket(const SharedPtr<Path>& path);

	/**
	 * Record statistics on outbound an packet.
	 *
	 * @param path Path over which packet is being sent
	 * @param packetId Packet ID
	 * @param payloadLength Packet data length
	 * @param verb Packet verb
	 * @param flowId Flow ID
	 * @param now Current time
	 */
	void recordOutgoingPacket(const SharedPtr<Path>& path, uint64_t packetId, uint16_t payloadLength, Packet::Verb verb, int32_t flowId, int64_t now);

	/**
	 * Process the contents of an inbound VERB_QOS_MEASUREMENT to gather path quality observations.
	 *
	 * @param now Current time
	 * @param count Number of records
	 * @param rx_id table of packet IDs
	 * @param rx_ts table of holding times
	 */
	void receivedQoS(const SharedPtr<Path>& path, int64_t now, int count, uint64_t* rx_id, uint16_t* rx_ts);

	/**
	 * Process the contents of an inbound VERB_ACK to gather path quality observations.
	 *
	 * @param path Path over which packet was received
	 * @param now Current time
	 * @param ackedBytes Number of bytes ACKed by this VERB_ACK
	 */
	void receivedAck(const SharedPtr<Path>& path, int64_t now, int32_t ackedBytes);

	/**
	 * Generate the contents of a VERB_QOS_MEASUREMENT packet.
	 *
	 * @param now Current time
	 * @param qosBuffer destination buffer
	 * @return Size of payload
	 */
	int32_t generateQoSPacket(const SharedPtr<Path>& path, int64_t now, char* qosBuffer);

	/**
	 * Record statistics for an inbound packet.
	 *
	 * @param path Path over which packet was received
	 * @param packetId Packet ID
	 * @param payloadLength Packet data length
	 * @param verb Packet verb
	 * @param flowId Flow ID
	 * @param now Current time
	 */
	void recordIncomingPacket(const SharedPtr<Path>& path, uint64_t packetId, uint16_t payloadLength, Packet::Verb verb, int32_t flowId, int64_t now);

	/**
	 * Determines the most appropriate path for packet and flow egress. This decision is made by
	 * the underlying bonding policy as well as QoS-related statistical observations of path quality.
	 *
	 * @param now Current time
	 * @param flowId Flow ID
	 * @return Pointer to suggested Path
	 */
	SharedPtr<Path> getAppropriatePath(int64_t now, int32_t flowId);

	/**
	 * Creates a new flow record
	 *
	 * @param path Path over which flow shall be handled
	 * @param flowId Flow ID
	 * @param entropy A byte of entropy to be used by the bonding algorithm
	 * @param now Current time
	 * @return Pointer to newly-created Flow
	 */
	SharedPtr<Flow> createFlow(const SharedPtr<Path>& path, int32_t flowId, unsigned char entropy, int64_t now);

	/**
	 * Removes flow records that are past a certain age limit.
	 *
	 * @param age Age threshold to be forgotten
	 * @param oldest Whether only the oldest shall be forgotten
	 * @param now Current time
	 */
	void forgetFlowsWhenNecessary(uint64_t age, bool oldest, int64_t now);

	/**
	 * Assigns a new flow to a bonded path
	 *
	 * @param flow Flow to be assigned
	 * @param now Current time
	 */
	bool assignFlowToBondedPath(SharedPtr<Flow>& flow, int64_t now);

	/**
	 * Determine whether a path change should occur given the remote peer's reported utility and our
	 * local peer's known utility. This has the effect of assigning inbound and outbound traffic to
	 * the same path.
	 *
	 * @param now Current time
	 * @param path Path over which the negotiation request was received
	 * @param remoteUtility How much utility the remote peer claims to gain by using the declared path
	 */
	void processIncomingPathNegotiationRequest(uint64_t now, SharedPtr<Path>& path, int16_t remoteUtility);

	/**
	 * Determine state of path synchronization and whether a negotiation request
	 * shall be sent to the peer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 */
	void pathNegotiationCheck(void* tPtr, const int64_t now);

	/**
	 * Sends a VERB_ACK to the remote peer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet should be sent
	 * @param localSocket Local source socket
	 * @param atAddress
	 * @param now Current time
	 */
	void sendACK(void* tPtr, const SharedPtr<Path>& path, int64_t localSocket, const InetAddress& atAddress, int64_t now);

	/**
	 * Sends a VERB_QOS_MEASUREMENT to the remote peer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet should be sent
	 * @param localSocket Local source socket
	 * @param atAddress
	 * @param now Current time
	 */
	void sendQOS_MEASUREMENT(void* tPtr, const SharedPtr<Path>& path, int64_t localSocket, const InetAddress& atAddress, int64_t now);

	/**
	 * Sends a VERB_PATH_NEGOTIATION_REQUEST to the remote peer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet should be sent
	 */
	void sendPATH_NEGOTIATION_REQUEST(void* tPtr, const SharedPtr<Path>& path);

	/**
	 *
	 * @param now Current time
	 */
	void processBalanceTasks(int64_t now);

	/**
	 * Perform periodic tasks unique to active-backup
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 */
	void processActiveBackupTasks(void* tPtr, int64_t now);

	/**
	 * Switches the active link in an active-backup scenario to the next best during
	 * a failover event.
	 *
	 * @param now Current time
	 */
	void dequeueNextActiveBackupPath(uint64_t now);

	/**
	 * Set bond parameters to reasonable defaults, these may later be overwritten by
	 * user-specified parameters.
	 *
	 * @param policy Bonding policy
	 * @param templateBond
	 */
	void setReasonableDefaults(int policy, SharedPtr<Bond> templateBond, bool useTemplate);

	/**
	 * Check and assign user-specified quality weights to this bond.
	 *
	 * @param weights Set of user-specified weights
	 * @param len Length of weight vector
	 */
	void setUserQualityWeights(float weights[], int len);

	/**
	 * @param latencyInMilliseconds Maximum acceptable latency.
	 */
	void setMaxAcceptableLatency(int16_t latencyInMilliseconds)
	{
		_maxAcceptableLatency = latencyInMilliseconds;
	}

	/**
	 * @param latencyInMilliseconds Maximum acceptable (mean) latency.
	 */
	void setMaxAcceptableMeanLatency(int16_t latencyInMilliseconds)
	{
		_maxAcceptableMeanLatency = latencyInMilliseconds;
	}

	/**
	 * @param latencyVarianceInMilliseconds Maximum acceptable packet delay variance (jitter).
	 */
	void setMaxAcceptablePacketDelayVariance(int16_t latencyVarianceInMilliseconds)
	{
		_maxAcceptablePacketDelayVariance = latencyVarianceInMilliseconds;
	}

	/**
	 * @param lossRatio Maximum acceptable packet loss ratio (PLR).
	 */
	void setMaxAcceptablePacketLossRatio(float lossRatio)
	{
		_maxAcceptablePacketLossRatio = lossRatio;
	}

	/**
	 * @param errorRatio Maximum acceptable packet error ratio (PER).
	 */
	void setMaxAcceptablePacketErrorRatio(float errorRatio)
	{
		_maxAcceptablePacketErrorRatio = errorRatio;
	}

	/**
	 * @param errorRatio Maximum acceptable packet error ratio (PER).
	 */
	void setMinAcceptableAllocation(float minAlloc)
	{
		_minAcceptableAllocation = minAlloc * 255;
	}

	/**
	 * @return Whether the user has defined links for use on this bond
	 */
	inline bool userHasSpecifiedLinks()
	{
		return _userHasSpecifiedLinks;
	}

	/**
	 * @return Whether the user has defined a set of failover link(s) for this bond
	 */
	inline bool userHasSpecifiedFailoverInstructions()
	{
		return _userHasSpecifiedFailoverInstructions;
	};

	/**
	 * @return Whether the user has specified a primary link
	 */
	inline bool userHasSpecifiedPrimaryLink()
	{
		return _userHasSpecifiedPrimaryLink;
	}

	/**
	 * @return Whether the user has specified link speeds
	 */
	inline bool userHasSpecifiedLinkSpeeds()
	{
		return _userHasSpecifiedLinkSpeeds;
	}

	/**
	 * Periodically perform maintenance tasks for each active bond.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 */
	void processBackgroundTasks(void* tPtr, int64_t now);

	/**
	 * Rate limit gate for VERB_ACK
	 *
	 * @param now Current time
	 * @return Whether the incoming packet should be rate-gated
	 */
	inline bool rateGateACK(const int64_t now)
	{
		_ackCutoffCount++;
		int numToDrain = _lastAckRateCheck ? (now - _lastAckRateCheck) / ZT_ACK_DRAINAGE_DIVISOR : _ackCutoffCount;
		_lastAckRateCheck = now;
		if (_ackCutoffCount > numToDrain) {
			_ackCutoffCount -= numToDrain;
		}
		else {
			_ackCutoffCount = 0;
		}
		return (_ackCutoffCount < ZT_ACK_CUTOFF_LIMIT);
	}

	/**
	 * Rate limit gate for VERB_QOS_MEASUREMENT
	 *
	 * @param now Current time
	 * @return Whether the incoming packet should be rate-gated
	 */
	inline bool rateGateQoS(const int64_t now)
	{
		_qosCutoffCount++;
		int numToDrain = (now - _lastQoSRateCheck) / ZT_QOS_DRAINAGE_DIVISOR;
		_lastQoSRateCheck = now;
		if (_qosCutoffCount > numToDrain) {
			_qosCutoffCount -= numToDrain;
		}
		else {
			_qosCutoffCount = 0;
		}
		return (_qosCutoffCount < ZT_QOS_CUTOFF_LIMIT);
	}

	/**
	 * Rate limit gate for VERB_PATH_NEGOTIATION_REQUEST
	 *
	 * @param now Current time
	 * @return Whether the incoming packet should be rate-gated
	 */
	inline bool rateGatePathNegotiation(const int64_t now)
	{
		if ((now - _lastPathNegotiationReceived) <= ZT_PATH_NEGOTIATION_CUTOFF_TIME)
			++_pathNegotiationCutoffCount;
		else
			_pathNegotiationCutoffCount = 0;
		_lastPathNegotiationReceived = now;
		return (_pathNegotiationCutoffCount < ZT_PATH_NEGOTIATION_CUTOFF_LIMIT);
	}

	/**
	 * @param interval Maximum amount of time user expects a failover to take on this bond.
	 */
	inline void setFailoverInterval(uint32_t interval)
	{
		_failoverInterval = interval;
	}

	/**
	 * @param interval Maximum amount of time user expects a failover to take on this bond.
	 */
	inline uint32_t getFailoverInterval()
	{
		return _failoverInterval;
	}

	/**
	 * @param strategy Strategy that the bond uses to re-assign protocol flows.
	 */
	inline void setFlowRebalanceStrategy(uint32_t strategy)
	{
		_flowRebalanceStrategy = strategy;
	}

	/**
	 * @param strategy Strategy that the bond uses to prob for path aliveness and quality
	 */
	inline void setLinkMonitorStrategy(uint8_t strategy)
	{
		_linkMonitorStrategy = strategy;
	}

	/**
	 * @param abOverflowEnabled Whether "overflow" mode is enabled for this active-backup bond
	 */
	inline void setOverflowMode(bool abOverflowEnabled)
	{
		_abOverflowEnabled = abOverflowEnabled;
	}

	/**
	 * @return the current up delay parameter
	 */
	inline uint16_t getUpDelay()
	{
		return _upDelay;
	}

	/**
	 * @param upDelay Length of time before a newly-discovered path is admitted to the bond
	 */
	inline void setUpDelay(int upDelay)
	{
		if (upDelay >= 0) {
			_upDelay = upDelay;
		}
	}

	/**
	 * @return Length of time before a newly-failed path is removed from the bond
	 */
	inline uint16_t getDownDelay()
	{
		return _downDelay;
	}

	/**
	 * @param downDelay Length of time before a newly-failed path is removed from the bond
	 */
	inline void setDownDelay(int downDelay)
	{
		if (downDelay >= 0) {
			_downDelay = downDelay;
		}
	}

	/**
	 * @return the current monitoring interval for the bond (can be overridden with intervals specific to certain links.)
	 */
	inline uint16_t getBondMonitorInterval()
	{
		return _bondMonitorInterval;
	}

	/**
	 * Set the current monitoring interval for the bond (can be overridden with intervals specific to certain links.)
	 *
	 * @param monitorInterval How often gratuitous VERB_HELLO(s) are sent to remote peer.
	 */
	inline void setBondMonitorInterval(uint16_t interval)
	{
		_bondMonitorInterval = interval;
	}

	/**
	 * @param policy Bonding policy for this bond
	 */
	inline void setPolicy(uint8_t policy)
	{
		_bondingPolicy = policy;
	}

	/**
	 * @return the current bonding policy
	 */
	inline uint8_t getPolicy()
	{
		return _bondingPolicy;
	}

	/**
	 * @return the health status of the bond
	 */
	inline bool isHealthy()
	{
		return _isHealthy;
	}

	/**
	 * @return the number of links comprising this bond which are considered alive
	 */
	inline uint8_t getNumAliveLinks()
	{
		return _numAliveLinks;
	};

	/**
	 * @return the number of links comprising this bond
	 */
	inline uint8_t getNumTotalLinks()
	{
		return _numTotalLinks;
	}

	/**
	 *
	 * @param allowFlowHashing
	 */
	inline void setFlowHashing(bool allowFlowHashing)
	{
		_allowFlowHashing = allowFlowHashing;
	}

	/**
	 * @return Whether flow-hashing is currently enabled for this bond.
	 */
	bool flowHashingEnabled()
	{
		return _allowFlowHashing;
	}

	/**
	 *
	 * @param packetsPerLink
	 */
	inline void setPacketsPerLink(int packetsPerLink)
	{
		_packetsPerLink = packetsPerLink;
	}

	/**
	 * @return Number of packets to be sent on each interface in a balance-rr bond
	 */
	inline int getPacketsPerLink()
	{
		return _packetsPerLink;
	}

	/**
	 *
	 * @param linkSelectMethod
	 */
	inline void setLinkSelectMethod(uint8_t method)
	{
		_abLinkSelectMethod = method;
	}

	/**
	 *
	 * @return
	 */
	inline uint8_t getLinkSelectMethod()
	{
		return _abLinkSelectMethod;
	}

	/**
	 *
	 * @param allowPathNegotiation
	 */
	inline void setAllowPathNegotiation(bool allowPathNegotiation)
	{
		_allowPathNegotiation = allowPathNegotiation;
	}

	/**
	 *
	 * @return
	 */
	inline bool allowPathNegotiation()
	{
		return _allowPathNegotiation;
	}

	/**
	 * Forcibly rotates the currently active link used in an active-backup bond to the next link in the failover queue
	 *
	 * @return True if this operation succeeded, false if otherwise
	 */
	bool abForciblyRotateLink();

	SharedPtr<Peer> getPeer()
	{
		return _peer;
	}

  private:
	const RuntimeEnvironment* RR;
	AtomicCounter __refCount;

	/**
	 * Custom name given by the user to this bond type.
	 */
	std::string _policyAlias;

	/**
	 * Paths that this bond has been made aware of but that are not necessarily
	 * part of the bond proper.
	 */
	SharedPtr<Path> _paths[ZT_MAX_PEER_NETWORK_PATHS];

	/**
	 * Set of indices corresponding to paths currently included in the bond proper. This
	 * may only be updated during a call to curateBond(). The reason for this is so that
	 * we can simplify the high frequency packet egress logic.
	 */
	int _bondedIdx[ZT_MAX_PEER_NETWORK_PATHS];

	/**
	 * Number of paths currently included in the _bondedIdx set.
	 */
	int _numBondedPaths;

	/**
	 * Flows hashed according to port and protocol
	 */
	std::map<int32_t, SharedPtr<Flow> > _flows;

	float _qualityWeights[ZT_QOS_WEIGHT_SIZE];	 // How much each factor contributes to the "quality" score of a path.

	uint8_t _bondingPolicy;
	uint32_t _upDelay;
	uint32_t _downDelay;

	// active-backup
	SharedPtr<Path> _abPath;   // current active path
	std::list<SharedPtr<Path> > _abFailoverQueue;
	uint8_t _abLinkSelectMethod;   // link re-selection policy for the primary link in active-backup
	bool _abOverflowEnabled;

	// balance-rr
	uint8_t _rrIdx;						 // index to path currently in use during Round Robin operation
	uint16_t _rrPacketsSentOnCurrLink;	 // number of packets sent on this link since the most recent path switch.
	/**
	 * How many packets will be sent on a path before moving to the next path
	 * in the round-robin sequence. A value of zero will cause a random path
	 * selection for each outgoing packet.
	 */
	int _packetsPerLink;

	// balance-aware
	uint64_t _totalBondUnderload;
	uint8_t _flowRebalanceStrategy;

	// dynamic link monitoring
	uint8_t _linkMonitorStrategy;
	uint32_t _dynamicPathMonitorInterval;

	// path negotiation
	int16_t _localUtility;
	SharedPtr<Path> negotiatedPath;
	uint8_t _numSentPathNegotiationRequests;
	unsigned int _pathNegotiationCutoffCount;
	bool _allowPathNegotiation;

	/**
	 * Timers and intervals
	 */
	uint32_t _failoverInterval;
	uint32_t _qosSendInterval;
	uint32_t _ackSendInterval;
	uint32_t throughputMeasurementInterval;
	uint32_t _qualityEstimationInterval;

	/**
	 * Acceptable quality thresholds
	 */
	float _maxAcceptablePacketLossRatio;
	float _maxAcceptablePacketErrorRatio;
	uint16_t _maxAcceptableLatency;
	uint16_t _maxAcceptableMeanLatency;
	uint16_t _maxAcceptablePacketDelayVariance;
	uint8_t _minAcceptableAllocation;

	/**
	 * Link state reporting
	 */
	bool _isHealthy;
	uint8_t _numAliveLinks;
	uint8_t _numTotalLinks;

	/**
	 * Default initial punishment inflicted on misbehaving paths. Punishment slowly
	 * drains linearly. For each eligibility change the remaining punishment is doubled.
	 */
	uint32_t _defaultPathRefractoryPeriod;

	/**
	 * Whether the current bonding policy requires computation of path statistics
	 */
	bool _shouldCollectPathStatistics;

	/**
	 * Free byte of entropy that is updated on every packet egress event.
	 */
	unsigned char _freeRandomByte;

	/**
	 * Remote peer that this bond services
	 */
	SharedPtr<Peer> _peer;

	/**
	 * Rate-limit cutoffs
	 */
	uint16_t _qosCutoffCount;
	uint16_t _ackCutoffCount;

	/**
	 * Recent event timestamps
	 */
	uint64_t _lastAckRateCheck;
	uint64_t _lastQoSRateCheck;
	uint64_t _lastQualityEstimation;
	uint64_t _lastCheckUserPreferences;
	uint64_t _lastBackgroundTaskCheck;
	uint64_t _lastBondStatusLog;
	uint64_t _lastPathNegotiationReceived;
	uint64_t _lastPathNegotiationCheck;
	uint64_t _lastSentPathNegotiationRequest;
	uint64_t _lastFlowStatReset;
	uint64_t _lastFlowExpirationCheck;
	uint64_t _lastFlowRebalance;
	uint64_t _lastFrame;
	uint64_t _lastActiveBackupPathChange;

	Mutex _paths_m;
	Mutex _flows_m;

	/**
	 * Whether the user has specified links for this bond.
	 */
	bool _userHasSpecifiedLinks;

	/**
	 * Whether the user has specified a primary link for this bond.
	 */
	bool _userHasSpecifiedPrimaryLink;

	/**
	 * Whether the user has specified failover instructions for this bond.
	 */
	bool _userHasSpecifiedFailoverInstructions;

	/**
	 * Whether the user has specified links speeds for this bond.
	 */
	bool _userHasSpecifiedLinkSpeeds;

	/**
	 * How frequently (in ms) a VERB_ECHO is sent to a peer to verify that a
	 * path is still active. A value of zero (0) will disable active path
	 * monitoring; as result, all monitoring will be a function of traffic.
	 */
	uint16_t _bondMonitorInterval;

	/**
	 * Whether or not flow hashing is allowed.
	 */
	bool _allowFlowHashing;
};

}	// namespace ZeroTier

#endif
