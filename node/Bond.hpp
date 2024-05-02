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

#ifndef ZT_BOND_HPP
#define ZT_BOND_HPP

#include "../osdep/Binder.hpp"
#include "../osdep/Phy.hpp"
#include "Packet.hpp"
#include "Path.hpp"
#include "RuntimeEnvironment.hpp"
#include "Trace.hpp"

#include <cstdarg>
#include <deque>
#include <list>
#include <map>

/**
 * Indices for the path quality weight vector
 */
enum ZT_BondQualityWeightIndex { ZT_QOS_LAT_MAX_IDX, ZT_QOS_PDV_MAX_IDX, ZT_QOS_PLR_MAX_IDX, ZT_QOS_PER_MAX_IDX, ZT_QOS_LAT_WEIGHT_IDX, ZT_QOS_PDV_WEIGHT_IDX, ZT_QOS_PLR_WEIGHT_IDX, ZT_QOS_PER_WEIGHT_IDX, ZT_QOS_PARAMETER_SIZE };

/**
 *  Multipath bonding policy
 */
enum ZT_BondBondingPolicy {
	/**
	 * Normal operation. No fault tolerance, no load balancing
	 */
	ZT_BOND_POLICY_NONE = 0,

	/**
	 * Sends traffic out on only one path at a time. Configurable immediate
	 * fail-over.
	 */
	ZT_BOND_POLICY_ACTIVE_BACKUP = 1,

	/**
	 * Sends traffic out on all paths
	 */
	ZT_BOND_POLICY_BROADCAST = 2,

	/**
	 * Stripes packets across all paths
	 */
	ZT_BOND_POLICY_BALANCE_RR = 3,

	/**
	 * Packets destined for specific peers will always be sent over the same
	 * path.
	 */
	ZT_BOND_POLICY_BALANCE_XOR = 4,

	/**
	 * Balances flows among all paths according to path performance
	 */
	ZT_BOND_POLICY_BALANCE_AWARE = 5
};

/**
 * Multipath active re-selection policy (linkSelectMethod)
 */
enum ZT_BondLinkSelectMethod {
	/**
	 * Primary link regains status as active link whenever it comes back up
	 * (default when links are explicitly specified)
	 */
	ZT_BOND_RESELECTION_POLICY_ALWAYS = 0,

	/**
	 * Primary link regains status as active link when it comes back up and
	 * (if) it is better than the currently-active link.
	 */
	ZT_BOND_RESELECTION_POLICY_BETTER = 1,

	/**
	 * Primary link regains status as active link only if the currently-active
	 * link fails.
	 */
	ZT_BOND_RESELECTION_POLICY_FAILURE = 2,

	/**
	 * The primary link can change if a superior path is detected.
	 * (default if user provides no fail-over guidance)
	 */
	ZT_BOND_RESELECTION_POLICY_OPTIMIZE = 3
};

/**
 * Mode of multipath link interface
 */
enum ZT_BondLinkMode { ZT_BOND_SLAVE_MODE_PRIMARY = 0, ZT_BOND_SLAVE_MODE_SPARE = 1 };

#include "../node/AtomicCounter.hpp"
#include "../node/SharedPtr.hpp"

#include <string>

namespace ZeroTier {

class Link {
	friend class SharedPtr<Link>;

  public:
	/**
	 *
	 * @param ifnameStr
	 * @param ipvPref
	 * @param capacity
	 * @param enabled
	 * @param mode
	 * @param failoverToLinkStr
	 */
	Link(std::string ifnameStr, uint8_t ipvPref, uint16_t mtu, uint32_t capacity, bool enabled, uint8_t mode, std::string failoverToLinkStr)
		: _ifnameStr(ifnameStr)
		, _ipvPref(ipvPref)
		, _mtu(mtu)
		, _capacity(capacity)
		, _relativeCapacity(0.0)
		, _enabled(enabled)
		, _mode(mode)
		, _failoverToLinkStr(failoverToLinkStr)
		, _isUserSpecified(false)
	{
	}

	/**
	 * @return The string representation of this link's underlying interface's system name.
	 */
	inline std::string ifname()
	{
		return _ifnameStr;
	}

	/**
	 * @return Whether this link is designated as a primary.
	 */
	inline bool primary()
	{
		return _mode == ZT_BOND_SLAVE_MODE_PRIMARY;
	}

	/**
	 * @return Whether this link is designated as a spare.
	 */
	inline bool spare()
	{
		return _mode == ZT_BOND_SLAVE_MODE_SPARE;
	}

	/**
	 * @return The name of the link interface that should be used in the event of a failure.
	 */
	inline std::string failoverToLink()
	{
		return _failoverToLinkStr;
	}

