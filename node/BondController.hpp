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

#ifndef ZT_BONDCONTROLLER_HPP
#define ZT_BONDCONTROLLER_HPP

#include <map>
#include <vector>

#include "SharedPtr.hpp"
#include "../osdep/Phy.hpp"
#include "../osdep/Link.hpp"

namespace ZeroTier {

class RuntimeEnvironment;
class Bond;
class Peer;

class BondController
{
	friend class Bond;

public:

	BondController(const RuntimeEnvironment *renv);

	/**
	 * @return Whether this link is permitted to become a member of a bond.
	 */
	bool linkAllowed(std::string &policyAlias, SharedPtr<Link> link);

	/**
	 * @return The minimum interval required to poll the active bonds to fulfill all active monitoring timing requirements.
	 */
	int minReqPathMonitorInterval() { return _minReqPathMonitorInterval; }

	/**
	 * @param minReqPathMonitorInterval The minimum interval required to poll the active bonds to fulfill all active monitoring timing requirements.
	 */
	static void setMinReqPathMonitorInterval(int minReqPathMonitorInterval) { _minReqPathMonitorInterval = minReqPathMonitorInterval; }

	/**
	 * @return Whether the bonding layer is currently set up to be used.
	 */
	bool inUse() { return !_bondPolicyTemplates.empty() || _defaultBondingPolicy; }

	/**
	 * @param basePolicyName Bonding policy name (See ZeroTierOne.h)
	 * @return The bonding policy code for a given human-readable bonding policy name
	 */
	static int getPolicyCodeByStr(const std::string& basePolicyName)
	{
		if (basePolicyName == "active-backup") { return 1; }
		if (basePolicyName == "broadcast") { return 2; }
		if (basePolicyName == "balance-rr") { return 3; }
		if (basePolicyName == "balance-xor") { return 4; }
		if (basePolicyName == "balance-aware") { return 5; }
		return 0; // "none"
	}

	/**
	 * @param policy Bonding policy code (See ZeroTierOne.h)
	 * @return The human-readable name for the given bonding policy code
	 */
	static std::string getPolicyStrByCode(int policy)
	{
		if (policy == 1) { return "active-backup"; }
		if (policy == 2) { return "broadcast"; }
		if (policy == 3) { return "balance-rr"; }
		if (policy == 4) { return "balance-xor"; }
		if (policy == 5) { return "balance-aware"; }
		return "none";
	}

	/**
	 * Sets the default bonding policy for new or undefined bonds.
	 *
	 * @param bp Bonding policy
	 */
	void setBondingLayerDefaultPolicy(uint8_t bp) { _defaultBondingPolicy = bp; }

	/**
	 * Sets the default (custom) bonding policy for new or undefined bonds.
	 *
	 * @param alias Human-readable string alias for bonding policy
	 */
	void setBondingLayerDefaultPolicyStr(std::string alias) { _defaultBondingPolicyStr = alias; }

	/**
	 * @return The default bonding policy
	 */
	static int defaultBondingPolicy() { return _defaultBondingPolicy; }

	/**
	 * Add a user-defined link to a given bonding policy.
	 *
	 * @param policyAlias User-defined custom name for variant of bonding policy
	 * @param link Pointer to new link definition
	 */
	void addCustomLink(std::string& policyAlias, SharedPtr<Link> link);

	/**
	 * Add a user-defined bonding policy that is based on one of the standard types.
	 *
	 * @param newBond Pointer to custom Bond object
	 * @return Whether a uniquely-named custom policy was successfully added
	 */
	bool addCustomPolicy(const SharedPtr<Bond>& newBond);

	/**
	 * Assigns a specific bonding policy
	 *
	 * @param identity
	 * @param policyAlias
	 * @return
	 */
	bool assignBondingPolicyToPeer(int64_t identity, const std::string& policyAlias);

	/**
	 * Add a new bond to the bond controller.
	 *
	 * @param renv Runtime environment
	 * @param peer Remote peer that this bond services
	 * @return A pointer to the newly created Bond
	 */
	SharedPtr<Bond> createTransportTriggeredBond(const RuntimeEnvironment *renv, const SharedPtr<Peer>& peer);

	/**
	 * Periodically perform maintenance tasks for the bonding layer.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 */
	void processBackgroundTasks(void *tPtr, int64_t now);

	/**
	 * Gets a reference to a physical link definition given a policy alias and a local socket.
	 *
	 * @param policyAlias Policy in use
	 * @param localSocket Local source socket
	 * @return Physical link definition
	 */
	SharedPtr<Link> getLinkBySocket(const std::string& policyAlias, uint64_t localSocket);

	/**
	 * Gets a reference to a physical link definition given its human-readable system name.
	 *
	 * @param policyAlias Policy in use
	 * @param ifname Alphanumeric human-readable name
	 * @return Physical link definition
	 */
	SharedPtr<Link> getLinkByName(const std::string& policyAlias, const std::string& ifname);

	/**
	 * @param ifname Name of interface that we want to know if we can bind to
	 */
	bool allowedToBind(const std::string& ifname);

	uint64_t getBondStartTime() { return bondStartTime; }

private:

	Phy<BondController *> *_phy;
	const RuntimeEnvironment *RR;

	Mutex _bonds_m;
	Mutex _links_m;

	/**
	 * The last time that the bond controller updated the set of bonds.
	 */
	uint64_t _lastBackgroundBondControlTaskCheck;

	/**
	 * The minimum monitoring interval among all paths in this bond.
	 */
	static int _minReqPathMonitorInterval;

	/**
	 * The default bonding policy used for new bonds unless otherwise specified.
	 */
	static uint8_t _defaultBondingPolicy;

	/**
	 * The default bonding policy used for new bonds unless otherwise specified.
	 */
	std::string _defaultBondingPolicyStr;

	/**
	 * All currently active bonds.
	 */
	std::map<int64_t,SharedPtr<Bond> > _bonds;

	/**
	 * Map of peers to custom bonding policies
	 */
	std::map<int64_t,std::string> _policyTemplateAssignments;

	/**
	 * User-defined bonding policies (can be assigned to a peer)
	 */
	std::map<std::string,SharedPtr<Bond> > _bondPolicyTemplates;

	/**
	 * Set of links defined for a given bonding policy
	 */
	std::map<std::string,std::vector<SharedPtr<Link> > > _linkDefinitions;

	/**
	 * Set of link objects mapped to their physical interfaces
	 */
	std::map<std::string, std::map<std::string, SharedPtr<Link> > > _interfaceToLinkMap;

	// TODO: Remove
	uint64_t bondStartTime;
};

} // namespace ZeroTier

#endif