	/**
	 * @return Whether this link interface was specified by the user or auto-detected.
	 */
	inline bool isUserSpecified()
	{
		return _isUserSpecified;
	}

	/**
	 * Signify that this link was specified by the user and not the result of auto-detection.
	 *
	 * @param isUserSpecified
	 */
	inline void setAsUserSpecified(bool isUserSpecified)
	{
		_isUserSpecified = isUserSpecified;
	}

	/**
	 * @return Whether or not the user has specified failover instructions.
	 */
	inline bool userHasSpecifiedFailoverInstructions()
	{
		return _failoverToLinkStr.length();
	}

	/**
	 * @return The capacity of the link relative to others in the bond.
	 */
	inline float relativeCapacity()
	{
		return _relativeCapacity;
	}

	/**
	 * Sets the capacity of the link relative to others in the bond.
	 *
	 * @param relativeCapacity The capacity relative to the rest of the link.
	 */
	inline void setRelativeCapacity(float relativeCapacity)
	{
		_relativeCapacity = relativeCapacity;
	}

	/**
	 * @return The absolute capacity of the link (as specified by the user.)
	 */
	inline uint32_t capacity()
	{
		return _capacity;
	}

	/**
	 * @return The address preference for this link (as specified by the user.)
	 */
	inline uint8_t ipvPref()
	{
		return _ipvPref;
	}

	/**
	 * @return The MTU for this link (as specified by the user.)
	 */
	inline uint16_t mtu()
	{
		return _mtu;
	}

	/**
	 * @return The mode (e.g. primary/spare) for this link (as specified by the user.)
	 */
	inline uint8_t mode()
	{
		return _mode;
	}

	/**
	 * @return Whether this link is enabled or disabled
	 */
	inline uint8_t enabled()
	{
		return _enabled;
	}

  private:
	/**
	 * String representation of underlying interface's system name
	 */
	std::string _ifnameStr;

	/**
	 * What preference (if any) a user has for IP protocol version used in
	 * path aggregations. Preference is expressed in the order of the digits:
	 *
	 *  0: no preference
	 *  4: IPv4 only
	 *  6: IPv6 only
	 * 46: IPv4 over IPv6
	 * 64: IPv6 over IPv4
	 */
	uint8_t _ipvPref;

	/**
	 * The physical-layer MTU for this link
	 */
	uint16_t _mtu;

	/**
	 * User-specified capacity of this link
	 */
	uint32_t _capacity;

	/**
	 * Speed relative to other specified links (computed by Bond)
	 */
	float _relativeCapacity;

	/**
	 * Whether this link is enabled, or (disabled (possibly bad config))
	 */
	uint8_t _enabled;

	/**
	 * Whether this link is designated as a primary, a spare, or no preference.
	 */
	uint8_t _mode;

	/**
	 * The specific name of the link to be used in the event that this
	 * link fails.
	 */
	std::string _failoverToLinkStr;

	/**
	 * Whether or not this link was created as a result of manual user specification. This is
	 * important to know because certain policy decisions are dependent on whether the user
	 * intents to use a specific set of interfaces.
	 */
	bool _isUserSpecified;

	AtomicCounter __refCount;
};

class Link;
class Peer;

class Bond {
  public:
	/**
	 * Stop bond's internal functions (can be resumed)
	 */
	void stopBond();

	/**
	 * Start or resume a bond's internal functions
	 */
	void startBond();

	/**
	 * @return Whether this link is permitted to become a member of a bond.
	 */
	static bool linkAllowed(std::string& policyAlias, SharedPtr<Link> link);

	/**
	 * @return The minimum interval required to poll the active bonds to fulfill all active monitoring timing requirements.
	 */
	static int minReqMonitorInterval()
	{
		return _minReqMonitorInterval;
	}

	/**
	 * @return Whether the bonding layer is currently set up to be used.
	 */
	static bool inUse()
	{
		return ! _bondPolicyTemplates.empty() || _defaultPolicy;
	}

	/**
	 * Sets a pointer to an instance of _binder used by the Bond to get interface data
	 */
	static void setBinder(Binder* b)
	{
		_binder = b;
	}

	/**
	 * @param basePolicyName Bonding policy name (See ZeroTierOne.h)
	 * @return The bonding policy code for a given human-readable bonding policy name
	 */
	static int getPolicyCodeByStr(const std::string& basePolicyName)
	{
		if (basePolicyName == "active-backup") {
			return 1;
		}
		if (basePolicyName == "broadcast") {
			return 2;
		}
		if (basePolicyName == "balance-rr") {
			return 3;
		}
		if (basePolicyName == "balance-xor") {
			return 4;
		}
		if (basePolicyName == "balance-aware") {
			return 5;
		}
		return 0;	// "none"
	}

	/**
	 * @param policy Bonding policy code (See ZeroTierOne.h)
	 * @return The human-readable name for the given bonding policy code
	 */
	static std::string getPolicyStrByCode(int policy)
	{
		if (policy == 1) {
			return "active-backup";
		}
		if (policy == 2) {
			return "broadcast";
		}
		if (policy == 3) {
			return "balance-rr";
		}
		if (policy == 4) {
			return "balance-xor";
		}
		if (policy == 5) {
			return "balance-aware";
		}
		return "none";
	}

	/**
	 * Sets the default bonding policy for new or undefined bonds.
	 *
	 * @param bp Bonding policy
	 */
	static void setBondingLayerDefaultPolicy(uint8_t bp)
	{
		_defaultPolicy = bp;
	}

	/**
	 * Sets the default (custom) bonding policy for new or undefined bonds.
	 *
	 * @param alias Human-readable string alias for bonding policy
	 */
	static void setBondingLayerDefaultPolicyStr(std::string alias)
	{
		_defaultPolicyStr = alias;
	}

	/**
	 * Add a user-defined link to a given bonding policy.
	 *
	 * @param policyAlias User-defined custom name for variant of bonding policy
	 * @param link Pointer to new link definition
	 */
	static void addCustomLink(std::string& policyAlias, SharedPtr<Link> link);

	/**
	 * Add a user-defined bonding policy that is based on one of the standard types.
	 *
	 * @param newBond Pointer to custom Bond object
	 * @return Whether a uniquely-named custom policy was successfully added
	 */
	static bool addCustomPolicy(const SharedPtr<Bond>& newBond);

	/**
	 * Assigns a specific bonding policy
	 *
	 * @param identity
	 * @param policyAlias
	 * @return
	 */
	static bool assignBondingPolicyToPeer(int64_t identity, const std::string& policyAlias);

	/**
	 * Get pointer to bond by a given peer ID
	 *
	 * @param peer Remote peer ID
	 * @return A pointer to the Bond
	 */
	static SharedPtr<Bond> getBondByPeerId(int64_t identity);

	/**
	 * Set MTU for link by given interface name and IP address (across all bonds)
	 *
	 * @param mtu MTU to be used on this link
	 * @param ifStr interface name to match
	 * @param ipStr IP address to match
	 * @return Whether the MTU was set
	 */
	static bool setAllMtuByTuple(uint16_t mtu, const std::string& ifStr, const std::string& ipStr);

	/**
	 * Set MTU for link by given interface name and IP address
	 *
	 * @param mtu MTU to be used on this link
	 * @param ifStr interface name to match
	 * @param ipStr IP address to match
	 * @return Whether the MTU was set
	 */
	bool setMtuByTuple(uint16_t mtu, const std::string& ifStr, const std::string& ipStr);

	/**
	 * Add a new bond to the bond controller.
	 *
	 * @param renv Runtime environment
	 * @param peer Remote peer that this bond services
	 * @return A pointer to the newly created Bond
	 */
	static SharedPtr<Bond> createBond(const RuntimeEnvironment* renv, const SharedPtr<Peer>& peer);

	/**
	 * Remove a bond from the bond controller.
	 *
	 * @param peerId Remote peer that this bond services
	 */
	static void destroyBond(uint64_t peerId);

	/**
	 * Periodically perform maintenance tasks for the bonding layer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 */
	static void processBackgroundTasks(void* tPtr, int64_t now);

	/**
	 * Gets a reference to a physical link definition given a policy alias and a local socket.
	 *
	 * @param policyAlias Policy in use
	 * @param localSocket Local source socket
	 * @param createIfNeeded Whether a Link object is created if the name wasn't previously in the link map
	 * @return Physical link definition
	 */
	SharedPtr<Link> getLinkBySocket(const std::string& policyAlias, uint64_t localSocket, bool createIfNeeded);

	/**
	 * Gets a reference to a physical link definition given its human-readable system name.
	 *
	 * @param policyAlias Policy in use
	 * @param ifname Alphanumeric human-readable name
	 * @return Physical link definition
	 */
	static SharedPtr<Link> getLinkByName(const std::string& policyAlias, const std::string& ifname);

  private:
	static Phy<Bond*>* _phy;

	static Mutex _bonds_m;
	static Mutex _links_m;

	/**
	 * The minimum required monitoring interval among all bonds
	 */
	static int _minReqMonitorInterval;

	/**
	 * The default bonding policy used for new bonds unless otherwise specified.
	 */
	static uint8_t _defaultPolicy;

	/**
	 * The default bonding policy used for new bonds unless otherwise specified.
	 */
	static std::string _defaultPolicyStr;

	/**
	 * All currently active bonds.
	 */
	static std::map<int64_t, SharedPtr<Bond> > _bonds;

	/**
	 * Map of peers to custom bonding policies
	 */
	static std::map<int64_t, std::string> _policyTemplateAssignments;

	/**
	 * User-defined bonding policies (can be assigned to a peer)
	 */
	static std::map<std::string, SharedPtr<Bond> > _bondPolicyTemplates;

	/**
	 * Set of links defined for a given bonding policy
	 */
	static std::map<std::string, std::vector<SharedPtr<Link> > > _linkDefinitions;

	/**
	 * Set of link objects mapped to their physical interfaces
	 */
	static std::map<std::string, std::map<std::string, SharedPtr<Link> > > _interfaceToLinkMap;

	struct NominatedPath;
	struct Flow;

	friend class SharedPtr<Bond>;
	friend class Peer;

  public:
	void dumpInfo(int64_t now, bool force);
	std::string pathToStr(const SharedPtr<Path>& path);
	void dumpPathStatus(int64_t now, int pathIdx);

	SharedPtr<Link> getLink(const SharedPtr<Path>& path);

	/**
	 * Constructor
	 *
	 *
	 */
	Bond(const RuntimeEnvironment* renv);

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
	 * Return whether this bond is able to properly process traffic
	 */
	bool isReady()
	{
		return _numBondedPaths;
	}

	/**
	 * Inform the bond about the path that its peer (owning object) just learned about.
	 * If the path is allowed to be used, it will be inducted into the bond on a trial
	 * period where link statistics will be collected to judge its quality.
	 *
	 * @param path Newly-learned Path which should now be handled by the Bond
	 * @param now Current time
	 */
	void nominatePathToBond(const SharedPtr<Path>& path, int64_t now);

	/**
	 * Add a nominated path to the bond. This merely maps the index from the nominated set
	 * to a smaller set and sets the path's bonded flag to true.
	 *
	 * @param nominatedIdx The index in the nominated set
	 * @param bondedIdx The index in the bonded set (subset of nominated)
	 */
	void addPathToBond(int nominatedIdx, int bondedIdx);

	/**
	 * Check path states and perform bond rebuilds if needed.
	 *
	 * @param now Current time
	 * @param rebuild Whether or not the bond should be reconstructed.
	 */
	void curateBond(int64_t now, bool rebuild);

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
	 * @param pathIdx Path over which packet was received
	 * @param now Current time
	 * @param ackedBytes Number of bytes ACKed by this VERB_ACK
	 */
	void receivedAck(int pathIdx, int64_t now, int32_t ackedBytes);

	/**
	 * Generate the contents of a VERB_QOS_MEASUREMENT packet.
	 *
	 * @param now Current time
	 * @param qosBuffer destination buffer
	 * @return Size of payload
	 */
	int32_t generateQoSPacket(int pathIdx, int64_t now, char* qosBuffer);

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
	 * @param np Path over which flow shall be handled
	 * @param flowId Flow ID
	 * @param entropy A byte of entropy to be used by the bonding algorithm
	 * @param now Current time
	 * @return Pointer to newly-created Flow
	 */
	SharedPtr<Flow> createFlow(int pathIdx, int32_t flowId, unsigned char entropy, int64_t now);

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
	 * @param reassign Whether this flow is being re-assigned to another path
	 */
	bool assignFlowToBondedPath(SharedPtr<Flow>& flow, int64_t now, bool reassign);

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
	void pathNegotiationCheck(void* tPtr, int64_t now);

	/**
	 * Sends a VERB_ACK to the remote peer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet should be sent
	 * @param localSocket Local source socket
	 * @param atAddress
	 * @param now Current time
	 */
	void sendACK(void* tPtr, int pathIdx, int64_t localSocket, const InetAddress& atAddress, int64_t now);

	/**
	 * Sends a VERB_QOS_MEASUREMENT to the remote peer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet should be sent
	 * @param localSocket Local source socket
	 * @param atAddress
	 * @param now Current time
	 */
	void sendQOS_MEASUREMENT(void* tPtr, int pathIdx, int64_t localSocket, const InetAddress& atAddress, int64_t now);

	/**
	 * Sends a VERB_PATH_NEGOTIATION_REQUEST to the remote peer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet should be sent
	 */
	void sendPATH_NEGOTIATION_REQUEST(void* tPtr, int pathIdx);

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
	 * Zero all timers
	 */
	void initTimers();

	/**
	 * Set bond parameters to reasonable defaults, these may later be overwritten by
	 * user-specified parameters.
	 *
	 * @param policy Bonding policy
	 * @param templateBond
	 */
	void setBondParameters(int policy, SharedPtr<Bond> templateBond, bool useTemplate);

	/**
	 * Check and assign user-specified link quality parameters to this bond.
	 *
	 * @param weights Set of user-specified parameters
	 * @param len Length of parameter vector
	 */
	void setUserLinkQualitySpec(float weights[], int len);

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
	 * @return Whether the user has specified link capacities
	 */
	inline bool userHasSpecifiedLinkCapacities()
	{
		return _userHasSpecifiedLinkCapacities;
	}

	/**
	 * Periodically perform maintenance tasks for each active bond.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 */
	void processBackgroundBondTasks(void* tPtr, int64_t now);

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
	inline bool rateGateQoS(int64_t now, SharedPtr<Path>& path)
	{
		char pathStr[64] = { 0 };
		path->address().toString(pathStr);
		uint64_t diff = now - _lastQoSRateCheck;
		if ((diff) <= (_qosSendInterval / ZT_MAX_PEER_NETWORK_PATHS)) {
			++_qosCutoffCount;
		}
		else {
			_qosCutoffCount = 0;
		}
		_lastQoSRateCheck = now;
		return (_qosCutoffCount < (ZT_MAX_PEER_NETWORK_PATHS * 2));
	}

	/**
	 * Rate limit gate for VERB_PATH_NEGOTIATION_REQUEST
	 *
	 * @param now Current time
	 * @return Whether the incoming packet should be rate-gated
	 */
	inline bool rateGatePathNegotiation(int64_t now, SharedPtr<Path>& path)
	{
		char pathStr[64] = { 0 };
		path->address().toString(pathStr);
		int diff = now - _lastPathNegotiationReceived;
		if ((diff) <= (ZT_PATH_NEGOTIATION_CUTOFF_TIME / ZT_MAX_PEER_NETWORK_PATHS)) {
			++_pathNegotiationCutoffCount;
		}
		else {
			_pathNegotiationCutoffCount = 0;
		}
		_lastPathNegotiationReceived = now;
		return (_pathNegotiationCutoffCount < (ZT_MAX_PEER_NETWORK_PATHS * 2));
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
	 * @param strategy Strategy that the bond uses to prob for path aliveness and quality
	 */
	inline void setLinkMonitorStrategy(uint8_t strategy)
	{
		_linkMonitorStrategy = strategy;
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
	 * @return The current monitoring interval for the bond
	 */
	inline int monitorInterval()
	{
		return _monitorInterval;
	}

	/**
	 * Set the current monitoring interval for the bond (can be overridden with intervals specific to certain links.)
	 *
	 * @param monitorInterval How often gratuitous VERB_HELLO(s) are sent to remote peer.
	 */
	inline void setBondMonitorInterval(uint16_t interval)
	{
		_monitorInterval = interval;
	}

	/**
	 * @param policy Bonding policy for this bond
	 */

	inline void setPolicy(uint8_t policy)
	{
		_policy = policy;
	}

	/**
	 * @return the current bonding policy
	 */
	inline uint8_t policy()
	{
		return _policy;
	}

	/**
	 * @return the number of links in this bond which are considered alive
	 */
	inline uint8_t getNumAliveLinks()
	{
		return _numAliveLinks;
	};

	/**
	 * @return the number of links in this bond
	 */
	inline uint8_t getNumTotalLinks()
	{
		return _numTotalLinks;
	}

	/**
	 * @return Whether flow-hashing is currently supported for this bond.
	 */
	bool flowHashingSupported()
	{
		return _policy == ZT_BOND_POLICY_BALANCE_XOR || _policy == ZT_BOND_POLICY_BALANCE_AWARE;
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

	/**
	 * Emit message to tracing system but with added timestamp and subsystem info
	 */
	void log(const char* fmt, ...)
#ifdef __GNUC__
		__attribute__((format(printf, 2, 3)))
#endif
	{
#ifdef ZT_TRACE
		time_t rawtime;
		struct tm* timeinfo;
		char timestamp[80];
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		strftime(timestamp, 80, "%F %T", timeinfo);
#define MAX_BOND_MSG_LEN 1024
		char traceMsg[MAX_BOND_MSG_LEN];
		char userMsg[MAX_BOND_MSG_LEN];
		va_list args;
		va_start(args, fmt);
		if (vsnprintf(userMsg, sizeof(userMsg), fmt, args) < 0) {
			fprintf(stderr, "Encountered format encoding error while writing to trace log\n");
			return;
		}
		snprintf(traceMsg, MAX_BOND_MSG_LEN, "%s (%llx/%s) %s", timestamp, _peerId, _policyAlias.c_str(), userMsg);
		va_end(args);
		RR->t->bondStateMessage(NULL, traceMsg);
#undef MAX_MSG_LEN
#endif
	}

	/**
	 * Emit message to tracing system but with added timestamp and subsystem info
	 */
	void debug(const char* fmt, ...)
#ifdef __GNUC__
		__attribute__((format(printf, 2, 3)))
#endif
	{
#ifdef ZT_DEBUG
		time_t rawtime;
		struct tm* timeinfo;
		char timestamp[80];
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		strftime(timestamp, 80, "%F %T", timeinfo);
#define MAX_BOND_MSG_LEN 1024
		char traceMsg[MAX_BOND_MSG_LEN];
		char userMsg[MAX_BOND_MSG_LEN];
		va_list args;
		va_start(args, fmt);
		if (vsnprintf(userMsg, sizeof(userMsg), fmt, args) < 0) {
			fprintf(stderr, "Encountered format encoding error while writing to trace log\n");
			return;
		}
		snprintf(traceMsg, MAX_BOND_MSG_LEN, "%s (%llx/%s) %s", timestamp, _peerId, _policyAlias.c_str(), userMsg);
		va_end(args);
		RR->t->bondStateMessage(NULL, traceMsg);
#undef MAX_MSG_LEN
#endif
	}

  private:
	struct NominatedPath {
		NominatedPath()
			: lastAckSent(0)
			, lastAckReceived(0)
			, lastQoSReceived(0)
			, unackedBytes(0)
			, packetsReceivedSinceLastAck(0)
			, lastQoSMeasurement(0)
			, lastThroughputEstimation(0)
			, lastRefractoryUpdate(0)
			, lastAliveToggle(0)
			, alive(false)
			, eligible(true)
			, lastEligibility(0)
			, whenNominated(0)
			, refractoryPeriod(0)
			, ipvPref(0)
			, mode(0)
			, onlyPathOnLink(false)
			, bonded(false)
			, negotiated(false)
			, shouldAvoid(false)
			, assignedFlowCount(0)
			, latency(0)
			, latencyVariance(0)
			, packetLossRatio(0)
			, packetErrorRatio(0)
			, relativeQuality(0)
			, relativeLinkCapacity(0)
			, failoverScore(0)
			, packetsReceivedSinceLastQoS(0)
			, packetsIn(0)
			, packetsOut(0)
			, localPort(0)
		{
		}

		/**
		 * Set or update a refractory period for the path.
		 *
		 * @param punishment How much a path should be punished
		 * @param pathFailure Whether this call is the result of a recent path failure
		 */
		inline void adjustRefractoryPeriod(int64_t now, uint32_t punishment, bool pathFailure)
		{
			if (pathFailure) {
				unsigned int suggestedRefractoryPeriod = refractoryPeriod ? punishment + (refractoryPeriod * 2) : punishment;
				refractoryPeriod = std::min(suggestedRefractoryPeriod, (unsigned int)ZT_BOND_MAX_REFRACTORY_PERIOD);
				lastRefractoryUpdate = 0;
			}
			else {
				uint32_t drainRefractory = 0;
				if (lastRefractoryUpdate) {
					drainRefractory = (now - lastRefractoryUpdate);
				}
				else {
					drainRefractory = (now - lastAliveToggle);
				}
				lastRefractoryUpdate = now;
				if (refractoryPeriod > drainRefractory) {
					refractoryPeriod -= drainRefractory;
				}
				else {
					refractoryPeriod = 0;
					lastRefractoryUpdate = 0;
				}
			}
		}

		/**
		 * @return True if a path is permitted to be used in a bond (according to user pref.)
		 */
		inline bool allowed()
		{
			return (! ipvPref || ((p->_addr.isV4() && (ipvPref == 4 || ipvPref == 46 || ipvPref == 64)) || ((p->_addr.isV6() && (ipvPref == 6 || ipvPref == 46 || ipvPref == 64)))));
		}

		/**
		 * @return True if a path exists on a link marked as a spare
		 */
		inline bool isSpare()
		{
			return mode == ZT_BOND_SLAVE_MODE_SPARE;
		}

		/**
		 * @return True if a path is preferred over another on the same physical link (according to user pref.)
		 */
		inline bool preferred()
		{
			return onlyPathOnLink || (p->_addr.isV4() && (ipvPref == 4 || ipvPref == 46)) || (p->_addr.isV6() && (ipvPref == 6 || ipvPref == 64));
		}

		/**
		 * @param now Current time
		 * @return Whether a QoS (VERB_QOS_MEASUREMENT) packet needs to be emitted at this time
		 */
		inline bool needsToSendQoS(int64_t now, uint64_t qosSendInterval)
		{
			return ((packetsReceivedSinceLastQoS >= ZT_QOS_TABLE_SIZE) || ((now - lastQoSMeasurement) > qosSendInterval)) && packetsReceivedSinceLastQoS;
		}

		/**
		 * @param now Current time
		 * @return Whether an ACK (VERB_ACK) packet needs to be emitted at this time
		 */
		inline bool needsToSendAck(int64_t now, uint64_t ackSendInterval)
		{
			return ((now - lastAckSent) >= ackSendInterval || (packetsReceivedSinceLastAck == ZT_QOS_TABLE_SIZE)) && packetsReceivedSinceLastAck;
		}

		/**
		 * Reset packet counters
		 */
		inline void resetPacketCounts()
		{
			packetsIn = 0;
			packetsOut = 0;
		}

		std::map<uint64_t, uint64_t> qosStatsOut;	// id:egress_time
		std::map<uint64_t, uint64_t> qosStatsIn;	// id:now
		std::map<uint64_t, uint64_t> ackStatsIn;	// id:now

		RingBuffer<int, ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> qosRecordSize;
		RingBuffer<float, ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> qosRecordLossSamples;
		RingBuffer<uint64_t, ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> throughputSamples;
		RingBuffer<bool, ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> packetValiditySamples;
		RingBuffer<float, ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> throughputVarianceSamples;
		RingBuffer<uint16_t, ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE> latencySamples;

		uint64_t lastAckSent;
		uint64_t lastAckReceived;
		uint64_t lastQoSReceived;
		uint64_t unackedBytes;
		uint64_t packetsReceivedSinceLastAck;

		uint64_t lastQoSMeasurement;		 // Last time that a VERB_QOS_MEASUREMENT was sent out on this path.
		uint64_t lastThroughputEstimation;	 // Last time that the path's throughput was estimated.
		uint64_t lastRefractoryUpdate;		 // The last time that the refractory period was updated.
		uint64_t lastAliveToggle;			 // The last time that the path was marked as "alive".
		bool alive;
		bool eligible;				  // State of eligibility at last check. Used for determining state changes.
		uint64_t lastEligibility;	  // The last time that this path was eligible
		uint64_t whenNominated;		  // Timestamp indicating when this path's trial period began.
		uint32_t refractoryPeriod;	  // Amount of time that this path will be prevented from becoming a member of a bond.
		uint8_t ipvPref;			  // IP version preference inherited from the physical link.
		uint8_t mode;				  // Mode inherited from the physical link.
		bool onlyPathOnLink;		  // IP version preference inherited from the physical link.
		bool enabled;				  // Enabled state inherited from the physical link.
		bool bonded;				  // Whether this path is currently part of a bond.
		bool negotiated;			  // Whether this path was intentionally negotiated by either peer.
		bool shouldAvoid;			  // Whether flows should be moved from this path. Current traffic flows will be re-allocated immediately.
		uint16_t assignedFlowCount;	  // The number of flows currently assigned to this path.
		float latency;				  // The mean latency (computed from a sliding window.)
		float latencyVariance;		  // Packet delay variance (computed from a sliding window.)
		float packetLossRatio;		  // The ratio of lost packets to received packets.
		float packetErrorRatio;		  // The ratio of packets that failed their MAC/CRC checks to those that did not.
		float relativeQuality;		  // The relative quality of the link.
		float relativeLinkCapacity;	  // The relative capacity of the link.

		uint32_t failoverScore;				   // Score that indicates to what degree this path is preferred over others that are available to the bonding policy. (specifically for active-backup)
		int32_t packetsReceivedSinceLastQoS;   // Number of packets received since the last VERB_QOS_MEASUREMENT was sent to the remote peer.

		/**
		 * Counters used for tracking path load.
		 */
		int packetsIn;
		int packetsOut;

		uint16_t localPort;

		// AtomicCounter __refCount;

		SharedPtr<Path> p;
		void set(uint64_t now, const SharedPtr<Path>& path)
		{
			p = path;
			whenNominated = now;
		}
	};

	/**
	 * Paths nominated to the bond (may or may not actually be bonded)
	 */
	NominatedPath _paths[ZT_MAX_PEER_NETWORK_PATHS];

	inline int getNominatedPathIdx(const SharedPtr<Path>& path)
	{
		for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (_paths[i].p == path) {
				return i;
			}
		}
		return ZT_MAX_PEER_NETWORK_PATHS;
	}

	/**
	 * A protocol flow that is identified by the origin and destination port.
	 */
	struct Flow {
		/**
		 * @param flowId Given flow ID
		 * @param now Current time
		 */
		Flow(int32_t flowId, int64_t now) : id(flowId), bytesIn(0), bytesOut(0), lastActivity(now), lastPathReassignment(0), assignedPath(ZT_MAX_PEER_NETWORK_PATHS)
		{
		}

		/**
		 * Reset flow statistics
		 */
		inline void resetByteCounts()
		{
			bytesIn = 0;
			bytesOut = 0;
		}

		/**
		 * How long since a packet was sent or received in this flow
		 *
		 * @param now Current time
		 * @return The age of the flow in terms of last recorded activity
		 */
		int64_t age(int64_t now)
		{
			return now - lastActivity;
		}

		/**
		 * @param path Assigned path over which this flow should be handled
		 */
		inline void assignPath(int pathIdx, int64_t now)
		{
			assignedPath = pathIdx;
			lastPathReassignment = now;
		}

		AtomicCounter __refCount;

		int32_t id;						// Flow ID used for hashing and path selection
		uint64_t bytesIn;				// Used for tracking flow size
		uint64_t bytesOut;				// Used for tracking flow size
		int64_t lastActivity;			// The last time that this flow handled traffic
		int64_t lastPathReassignment;	// Time of last path assignment. Used for anti-flapping
		int assignedPath;				// Index of path to which this flow is assigned
	};

	const RuntimeEnvironment* RR;
	AtomicCounter __refCount;

	std::string _policyAlias;	// Custom name given by the user to this bond type.

	static Binder* _binder;

	/**
	 * Set of indices corresponding to paths currently included in the bond proper. This
	 * may only be updated during a call to curateBond(). The reason for this is so that
	 * we can simplify the high frequency packet egress logic.
	 */
	int _realIdxMap[ZT_MAX_PEER_NETWORK_PATHS] = { ZT_MAX_PEER_NETWORK_PATHS };
	int _numBondedPaths;						  // Number of paths currently included in the _realIdxMap set.
	std::map<int16_t, SharedPtr<Flow> > _flows;	  // Flows hashed according to port and protocol
	float _qw[ZT_QOS_PARAMETER_SIZE];			  // Link quality specification (can be customized by user)

	bool _run;

	uint8_t _policy;
	uint32_t _upDelay;
	uint32_t _downDelay;

	// active-backup
	int _abPathIdx;	  // current active path
	std::deque<int> _abFailoverQueue;
	uint8_t _abLinkSelectMethod;   // link re-selection policy for the primary link in active-backup

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

	// dynamic link monitoring
	uint8_t _linkMonitorStrategy;

	// path negotiation
	int16_t _localUtility;
	int _negotiatedPathIdx;
	uint8_t _numSentPathNegotiationRequests;
	bool _allowPathNegotiation;

	/**
	 * Timers and intervals
	 */
	uint64_t _failoverInterval;
	uint64_t _qosSendInterval;
	uint64_t _ackSendInterval;
	uint64_t throughputMeasurementInterval;
	uint64_t _qualityEstimationInterval;

	/**
	 * Link state reporting
	 */
	uint8_t _numAliveLinks;
	uint8_t _numTotalLinks;

	/**
	 * Default initial punishment inflicted on misbehaving paths. Punishment slowly
	 * drains linearly. For each eligibility change the remaining punishment is doubled.
	 */
	uint32_t _defaultPathRefractoryPeriod;
	unsigned char _freeRandomByte;	 // Free byte of entropy that is updated on every packet egress event.
	SharedPtr<Peer> _peer;			 // Remote peer that this bond services
	unsigned long long _peerId;		 // ID of the peer that this bond services
	bool _isLeaf;

	/**
	 * Rate-limiting
	 */
	uint16_t _qosCutoffCount;
	uint16_t _ackCutoffCount;
	uint64_t _lastQoSRateCheck;
	uint64_t _lastAckRateCheck;
	uint16_t _pathNegotiationCutoffCount;
	uint64_t _lastPathNegotiationReceived;

	/**
	 * Recent event timestamps
	 */
	uint64_t _lastSummaryDump;

	uint64_t _lastQualityEstimation;
	uint64_t _lastBackgroundTaskCheck;
	uint64_t _lastBondStatusLog;
	uint64_t _lastPathNegotiationCheck;
	uint64_t _lastSentPathNegotiationRequest;
	uint64_t _lastFlowExpirationCheck;
	uint64_t _lastFlowRebalance;
	uint64_t _lastFrame;
	uint64_t _lastActiveBackupPathChange;

	Mutex _paths_m;

	Mutex _flows_m;

	bool _userHasSpecifiedLinks;				  // Whether the user has specified links for this bond.
	bool _userHasSpecifiedPrimaryLink;			  // Whether the user has specified a primary link for this bond.
	bool _userHasSpecifiedFailoverInstructions;	  // Whether the user has specified failover instructions for this bond.
	bool _userHasSpecifiedLinkCapacities;		  // Whether the user has specified links capacities for this bond.
	/**
	 * How frequently (in ms) a VERB_ECHO is sent to a peer to verify that a
	 * path is still active. A value of zero (0) will disable active path
	 * monitoring; as result, all monitoring will be a function of traffic.
	 */
	int _monitorInterval;
	bool _allowFlowHashing;	  // Whether or not flow hashing is allowed.

	uint64_t _overheadBytes;
};

}	// namespace ZeroTier

#endif
