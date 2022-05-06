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

#include "Bond.hpp"

#include "Switch.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace ZeroTier {

static unsigned char s_freeRandomByteCounter = 0;

int Bond::_minReqMonitorInterval = ZT_BOND_FAILOVER_DEFAULT_INTERVAL;
uint8_t Bond::_defaultPolicy = ZT_BOND_POLICY_NONE;

Phy<Bond*>* Bond::_phy;

Mutex Bond::_bonds_m;
Mutex Bond::_links_m;

std::string Bond::_defaultPolicyStr;
std::map<int64_t, SharedPtr<Bond> > Bond::_bonds;
std::map<int64_t, std::string> Bond::_policyTemplateAssignments;
std::map<std::string, SharedPtr<Bond> > Bond::_bondPolicyTemplates;
std::map<std::string, std::vector<SharedPtr<Link> > > Bond::_linkDefinitions;
std::map<std::string, std::map<std::string, SharedPtr<Link> > > Bond::_interfaceToLinkMap;

bool Bond::linkAllowed(std::string& policyAlias, SharedPtr<Link> link)
{
	bool foundInDefinitions = false;
	if (_linkDefinitions.count(policyAlias)) {
		auto it = _linkDefinitions[policyAlias].begin();
		while (it != _linkDefinitions[policyAlias].end()) {
			if (link->ifname() == (*it)->ifname()) {
				foundInDefinitions = true;
				break;
			}
			++it;
		}
	}
	return _linkDefinitions[policyAlias].empty() || foundInDefinitions;
}

void Bond::addCustomLink(std::string& policyAlias, SharedPtr<Link> link)
{
	Mutex::Lock _l(_links_m);
	_linkDefinitions[policyAlias].push_back(link);
	auto search = _interfaceToLinkMap[policyAlias].find(link->ifname());
	if (search == _interfaceToLinkMap[policyAlias].end()) {
		link->setAsUserSpecified(true);
		_interfaceToLinkMap[policyAlias].insert(std::pair<std::string, SharedPtr<Link> >(link->ifname(), link));
	}
}

bool Bond::addCustomPolicy(const SharedPtr<Bond>& newBond)
{
	Mutex::Lock _l(_bonds_m);
	if (! _bondPolicyTemplates.count(newBond->policyAlias())) {
		_bondPolicyTemplates[newBond->policyAlias()] = newBond;
		return true;
	}
	return false;
}

bool Bond::assignBondingPolicyToPeer(int64_t identity, const std::string& policyAlias)
{
	Mutex::Lock _l(_bonds_m);
	if (! _policyTemplateAssignments.count(identity)) {
		_policyTemplateAssignments[identity] = policyAlias;
		return true;
	}
	return false;
}

SharedPtr<Bond> Bond::getBondByPeerId(int64_t identity)
{
	Mutex::Lock _l(_bonds_m);
	return _bonds.count(identity) ? _bonds[identity] : SharedPtr<Bond>();
}

SharedPtr<Bond> Bond::createBond(const RuntimeEnvironment* renv, const SharedPtr<Peer>& peer)
{
	Mutex::Lock _l(_bonds_m);
	int64_t identity = peer->identity().address().toInt();
	Bond* bond = nullptr;
	if (! _bonds.count(identity)) {
		if (! _policyTemplateAssignments.count(identity)) {
			if (_defaultPolicy) {
				bond = new Bond(renv, _defaultPolicy, peer);
				bond->debug("new default bond");
			}
			if (! _defaultPolicy && _defaultPolicyStr.length()) {
				bond = new Bond(renv, _bondPolicyTemplates[_defaultPolicyStr].ptr(), peer);
				bond->debug("new default custom bond (based on %s)", bond->getPolicyStrByCode(bond->policy()).c_str());
			}
		}
		else {
			if (! _bondPolicyTemplates[_policyTemplateAssignments[identity]]) {
				bond = new Bond(renv, _defaultPolicy, peer);
				bond->debug("peer-specific bond, was specified as %s but the bond definition was not found, using default %s", _policyTemplateAssignments[identity].c_str(), getPolicyStrByCode(_defaultPolicy).c_str());
			}
			else {
				bond = new Bond(renv, _bondPolicyTemplates[_policyTemplateAssignments[identity]].ptr(), peer);
				bond->debug("new default bond");
			}
		}
	}
	if (bond) {
		_bonds[identity] = bond;
		/**
		 * Determine if user has specified anything that could affect the bonding policy's decisions
		 */
		if (_interfaceToLinkMap.count(bond->policyAlias())) {
			std::map<std::string, SharedPtr<Link> >::iterator it = _interfaceToLinkMap[bond->policyAlias()].begin();
			while (it != _interfaceToLinkMap[bond->policyAlias()].end()) {
				if (it->second->isUserSpecified()) {
					bond->_userHasSpecifiedLinks = true;
				}
				if (it->second->isUserSpecified() && it->second->primary()) {
					bond->_userHasSpecifiedPrimaryLink = true;
				}
				if (it->second->isUserSpecified() && it->second->userHasSpecifiedFailoverInstructions()) {
					bond->_userHasSpecifiedFailoverInstructions = true;
				}
				if (it->second->isUserSpecified() && (it->second->speed() > 0)) {
					bond->_userHasSpecifiedLinkSpeeds = true;
				}
				++it;
			}
		}
		return bond;
	}
	return SharedPtr<Bond>();
}

void Bond::destroyBond(uint64_t peerId)
{
	Mutex::Lock _l(_bonds_m);
	_bonds.erase(peerId);
}

SharedPtr<Link> Bond::getLinkBySocket(const std::string& policyAlias, uint64_t localSocket)
{
	Mutex::Lock _l(_links_m);
	char ifname[64] = { 0 };
	_phy->getIfName((PhySocket*)((uintptr_t)localSocket), ifname, sizeof(ifname) - 1);
	std::string ifnameStr(ifname);
	auto search = _interfaceToLinkMap[policyAlias].find(ifnameStr);
	if (search == _interfaceToLinkMap[policyAlias].end()) {
		// If the link wasn't already known, add a new entry
		SharedPtr<Link> s = new Link(ifnameStr, 0, 0, true, ZT_BOND_SLAVE_MODE_SPARE, "", 0.0);
		_interfaceToLinkMap[policyAlias].insert(std::pair<std::string, SharedPtr<Link> >(ifnameStr, s));
		return s;
	}
	else {
		return search->second;
	}
}

SharedPtr<Link> Bond::getLinkByName(const std::string& policyAlias, const std::string& ifname)
{
	Mutex::Lock _l(_links_m);
	auto search = _interfaceToLinkMap[policyAlias].find(ifname);
	if (search != _interfaceToLinkMap[policyAlias].end()) {
		return search->second;
	}
	return SharedPtr<Link>();
}

void Bond::processBackgroundTasks(void* tPtr, const int64_t now)
{
	unsigned long _currMinReqMonitorInterval = ZT_BOND_FAILOVER_DEFAULT_INTERVAL;
	Mutex::Lock _l(_bonds_m);
	std::map<int64_t, SharedPtr<Bond> >::iterator bondItr = _bonds.begin();
	while (bondItr != _bonds.end()) {
		// Update Bond Controller's background processing timer
		_currMinReqMonitorInterval = std::min(_currMinReqMonitorInterval, (unsigned long)(bondItr->second->monitorInterval()));
		bondItr->second->processBackgroundBondTasks(tPtr, now);
		++bondItr;
	}
	_minReqMonitorInterval = std::min(_currMinReqMonitorInterval, (unsigned long)ZT_BOND_FAILOVER_DEFAULT_INTERVAL);
}

Bond::Bond(const RuntimeEnvironment* renv) : RR(renv)
{
	initTimers();
}

Bond::Bond(const RuntimeEnvironment* renv, int policy, const SharedPtr<Peer>& peer) : RR(renv), _freeRandomByte((unsigned char)((uintptr_t)this >> 4) ^ ++s_freeRandomByteCounter), _peer(peer), _peerId(_peer->_id.address().toInt())
{
	initTimers();
	setBondParameters(policy, SharedPtr<Bond>(), false);
	_policyAlias = getPolicyStrByCode(policy);
}

Bond::Bond(const RuntimeEnvironment* renv, std::string& basePolicy, std::string& policyAlias, const SharedPtr<Peer>& peer) : RR(renv), _policyAlias(policyAlias), _peer(peer)
{
	initTimers();
	setBondParameters(getPolicyCodeByStr(basePolicy), SharedPtr<Bond>(), false);
}

Bond::Bond(const RuntimeEnvironment* renv, SharedPtr<Bond> originalBond, const SharedPtr<Peer>& peer)
	: RR(renv)
	, _freeRandomByte((unsigned char)((uintptr_t)this >> 4) ^ ++s_freeRandomByteCounter)
	, _peer(peer)
	, _peerId(_peer->_id.address().toInt())
{
	initTimers();
	setBondParameters(originalBond->_policy, originalBond, true);
}

void Bond::nominatePathToBond(const SharedPtr<Path>& path, int64_t now)
{
	Mutex::Lock _l(_paths_m);
	/**
	 * Ensure the link is allowed and the path is not already present
	 */
	if (! RR->bc->linkAllowed(_policyAlias, getLink(path))) {
		return;
	}
	bool alreadyPresent = false;
	for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		// Sanity check
		if (path.ptr() == _paths[i].p.ptr()) {
			alreadyPresent = true;
			break;
		}
	}
	if (! alreadyPresent) {
		/**
		 * Find somewhere to stick it
		 */
		for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (! _paths[i].p) {
				_paths[i].set(now, path);
				/**
				 * Set user preferences and update state variables of other paths on the same link
				 */
				SharedPtr<Link> sl = getLink(_paths[i].p);
				if (sl) {
					// Determine if there are any other paths on this link
					bool bFoundCommonLink = false;
					SharedPtr<Link> commonLink = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());
					for (unsigned int j = 0; j < ZT_MAX_PEER_NETWORK_PATHS; ++j) {
						if (_paths[j].p && _paths[j].p.ptr() != _paths[i].p.ptr()) {
							if (RR->bc->getLinkBySocket(_policyAlias, _paths[j].p->localSocket()) == commonLink) {
								bFoundCommonLink = true;
								_paths[j].onlyPathOnLink = false;
							}
						}
					}
					_paths[i].ipvPref = sl->ipvPref();
					_paths[i].mode = sl->mode();
					_paths[i].enabled = sl->enabled();
					_paths[i].onlyPathOnLink = ! bFoundCommonLink;
				}
				log("nominate link %s", pathToStr(path).c_str());
				break;
			}
		}
	}
	curateBond(now, true);
	estimatePathQuality(now);
}

void Bond::addPathToBond(int nominatedIdx, int bondedIdx)
{
	// Map bonded set to nominated set
	_bondIdxMap[bondedIdx] = nominatedIdx;
	// Tell the bonding layer that we can now use this bond for traffic
	_paths[nominatedIdx].bonded = true;
}

SharedPtr<Path> Bond::getAppropriatePath(int64_t now, int32_t flowId)
{
	Mutex::Lock _l(_paths_m);
	/**
	 * active-backup
	 */
	if (_policy == ZT_BOND_POLICY_ACTIVE_BACKUP) {
		if (_abPathIdx != ZT_MAX_PEER_NETWORK_PATHS && _paths[_abPathIdx].p) {
			return _paths[_abPathIdx].p;
		}
	}
	/**
	 * broadcast
	 */
	if (_policy == ZT_BOND_POLICY_BROADCAST) {
		return SharedPtr<Path>();	// Handled in Switch::_trySend()
	}
	if (! _numBondedPaths) {
		return SharedPtr<Path>();	// No paths assigned to bond yet, cannot balance traffic
	}
	/**
	 * balance-rr
	 */
	if (_policy == ZT_BOND_POLICY_BALANCE_RR) {
		if (! _allowFlowHashing) {
			if (_packetsPerLink == 0) {
				// Randomly select a path
				return _paths[_bondIdxMap[_freeRandomByte % _numBondedPaths]].p;
			}
			if (_rrPacketsSentOnCurrLink < _packetsPerLink) {
				// Continue to use this link
				++_rrPacketsSentOnCurrLink;
				return _paths[_bondIdxMap[_rrIdx]].p;
			}
			// Reset striping counter
			_rrPacketsSentOnCurrLink = 0;
			if (_numBondedPaths == 1 || _rrIdx >= (ZT_MAX_PEER_NETWORK_PATHS - 1)) {
				_rrIdx = 0;
			}
			else {
				int _tempIdx = _rrIdx;
				for (int searchCount = 0; searchCount < (_numBondedPaths - 1); searchCount++) {
					_tempIdx = (_tempIdx == (_numBondedPaths - 1)) ? 0 : _tempIdx + 1;
					if (_bondIdxMap[_tempIdx] != ZT_MAX_PEER_NETWORK_PATHS) {
						if (_paths[_bondIdxMap[_tempIdx]].p && _paths[_bondIdxMap[_tempIdx]].eligible) {
							_rrIdx = _tempIdx;
							break;
						}
					}
				}
			}
			if (_paths[_bondIdxMap[_rrIdx]].p) {
				return _paths[_bondIdxMap[_rrIdx]].p;
			}
		}
	}
	/**
	 * balance-xor
	 */
	if (_policy == ZT_BOND_POLICY_BALANCE_XOR || _policy == ZT_BOND_POLICY_BALANCE_AWARE) {
		if (! _allowFlowHashing || flowId == -1) {
			// No specific path required for unclassified traffic, send on anything
			int m_idx = _bondIdxMap[_freeRandomByte % _numBondedPaths];
			return _paths[m_idx].p;
		}
		else if (_allowFlowHashing) {
			Mutex::Lock _l(_flows_m);
			SharedPtr<Flow> flow;
			if (_flows.count(flowId)) {
				flow = _flows[flowId];
				flow->lastActivity = now;
			}
			else {
				unsigned char entropy;
				Utils::getSecureRandom(&entropy, 1);
				flow = createFlow(ZT_MAX_PEER_NETWORK_PATHS, flowId, entropy, now);
			}
			if (flow) {
				return _paths[flow->assignedPath].p;
			}
		}
	}
	return SharedPtr<Path>();
}

void Bond::recordIncomingInvalidPacket(const SharedPtr<Path>& path)
{
	Mutex::Lock _l(_paths_m);
	for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i].p == path) {
			//_paths[i].packetValiditySamples.push(false);
		}
	}
}

void Bond::recordOutgoingPacket(const SharedPtr<Path>& path, uint64_t packetId, uint16_t payloadLength, const Packet::Verb verb, const int32_t flowId, int64_t now)
{
	_freeRandomByte += (unsigned char)(packetId >> 8);	 // Grab entropy to use in path selection logic
	bool isFrame = (verb == Packet::Packet::VERB_ECHO || verb == Packet::VERB_FRAME || verb == Packet::VERB_EXT_FRAME);
	bool shouldRecord = (packetId & (ZT_QOS_ACK_DIVISOR - 1) && (verb != Packet::VERB_ACK) && (verb != Packet::VERB_QOS_MEASUREMENT));
	if (isFrame || shouldRecord) {
		Mutex::Lock _l(_paths_m);
		int pathIdx = getNominatedPathIdx(path);
		if (pathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
			return;
		}
		if (isFrame) {
			++(_paths[pathIdx].packetsOut);
			_lastFrame = now;
		}
		if (shouldRecord) {
			//_paths[pathIdx].unackedBytes += payloadLength;
			// Take note that we're expecting a VERB_ACK on this path as of a specific time
			if (_paths[pathIdx].qosStatsOut.size() < ZT_QOS_MAX_PENDING_RECORDS) {
				_paths[pathIdx].qosStatsOut[packetId] = now;
			}
		}
	}
	if (_allowFlowHashing && (flowId != ZT_QOS_NO_FLOW)) {
		Mutex::Lock _l(_flows_m);
		if (_flows.count(flowId)) {
			_flows[flowId]->bytesOut += payloadLength;
		}
	}
}

void Bond::recordIncomingPacket(const SharedPtr<Path>& path, uint64_t packetId, uint16_t payloadLength, Packet::Verb verb, int32_t flowId, int64_t now)
{
	bool isFrame = (verb == Packet::Packet::VERB_ECHO || verb == Packet::VERB_FRAME || verb == Packet::VERB_EXT_FRAME);
	bool shouldRecord = (packetId & (ZT_QOS_ACK_DIVISOR - 1) && (verb != Packet::VERB_ACK) && (verb != Packet::VERB_QOS_MEASUREMENT));
	Mutex::Lock _l(_paths_m);
	int pathIdx = getNominatedPathIdx(path);
	if (pathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
		return;
	}
	// Take note of the time that this previously-dead path received a packet
	if (! _paths[pathIdx].alive) {
		_paths[pathIdx].lastAliveToggle = now;
	}
	if (isFrame || shouldRecord) {
		if (_paths[pathIdx].allowed()) {
			if (isFrame) {
				++(_paths[pathIdx].packetsIn);
				_lastFrame = now;
			}
			if (shouldRecord) {
				if (_paths[pathIdx].qosStatsIn.size() < ZT_QOS_MAX_PENDING_RECORDS) {
					_paths[pathIdx].qosStatsIn[packetId] = now;
					++(_paths[pathIdx].packetsReceivedSinceLastQoS);
					//_paths[pathIdx].packetValiditySamples.push(true);
				}
			}
		}
	}

	/**
	 * Learn new flows and pro-actively create entries for them in the bond so
	 * that the next time we send a packet out that is part of a flow we know
	 * which path to use.
	 */
	if ((flowId != ZT_QOS_NO_FLOW) && (_policy == ZT_BOND_POLICY_BALANCE_RR || _policy == ZT_BOND_POLICY_BALANCE_XOR || _policy == ZT_BOND_POLICY_BALANCE_AWARE)) {
		Mutex::Lock _l(_flows_m);
		SharedPtr<Flow> flow;
		if (! _flows.count(flowId)) {
			flow = createFlow(pathIdx, flowId, 0, now);
		}
		else {
			flow = _flows[flowId];
		}
		if (flow) {
			flow->bytesIn += payloadLength;
		}
	}
}

void Bond::receivedQoS(const SharedPtr<Path>& path, int64_t now, int count, uint64_t* rx_id, uint16_t* rx_ts)
{
	Mutex::Lock _l(_paths_m);
	int pathIdx = getNominatedPathIdx(path);
	if (pathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
		return;
	}
	// debug("received QoS packet (sampling %d frames) via %s", count, pathToStr(path).c_str());
	// Look up egress times and compute latency values for each record
	std::map<uint64_t, uint64_t>::iterator it;
	for (int j = 0; j < count; j++) {
		it = _paths[pathIdx].qosStatsOut.find(rx_id[j]);
		if (it != _paths[pathIdx].qosStatsOut.end()) {
			_paths[pathIdx].latencySamples.push(((uint16_t)(now - it->second) - rx_ts[j]) / 2);
			_paths[pathIdx].qosStatsOut.erase(it);
		}
	}
	_paths[pathIdx].qosRecordSize.push(count);
}

int32_t Bond::generateQoSPacket(int pathIdx, int64_t now, char* qosBuffer)
{
	int32_t len = 0;
	std::map<uint64_t, uint64_t>::iterator it = _paths[pathIdx].qosStatsIn.begin();
	int i = 0;
	int numRecords = std::min(_paths[pathIdx].packetsReceivedSinceLastQoS, ZT_QOS_TABLE_SIZE);
	while (i < numRecords && it != _paths[pathIdx].qosStatsIn.end()) {
		uint64_t id = it->first;
		memcpy(qosBuffer, &id, sizeof(uint64_t));
		qosBuffer += sizeof(uint64_t);
		uint16_t holdingTime = (uint16_t)(now - it->second);
		memcpy(qosBuffer, &holdingTime, sizeof(uint16_t));
		qosBuffer += sizeof(uint16_t);
		len += sizeof(uint64_t) + sizeof(uint16_t);
		_paths[pathIdx].qosStatsIn.erase(it++);
		++i;
	}
	return len;
}

bool Bond::assignFlowToBondedPath(SharedPtr<Flow>& flow, int64_t now)
{
	if (! _numBondedPaths) {
		debug("unable to assign flow %x (bond has no links)\n", flow->id);
		return false;
	}
	unsigned int idx = ZT_MAX_PEER_NETWORK_PATHS;
	if (_policy == ZT_BOND_POLICY_BALANCE_XOR) {
		idx = abs((int)(flow->id % (_numBondedPaths)));
		flow->assignPath(_bondIdxMap[idx], now);
		++(_paths[_bondIdxMap[idx]].assignedFlowCount);
	}
	if (_policy == ZT_BOND_POLICY_BALANCE_AWARE) {
		unsigned char entropy;
		Utils::getSecureRandom(&entropy, 1);
		if (_totalBondUnderload) {
			entropy %= _totalBondUnderload;
		}
		/* Since there may be scenarios where a path is removed before we can re-estimate
		relative qualities (and thus allocations) we need to down-modulate the entropy
		value that we use to randomly assign among the surviving paths, otherwise we risk
		not being able to find a path to assign this flow to. */
		int totalIncompleteAllocation = 0;
		for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (_paths[i].p && _paths[i].bonded) {
				totalIncompleteAllocation += _paths[i].allocation;
			}
		}
		entropy %= totalIncompleteAllocation;
		for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (_paths[i].p && _paths[i].bonded) {
				uint8_t probabilitySegment = (_totalBondUnderload > 0) ? _paths[i].affinity : _paths[i].allocation;
				if (entropy <= probabilitySegment) {
					idx = i;
					break;
				}
				entropy -= probabilitySegment;
			}
		}
		if (idx < ZT_MAX_PEER_NETWORK_PATHS) {
			flow->assignPath(idx, now);
			++(_paths[idx].assignedFlowCount);
		}
		else {
			debug("unable to assign out-flow %x (unknown reason)", flow->id);
			return false;
		}
	}
	if (_policy == ZT_BOND_POLICY_ACTIVE_BACKUP) {
		if (_abPathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
			debug("unable to assign out-flow %x (no active backup link)", flow->id);
		}
		flow->assignPath(_abPathIdx, now);
	}
	SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[flow->assignedPath].p->localSocket());
	debug("assign out-flow %04x to link %s (%lu / %lu flows)", flow->id, pathToStr(_paths[flow->assignedPath].p).c_str(), _paths[flow->assignedPath].assignedFlowCount, (unsigned long)_flows.size());
	return true;
}

SharedPtr<Bond::Flow> Bond::createFlow(int pathIdx, int32_t flowId, unsigned char entropy, int64_t now)
{
	if (! _numBondedPaths) {
		debug("unable to assign flow %x (bond has no links)\n", flowId);
		return SharedPtr<Flow>();
	}
	if (_flows.size() >= ZT_FLOW_MAX_COUNT) {
		debug("forget oldest flow (max flows reached: %d)\n", ZT_FLOW_MAX_COUNT);
		forgetFlowsWhenNecessary(0, true, now);
	}
	SharedPtr<Flow> flow = new Flow(flowId, now);
	_flows[flowId] = flow;
	/**
	 * Add a flow with a given Path already provided. This is the case when a packet
	 * is received on a path but no flow exists, in this case we simply assign the path
	 * that the remote peer chose for us.
	 */
	if (pathIdx != ZT_MAX_PEER_NETWORK_PATHS) {
		flow->assignPath(pathIdx, now);
		_paths[pathIdx].assignedFlowCount++;
		debug("assign in-flow %x to link %s (%lu / %lu)", flow->id, pathToStr(_paths[pathIdx].p).c_str(), _paths[pathIdx].assignedFlowCount, (unsigned long)_flows.size());
	}
	/**
	 * Add a flow when no path was provided. This means that it is an outgoing packet
	 * and that it is up to the local peer to decide how to load-balance its transmission.
	 */
	else {
		assignFlowToBondedPath(flow, now);
	}
	return flow;
}

void Bond::forgetFlowsWhenNecessary(uint64_t age, bool oldest, int64_t now)
{
	std::map<int32_t, SharedPtr<Flow> >::iterator it = _flows.begin();
	std::map<int32_t, SharedPtr<Flow> >::iterator oldestFlow = _flows.end();
	SharedPtr<Flow> expiredFlow;
	if (age) {	 // Remove by specific age
		while (it != _flows.end()) {
			if (it->second->age(now) > age) {
				debug("forget flow %x (age %llu) (%lu / %lu)", it->first, (unsigned long long)it->second->age(now), _paths[it->second->assignedPath].assignedFlowCount, (unsigned long)(_flows.size() - 1));
				_paths[it->second->assignedPath].assignedFlowCount--;
				it = _flows.erase(it);
			}
			else {
				++it;
			}
		}
	}
	else if (oldest) {	 // Remove single oldest by natural expiration
		uint64_t maxAge = 0;
		while (it != _flows.end()) {
			if (it->second->age(now) > maxAge) {
				maxAge = (now - it->second->age(now));
				oldestFlow = it;
			}
			++it;
		}
		if (oldestFlow != _flows.end()) {
			debug("forget oldest flow %x (age %llu) (total flows: %lu)", oldestFlow->first, (unsigned long long)oldestFlow->second->age(now), (unsigned long)(_flows.size() - 1));
			_paths[oldestFlow->second->assignedPath].assignedFlowCount--;
			_flows.erase(oldestFlow);
		}
	}
}

void Bond::processIncomingPathNegotiationRequest(uint64_t now, SharedPtr<Path>& path, int16_t remoteUtility)
{
	char pathStr[64] = { 0 };
	if (_abLinkSelectMethod != ZT_BOND_RESELECTION_POLICY_OPTIMIZE) {
		return;
	}
	Mutex::Lock _l(_paths_m);
	int pathIdx = getNominatedPathIdx(path);
	if (pathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
		return;
	}
	_paths[pathIdx].p->address().toString(pathStr);
	if (! _lastPathNegotiationCheck) {
		return;
	}
	SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[pathIdx].p->localSocket());
	if (remoteUtility > _localUtility) {
		_paths[pathIdx].p->address().toString(pathStr);
		debug("peer suggests alternate link %s/%s, remote utility (%d) greater than local utility (%d), switching to suggested link\n", link->ifname().c_str(), pathStr, remoteUtility, _localUtility);
		_negotiatedPathIdx = pathIdx;
	}
	if (remoteUtility < _localUtility) {
		debug("peer suggests alternate link %s/%s, remote utility (%d) less than local utility (%d), not switching\n", link->ifname().c_str(), pathStr, remoteUtility, _localUtility);
	}
	if (remoteUtility == _localUtility) {
		debug("peer suggests alternate link %s/%s, remote utility (%d) equal to local utility (%d)\n", link->ifname().c_str(), pathStr, remoteUtility, _localUtility);
		if (_peer->_id.address().toInt() > RR->node->identity().address().toInt()) {
			debug("agree with peer to use alternate link %s/%s\n", link->ifname().c_str(), pathStr);
			_negotiatedPathIdx = pathIdx;
		}
		else {
			debug("ignore petition from peer to use alternate link %s/%s\n", link->ifname().c_str(), pathStr);
		}
	}
}

void Bond::pathNegotiationCheck(void* tPtr, int64_t now)
{
	int maxInPathIdx = ZT_MAX_PEER_NETWORK_PATHS;
	int maxOutPathIdx = ZT_MAX_PEER_NETWORK_PATHS;
	uint64_t maxInCount = 0;
	uint64_t maxOutCount = 0;
	for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (! _paths[i].p) {
			continue;
		}
		if (_paths[i].packetsIn > maxInCount) {
			maxInCount = _paths[i].packetsIn;
			maxInPathIdx = i;
		}
		if (_paths[i].packetsOut > maxOutCount) {
			maxOutCount = _paths[i].packetsOut;
			maxOutPathIdx = i;
		}
		_paths[i].resetPacketCounts();
	}
	bool _peerLinksSynchronized = ((maxInPathIdx != ZT_MAX_PEER_NETWORK_PATHS) && (maxOutPathIdx != ZT_MAX_PEER_NETWORK_PATHS) && (maxInPathIdx != maxOutPathIdx)) ? false : true;
	/**
	 * Determine utility and attempt to petition remote peer to switch to our chosen path
	 */
	if (! _peerLinksSynchronized) {
		_localUtility = _paths[maxOutPathIdx].failoverScore - _paths[maxInPathIdx].failoverScore;
		if (_paths[maxOutPathIdx].negotiated) {
			_localUtility -= ZT_BOND_FAILOVER_HANDICAP_NEGOTIATED;
		}
		if ((now - _lastSentPathNegotiationRequest) > ZT_PATH_NEGOTIATION_CUTOFF_TIME) {
			// fprintf(stderr, "BT: (sync) it's been long enough, sending more requests.\n");
			_numSentPathNegotiationRequests = 0;
		}
		if (_numSentPathNegotiationRequests < ZT_PATH_NEGOTIATION_TRY_COUNT) {
			if (_localUtility >= 0) {
				// fprintf(stderr, "BT: (sync) paths appear to be out of sync (utility=%d)\n", _localUtility);
				sendPATH_NEGOTIATION_REQUEST(tPtr, _paths[maxOutPathIdx].p);
				++_numSentPathNegotiationRequests;
				_lastSentPathNegotiationRequest = now;
				// fprintf(stderr, "sending request to use %s on %s, ls=%llx, utility=%d\n", pathStr, link->ifname().c_str(), _paths[maxOutPathIdx].p->localSocket(), _localUtility);
			}
		}
		/**
		 * Give up negotiating and consider switching
		 */
		else if ((now - _lastSentPathNegotiationRequest) > (2 * ZT_BOND_OPTIMIZE_INTERVAL)) {
			if (_localUtility == 0) {
				// There's no loss to us, just switch without sending a another request
				// fprintf(stderr, "BT: (sync) giving up, switching to remote peer's path.\n");
				_negotiatedPathIdx = maxInPathIdx;
			}
		}
	}
}

void Bond::sendPATH_NEGOTIATION_REQUEST(void* tPtr, int pathIdx)
{
	debug("send link negotiation request to peer via link %s, local utility is %d", pathToStr(_paths[pathIdx].p).c_str(), _localUtility);
	if (_abLinkSelectMethod != ZT_BOND_RESELECTION_POLICY_OPTIMIZE) {
		return;
	}
	Packet outp(_peer->_id.address(), RR->identity.address(), Packet::VERB_PATH_NEGOTIATION_REQUEST);
	outp.append<int16_t>(_localUtility);
	if (_paths[pathIdx].p->address()) {
		outp.armor(_peer->key(), false, _peer->aesKeysIfSupported());
		RR->node->putPacket(tPtr, _paths[pathIdx].p->localSocket(), _paths[pathIdx].p->address(), outp.data(), outp.size());
		_overheadBytes += outp.size();
	}
}

void Bond::sendQOS_MEASUREMENT(void* tPtr, int pathIdx, int64_t localSocket, const InetAddress& atAddress, int64_t now)
{
	int64_t _now = RR->node->now();
	Packet outp(_peer->_id.address(), RR->identity.address(), Packet::VERB_QOS_MEASUREMENT);
	char qosData[ZT_QOS_MAX_PACKET_SIZE];
	int16_t len = generateQoSPacket(pathIdx, _now, qosData);
	if (len) {
		outp.append(qosData, len);
		if (atAddress) {
			outp.armor(_peer->key(), false, _peer->aesKeysIfSupported());
			RR->node->putPacket(tPtr, localSocket, atAddress, outp.data(), outp.size());
		}
		else {
			RR->sw->send(tPtr, outp, false);
		}
		_paths[pathIdx].packetsReceivedSinceLastQoS = 0;
		_paths[pathIdx].lastQoSMeasurement = now;
		_overheadBytes += outp.size();
	}
	// debug("send QOS via link %s (len=%d)", pathToStr(_paths[pathIdx].p).c_str(), len);
}

void Bond::processBackgroundBondTasks(void* tPtr, int64_t now)
{
	if (! _peer->_localMultipathSupported || (now - _lastBackgroundTaskCheck) < ZT_BOND_BACKGROUND_TASK_MIN_INTERVAL) {
		return;
	}
	_lastBackgroundTaskCheck = now;
	Mutex::Lock _l(_paths_m);

	curateBond(now, false);
	if ((now - _lastQualityEstimation) > _qualityEstimationInterval) {
		_lastQualityEstimation = now;
		estimatePathQuality(now);
	}
	dumpInfo(now, false);

	// Send ambient monitoring traffic
	for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i].p && _paths[i].allowed()) {
			if (_isLeaf) {
				if ((_monitorInterval > 0) && (((now - _paths[i].p->_lastIn) >= (_paths[i].alive ? _monitorInterval : _failoverInterval)))) {
					if ((_peer->remoteVersionProtocol() >= 5) && (! ((_peer->remoteVersionMajor() == 1) && (_peer->remoteVersionMinor() == 1) && (_peer->remoteVersionRevision() == 0)))) {
						Packet outp(_peer->address(), RR->identity.address(), Packet::VERB_ECHO);	// ECHO (this is our bond's heartbeat)
						outp.armor(_peer->key(), true, _peer->aesKeysIfSupported());
						RR->node->expectReplyTo(outp.packetId());
						RR->node->putPacket(tPtr, _paths[i].p->localSocket(), _paths[i].p->address(), outp.data(), outp.size());
						_paths[i].p->_lastOut = now;
						_overheadBytes += outp.size();
						debug("tx: verb 0x%-2x of len %4d via %s (ECHO)", Packet::VERB_ECHO, outp.size(), pathToStr(_paths[i].p).c_str());
					}
				}
				// QOS
				if (_paths[i].needsToSendQoS(now, _qosSendInterval)) {
					sendQOS_MEASUREMENT(tPtr, i, _paths[i].p->localSocket(), _paths[i].p->address(), now);
				}
			}
		}
	}
	// Perform periodic background tasks unique to each bonding policy
	switch (_policy) {
		case ZT_BOND_POLICY_ACTIVE_BACKUP:
			processActiveBackupTasks(tPtr, now);
			break;
		case ZT_BOND_POLICY_BROADCAST:
			break;
		case ZT_BOND_POLICY_BALANCE_RR:
		case ZT_BOND_POLICY_BALANCE_XOR:
		case ZT_BOND_POLICY_BALANCE_AWARE:
			processBalanceTasks(now);
			break;
		default:
			break;
	}
	// Check whether or not a path negotiation needs to be performed
	if (((now - _lastPathNegotiationCheck) > ZT_BOND_OPTIMIZE_INTERVAL) && _allowPathNegotiation) {
		_lastPathNegotiationCheck = now;
		pathNegotiationCheck(tPtr, now);
	}
}

void Bond::curateBond(int64_t now, bool rebuildBond)
{
	uint8_t tmpNumAliveLinks = 0;
	uint8_t tmpNumTotalLinks = 0;

	/**
	 * Update path state variables. State variables are used so that critical
	 * blocks that perform fast packet processing won't need to make as many
	 * function calls or computations.
	 */
	for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (! _paths[i].p) {
			continue;
		}

		/**
		 * Remove expired links from bond
		 */
		if ((now - _paths[i].p->_lastIn) > (ZT_PEER_EXPIRED_PATH_TRIAL_PERIOD)) {
			log("link %s has expired, removing from bond", pathToStr(_paths[i].p).c_str());
			_paths[i] = NominatedPath();
			_paths[i].p = SharedPtr<Path>();
			continue;
		}

		tmpNumTotalLinks++;
		if (_paths[i].eligible) {
			tmpNumAliveLinks++;
		}

		/**
		 * Determine aliveness
		 */
		_paths[i].alive = _isLeaf ? (now - _paths[i].p->_lastIn) < _failoverInterval : (now - _paths[i].p->_lastIn) < ZT_PEER_PATH_EXPIRATION;

		/**
		 * Determine current eligibility
		 */
		bool currEligibility = false;
		// Simple RX age (driven by packets of any type and gratuitous VERB_HELLOs)
		bool acceptableAge = _isLeaf ? (_paths[i].p->age(now) < (_failoverInterval + _downDelay)) : _paths[i].alive;
		// Whether we've waited long enough since the link last came online
		bool satisfiedUpDelay = (now - _paths[i].lastAliveToggle) >= _upDelay;
		// Whether this path is still in its trial period
		bool inTrial = (now - _paths[i].whenNominated) < ZT_BOND_OPTIMIZE_INTERVAL;
		// if (includeRefractoryPeriod && _paths[i].refractoryPeriod) {
		// As long as the refractory period value has not fully drained this path is not eligible
		//	currEligibility = false;
		//}
		currEligibility = _paths[i].allowed() && ((acceptableAge && satisfiedUpDelay) || inTrial);
		// debug("[%d] allowed=%d, acceptableAge=%d, satisfiedUpDelay=%d, inTrial=%d ==== %d", i, _paths[i].allowed(), acceptableAge, satisfiedUpDelay, inTrial, currEligibility);

		/**
		 * Note eligibility state change (if any) and take appropriate action
		 */
		if (currEligibility != _paths[i].eligible) {
			if (currEligibility == 0) {
				log("link %s is no longer eligible", pathToStr(_paths[i].p).c_str());
			}
			if (currEligibility == 1) {
				log("link %s is eligible", pathToStr(_paths[i].p).c_str());
			}
			dumpPathStatus(now, i);
			if (currEligibility) {
				rebuildBond = true;
			}
			if (! currEligibility) {
				_paths[i].adjustRefractoryPeriod(now, _defaultPathRefractoryPeriod, ! currEligibility);
				if (_paths[i].bonded) {
					if (_allowFlowHashing) {
						debug("link %s was bonded, flow reallocation will occur soon", pathToStr(_paths[i].p).c_str());
						rebuildBond = true;
						_paths[i].shouldReallocateFlows = _paths[i].bonded;
					}
					_paths[i].bonded = false;
				}
			}
		}
		if (currEligibility) {
			_paths[i].adjustRefractoryPeriod(now, _defaultPathRefractoryPeriod, false);
		}
		_paths[i].eligible = currEligibility;
	}

	/**
	 * Trigger status report if number of links change
	 */
	_numAliveLinks = tmpNumAliveLinks;
	_numTotalLinks = tmpNumTotalLinks;
	if ((_numAliveLinks != tmpNumAliveLinks) || (_numTotalLinks != tmpNumTotalLinks)) {
		dumpInfo(now, true);
	}

	/**
	 * Curate the set of paths that are part of the bond proper. Select a set of paths
	 * per logical link according to eligibility and user-specified constraints.
	 */
	if ((_policy == ZT_BOND_POLICY_BALANCE_RR) || (_policy == ZT_BOND_POLICY_BALANCE_XOR) || (_policy == ZT_BOND_POLICY_BALANCE_AWARE)) {
		if (! _numBondedPaths) {
			rebuildBond = true;
		}
		if (rebuildBond) {
			debug("rebuilding bond");
			int updatedBondedPathCount = 0;
			// Build map associating paths with local physical links. Will be selected from in next step
			std::map<SharedPtr<Link>, std::vector<int> > linkMap;
			for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (_paths[i].p) {
					SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());
					linkMap[link].push_back(i);
				}
			}
			// Re-form bond from link<->path map
			std::map<SharedPtr<Link>, std::vector<int> >::iterator it = linkMap.begin();
			while (it != linkMap.end()) {
				SharedPtr<Link> link = it->first;
				int ipvPref = link->ipvPref();

				// If user has no address type preference, then use every path we find on a link
				if (ipvPref == 0) {
					for (int j = 0; j < it->second.size(); j++) {
						int idx = it->second.at(j);
						if (! _paths[idx].p || ! _paths[idx].eligible || ! _paths[idx].allowed()) {
							continue;
						}
						addPathToBond(idx, updatedBondedPathCount);
						++updatedBondedPathCount;
						debug("add %s (no user addr preference)", pathToStr(_paths[idx].p).c_str());
					}
				}
				// If the user prefers to only use one address type (IPv4 or IPv6)
				if (ipvPref == 4 || ipvPref == 6) {
					for (int j = 0; j < it->second.size(); j++) {
						int idx = it->second.at(j);
						if (! _paths[idx].p || ! _paths[idx].eligible) {
							continue;
						}
						if (! _paths[idx].allowed()) {
							debug("did not add %s (user addr preference %d)", pathToStr(_paths[idx].p).c_str(), ipvPref);
							continue;
						}
						addPathToBond(idx, updatedBondedPathCount);
						++updatedBondedPathCount;
						debug("add path %s (user addr preference %d)", pathToStr(_paths[idx].p).c_str(), ipvPref);
					}
				}
				// If the users prefers one address type to another, try to find at least
				// one path of that type before considering others.
				if (ipvPref == 46 || ipvPref == 64) {
					bool foundPreferredPath = false;
					// Search for preferred paths
					for (int j = 0; j < it->second.size(); j++) {
						int idx = it->second.at(j);
						if (! _paths[idx].p || ! _paths[idx].eligible || ! _paths[idx].allowed()) {
							continue;
						}
						if (_paths[idx].preferred()) {
							addPathToBond(idx, updatedBondedPathCount);
							++updatedBondedPathCount;
							debug("add %s (user addr preference %d)", pathToStr(_paths[idx].p).c_str(), ipvPref);
							foundPreferredPath = true;
						}
					}
					// Unable to find a path that matches user preference, settle for another address type
					if (! foundPreferredPath) {
						debug("did not find first-choice path type on link %s (user preference %d)", link->ifname().c_str(), ipvPref);
						for (int j = 0; j < it->second.size(); j++) {
							int idx = it->second.at(j);
							if (! _paths[idx].p || ! _paths[idx].eligible) {
								continue;
							}
							addPathToBond(idx, updatedBondedPathCount);
							++updatedBondedPathCount;
							debug("add %s (user addr preference %d)", pathToStr(_paths[idx].p).c_str(), ipvPref);
							foundPreferredPath = true;
						}
					}
				}
				++it;	// Next link
			}
			_numBondedPaths = updatedBondedPathCount;
			if (_policy == ZT_BOND_POLICY_BALANCE_RR) {
				// Cause a RR reset since the current index might no longer be valid
				_rrPacketsSentOnCurrLink = _packetsPerLink;
			}
		}
	}
}

void Bond::estimatePathQuality(int64_t now)
{
	uint32_t totUserSpecifiedLinkSpeed = 0;
	if (_numBondedPaths) {	 // Compute relative user-specified speeds of links
		for (unsigned int i = 0; i < _numBondedPaths; ++i) {
			if (_paths[i].p && _paths[i].allowed()) {
				SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());
				totUserSpecifiedLinkSpeed += link->speed();
			}
		}
		for (unsigned int i = 0; i < _numBondedPaths; ++i) {
			if (_paths[i].p && _paths[i].allowed()) {
				SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());
				link->setRelativeSpeed((uint8_t)round(((float)link->speed() / (float)totUserSpecifiedLinkSpeed) * 255));
			}
		}
	}

	float lat[ZT_MAX_PEER_NETWORK_PATHS] = { 0 };
	float pdv[ZT_MAX_PEER_NETWORK_PATHS] = { 0 };
	float plr[ZT_MAX_PEER_NETWORK_PATHS] = { 0 };
	float per[ZT_MAX_PEER_NETWORK_PATHS] = { 0 };

	float maxLAT = 0;
	float maxPDV = 0;
	float maxPLR = 0;
	float maxPER = 0;

	float quality[ZT_MAX_PEER_NETWORK_PATHS] = { 0 };
	uint8_t alloc[ZT_MAX_PEER_NETWORK_PATHS] = { 0 };

	float totQuality = 0.0f;

	// Compute initial summary statistics
	for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (! _paths[i].p || ! _paths[i].allowed()) {
			continue;
		}
		// Compute/Smooth average of real-world observations
		_paths[i].latencyMean = _paths[i].latencySamples.mean();
		_paths[i].latencyVariance = _paths[i].latencySamples.stddev();
		//_paths[i].packetErrorRatio = 1.0 - (_paths[i].packetValiditySamples.count() ? _paths[i].packetValiditySamples.mean() : 1.0);

		if (userHasSpecifiedLinkSpeeds()) {
			// Use user-reported metrics
			SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());
			if (link) {
				_paths[i].throughputMean = link->speed();
				_paths[i].throughputVariance = 0;
			}
		}

		// Drain unacknowledged QoS records
		int qosRecordTimeout = (_qosSendInterval * 3);
		std::map<uint64_t, uint64_t>::iterator it = _paths[i].qosStatsOut.begin();
		int numDroppedQosOutRecords = 0;
		while (it != _paths[i].qosStatsOut.end()) {
			if ((now - it->second) >= qosRecordTimeout) {
				it = _paths[i].qosStatsOut.erase(it);
				++numDroppedQosOutRecords;
			}
			else {
				++it;
			}
		}
		if (numDroppedQosOutRecords) {
			log("Dropped %d QOS out-records", numDroppedQosOutRecords);
		}

		it = _paths[i].qosStatsIn.begin();
		int numDroppedQosInRecords = 0;
		while (it != _paths[i].qosStatsIn.end()) {
			if ((now - it->second) >= qosRecordTimeout) {
				it = _paths[i].qosStatsIn.erase(it);
				++numDroppedQosInRecords;
			}
			else {
				++it;
			}
		}
		if (numDroppedQosInRecords) {
			log("Dropped %d QOS in-records", numDroppedQosInRecords);
		}

		quality[i] = 0;
		totQuality = 0;
		// Normalize raw observations according to sane limits and/or user specified values
		lat[i] = 1.0 / expf(4 * Utils::normalize(_paths[i].latencyMean, 0, _maxAcceptableLatency, 0, 1));
		pdv[i] = 1.0 / expf(4 * Utils::normalize(_paths[i].latencyVariance, 0, _maxAcceptablePacketDelayVariance, 0, 1));
		plr[i] = 1.0 / expf(4 * Utils::normalize(_paths[i].packetLossRatio, 0, _maxAcceptablePacketLossRatio, 0, 1));
		per[i] = 1.0 / expf(4 * Utils::normalize(_paths[i].packetErrorRatio, 0, _maxAcceptablePacketErrorRatio, 0, 1));
		// Record bond-wide maximums to determine relative values
		maxLAT = lat[i] > maxLAT ? lat[i] : maxLAT;
		maxPDV = pdv[i] > maxPDV ? pdv[i] : maxPDV;
		maxPLR = plr[i] > maxPLR ? plr[i] : maxPLR;
		maxPER = per[i] > maxPER ? per[i] : maxPER;
	}
	// Convert metrics to relative quantities and apply contribution weights
	for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i].p && _paths[i].bonded) {
			quality[i] += ((maxLAT > 0.0f ? lat[i] / maxLAT : 0.0f) * _qw[ZT_QOS_LAT_IDX]);
			quality[i] += ((maxPDV > 0.0f ? pdv[i] / maxPDV : 0.0f) * _qw[ZT_QOS_PDV_IDX]);
			quality[i] += ((maxPLR > 0.0f ? plr[i] / maxPLR : 0.0f) * _qw[ZT_QOS_PLR_IDX]);
			quality[i] += ((maxPER > 0.0f ? per[i] / maxPER : 0.0f) * _qw[ZT_QOS_PER_IDX]);
			totQuality += quality[i];
		}
	}
	// Normalize to 8-bit allocation values
	for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i].p && _paths[i].bonded) {
			alloc[i] = (uint8_t)(std::ceil((quality[i] / totQuality) * (float)255));
			_paths[i].allocation = alloc[i];
		}
	}
}

void Bond::processBalanceTasks(int64_t now)
{
	if (_allowFlowHashing) {
		/**
		 * Clean up and reset flows if necessary
		 */
		if ((now - _lastFlowExpirationCheck) > ZT_PEER_PATH_EXPIRATION) {
			Mutex::Lock _l(_flows_m);
			forgetFlowsWhenNecessary(ZT_PEER_PATH_EXPIRATION, false, now);
			std::map<int32_t, SharedPtr<Flow> >::iterator it = _flows.begin();
			while (it != _flows.end()) {
				it->second->resetByteCounts();
				++it;
			}
			_lastFlowExpirationCheck = now;
		}
		/**
		 * Re-allocate flows from dead paths
		 */
		if (_policy == ZT_BOND_POLICY_BALANCE_XOR || _policy == ZT_BOND_POLICY_BALANCE_AWARE) {
			Mutex::Lock _l(_flows_m);
			for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (! _paths[i].p) {
					continue;
				}
				if (! _paths[i].eligible && _paths[i].shouldReallocateFlows) {
					log("reallocate flows from dead link %s", pathToStr(_paths[i].p).c_str());
					std::map<int32_t, SharedPtr<Flow> >::iterator flow_it = _flows.begin();
					while (flow_it != _flows.end()) {
						if (_paths[flow_it->second->assignedPath].p == _paths[i].p) {
							if (assignFlowToBondedPath(flow_it->second, now)) {
								_paths[i].assignedFlowCount--;
							}
						}
						++flow_it;
					}
					_paths[i].shouldReallocateFlows = false;
				}
			}
		}
		/**
		 * Re-allocate flows from under-performing
		 * NOTE: This could be part of the above block but was kept separate for clarity.
		 */
		if (_policy == ZT_BOND_POLICY_BALANCE_AWARE) {
			int totalAllocation = 0;
			for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (! _paths[i].p) {
					continue;
				}
				if (_paths[i].p && _paths[i].bonded && _paths[i].eligible) {
					totalAllocation += _paths[i].allocation;
				}
			}
			unsigned char minimumAllocationValue = (uint8_t)(0.33 * ((float)totalAllocation / (float)_numBondedPaths));

			Mutex::Lock _l(_flows_m);
			for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (! _paths[i].p) {
					continue;
				}
				if (_paths[i].p && _paths[i].bonded && _paths[i].eligible && (_paths[i].allocation < minimumAllocationValue) && _paths[i].assignedFlowCount) {
					log("reallocate flows from under-performing link %s\n", pathToStr(_paths[i].p).c_str());
					std::map<int32_t, SharedPtr<Flow> >::iterator flow_it = _flows.begin();
					while (flow_it != _flows.end()) {
						if (flow_it->second->assignedPath == _paths[i].p) {
							if (assignFlowToBondedPath(flow_it->second, now)) {
								_paths[i].assignedFlowCount--;
							}
						}
						++flow_it;
					}
					_paths[i].shouldReallocateFlows = false;
				}
			}
		}
	}
}

void Bond::dequeueNextActiveBackupPath(uint64_t now)
{
	if (_abFailoverQueue.empty()) {
		return;
	}
	_abPathIdx = _abFailoverQueue.front();
	_abFailoverQueue.pop_front();
	_lastActiveBackupPathChange = now;
	for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i].p) {
			_paths[i].resetPacketCounts();
		}
	}
}

bool Bond::abForciblyRotateLink()
{
	if (_policy == ZT_BOND_POLICY_ACTIVE_BACKUP) {
		int prevPathIdx = _abPathIdx;
		dequeueNextActiveBackupPath(RR->node->now());
		log("active link rotated from %s to %s", pathToStr(_paths[prevPathIdx].p).c_str(), pathToStr(_paths[_abPathIdx].p).c_str());
		return true;
	}
	return false;
}

void Bond::processActiveBackupTasks(void* tPtr, int64_t now)
{
	int prevActiveBackupPathIdx = _abPathIdx;
	int nonPreferredPathIdx = ZT_MAX_PEER_NETWORK_PATHS;
	bool bFoundPrimaryLink = false;

	if (_abPathIdx != ZT_MAX_PEER_NETWORK_PATHS && ! _paths[_abPathIdx].p) {
		_abPathIdx = ZT_MAX_PEER_NETWORK_PATHS;
		log("main active-backup path has been removed");
	}

	/**
	 * Generate periodic status report
	 */
	if ((now - _lastBondStatusLog) > ZT_BOND_STATUS_INTERVAL) {
		_lastBondStatusLog = now;
		if (_abPathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
			log("no active link");
		}
		else if (_paths[_abPathIdx].p) {
			log("active link is %s, failover queue size is %zu", pathToStr(_paths[_abPathIdx].p).c_str(), _abFailoverQueue.size());
		}
		if (_abFailoverQueue.empty()) {
			log("failover queue is empty, bond is no longer fault-tolerant");
		}
	}
	/**
	 * Select initial "active" active-backup link
	 */
	if (_abPathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
		/**
		 * [Automatic mode]
		 * The user has not explicitly specified links or their failover schedule,
		 * the bonding policy will now select the first eligible path and set it as
		 * its active backup path, if a substantially better path is detected the bonding
		 * policy will assign it as the new active backup path. If the path fails it will
		 * simply find the next eligible path.
		 */
		if (! userHasSpecifiedLinks()) {
			for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (_paths[i].p && _paths[i].eligible) {
					SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());
					if (link) {
						log("found eligible link %s", pathToStr(_paths[i].p).c_str());
						_abPathIdx = i;
						break;
					}
				}
			}
		}

		/**
		 * [Manual mode]
		 * The user has specified links or failover rules that the bonding policy should adhere to.
		 */
		else if (userHasSpecifiedLinks()) {
			if (userHasSpecifiedPrimaryLink()) {
				for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
					if (! _paths[i].p) {
						continue;
					}
					SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());
					if (_paths[i].eligible && link->primary()) {
						if (! _paths[i].preferred()) {
							// Found path on primary link, take note in case we don't find a preferred path
							nonPreferredPathIdx = i;
							bFoundPrimaryLink = true;
						}
						if (_paths[i].preferred()) {
							_abPathIdx = i;
							bFoundPrimaryLink = true;
							if (_paths[_abPathIdx].p) {
								SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[_abPathIdx].p->localSocket());
								if (link) {
									log("found preferred primary link %s", pathToStr(_paths[_abPathIdx].p).c_str());
								}
								break;	 // Found preferred path on primary link
							}
						}
					}
				}
				if (bFoundPrimaryLink && (nonPreferredPathIdx != ZT_MAX_PEER_NETWORK_PATHS)) {
					log("found non-preferred primary link");
					_abPathIdx = nonPreferredPathIdx;
				}
				if (_abPathIdx == ZT_MAX_PEER_NETWORK_PATHS) {
					log("user-designated primary link is not yet ready");
					// TODO: Should wait for some time (failover interval?) and then switch to spare link
				}
			}

			else if (! userHasSpecifiedPrimaryLink()) {
				log("user did not specify a primary link, select first available link");
				for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
					if (_paths[i].p && _paths[i].eligible) {
						_abPathIdx = i;
						break;
					}
				}
				if (_abPathIdx != ZT_MAX_PEER_NETWORK_PATHS) {
					if (_paths[_abPathIdx].p) {
						SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[_abPathIdx].p->localSocket());
						if (link) {
							log("select non-primary link %s", pathToStr(_paths[_abPathIdx].p).c_str());
						}
					}
				}
			}
		}
	}

	// Short-circuit if we don't have an active link yet. Everything below is optimization from the base case
	if (_abPathIdx < 0 || _abPathIdx == ZT_MAX_PEER_NETWORK_PATHS || (! _paths[_abPathIdx].p)) {
		return;
	}

	// Remove ineligible paths from the failover link queue
	for (std::deque<int>::iterator it(_abFailoverQueue.begin()); it != _abFailoverQueue.end();) {
		if (_paths[(*it)].p && ! _paths[(*it)].eligible) {
			SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[(*it)].p->localSocket());
			it = _abFailoverQueue.erase(it);
			if (link) {
				log("link %s is ineligible, removing from failover queue (%zu links in queue)", pathToStr(_paths[_abPathIdx].p).c_str(), _abFailoverQueue.size());
			}
		}
		else {
			++it;
		}
	}
	/**
	 * Failover instructions were provided by user, build queue according those as well as IPv
	 * preference, disregarding performance.
	 */
	if (userHasSpecifiedFailoverInstructions()) {
		/**
		 * Clear failover scores
		 */
		for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (_paths[i].p) {
				_paths[i].failoverScore = 0;
			}
		}
		// Follow user-specified failover instructions
		for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (! _paths[i].p || ! _paths[i].allowed() || ! _paths[i].eligible) {
				continue;
			}
			SharedPtr<Link> link = RR->bc->getLinkBySocket(_policyAlias, _paths[i].p->localSocket());

			int failoverScoreHandicap = _paths[i].failoverScore;
			if (_paths[i].preferred()) {
				failoverScoreHandicap += ZT_BOND_FAILOVER_HANDICAP_PREFERRED;
			}
			if (link->primary()) {
				// If using "optimize" primary re-select mode, ignore user link designations
				failoverScoreHandicap += ZT_BOND_FAILOVER_HANDICAP_PRIMARY;
			}
			if (! _paths[i].failoverScore) {
				// If we didn't inherit a failover score from a "parent" that wants to use this path as a failover
				int newHandicap = failoverScoreHandicap ? failoverScoreHandicap : _paths[i].allocation;
				_paths[i].failoverScore = newHandicap;
			}
			SharedPtr<Link> failoverLink;
			if (link->failoverToLink().length()) {
				failoverLink = RR->bc->getLinkByName(_policyAlias, link->failoverToLink());
			}
			if (failoverLink) {
				for (int j = 0; j < ZT_MAX_PEER_NETWORK_PATHS; j++) {
					if (_paths[j].p && getLink(_paths[j].p) == failoverLink.ptr()) {
						int inheritedHandicap = failoverScoreHandicap - 10;
						int newHandicap = _paths[j].failoverScore > inheritedHandicap ? _paths[j].failoverScore : inheritedHandicap;
						if (! _paths[j].preferred()) {
							newHandicap--;
						}
						_paths[j].failoverScore = newHandicap;
					}
				}
			}
			if (_paths[i].p) {
				if (_paths[i].p.ptr() != _paths[_abPathIdx].p.ptr()) {
					bool bFoundPathInQueue = false;
					for (std::deque<int>::iterator it(_abFailoverQueue.begin()); it != _abFailoverQueue.end(); ++it) {
						if (_paths[(*it)].p && (_paths[i].p.ptr() == _paths[(*it)].p.ptr())) {
							bFoundPathInQueue = true;
						}
					}
					if (! bFoundPathInQueue) {
						_abFailoverQueue.push_front(i);
						log("add link %s to failover queue (%zu links in queue)", pathToStr(_paths[i].p).c_str(), _abFailoverQueue.size());
						addPathToBond(0, i);
					}
				}
			}
		}
	}
	/**
	 * No failover instructions provided by user, build queue according to performance
	 * and IPv preference.
	 */
	else if (! userHasSpecifiedFailoverInstructions()) {
		for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (! _paths[i].p || ! _paths[i].allowed() || ! _paths[i].eligible) {
				continue;
			}
			int failoverScoreHandicap = 0;
			if (_paths[i].preferred()) {
				failoverScoreHandicap = ZT_BOND_FAILOVER_HANDICAP_PREFERRED;
			}
			if (! _paths[i].eligible) {
				failoverScoreHandicap = -10000;
			}
			if (getLink(_paths[i].p)->primary() && _abLinkSelectMethod != ZT_BOND_RESELECTION_POLICY_OPTIMIZE) {
				// If using "optimize" primary re-select mode, ignore user link designations
				failoverScoreHandicap = ZT_BOND_FAILOVER_HANDICAP_PRIMARY;
			}
			/*
			if (_paths[i].p.ptr() == _paths[_negotiatedPathIdx].p.ptr()) {
				_paths[i].negotiated = true;
				failoverScoreHandicap = ZT_BOND_FAILOVER_HANDICAP_NEGOTIATED;
			}
			else {
				_paths[i].negotiated = false;
			}
			*/
			_paths[i].failoverScore = _paths[i].allocation + failoverScoreHandicap;
			if (_paths[i].p.ptr() != _paths[_abPathIdx].p.ptr()) {
				bool bFoundPathInQueue = false;
				for (std::deque<int>::iterator it(_abFailoverQueue.begin()); it != _abFailoverQueue.end(); ++it) {
					if (_paths[i].p.ptr() == _paths[(*it)].p.ptr()) {
						bFoundPathInQueue = true;
					}
				}
				if (! bFoundPathInQueue) {
					_abFailoverQueue.push_front(i);
					log("add link %s to failover queue (%zu links in queue)", pathToStr(_paths[i].p).c_str(), _abFailoverQueue.size());
					addPathToBond(0, i);
				}
			}
		}
	}
	// Sort queue based on performance
	if (! _abFailoverQueue.empty()) {
		for (int i = 0; i < _abFailoverQueue.size(); i++) {
			int value_to_insert = _abFailoverQueue[i];
			int hole_position = i;
			while (hole_position > 0 && (_abFailoverQueue[hole_position - 1] > value_to_insert)) {
				_abFailoverQueue[hole_position] = _abFailoverQueue[hole_position - 1];
				hole_position = hole_position - 1;
			}
			_abFailoverQueue[hole_position] = value_to_insert;
		}
	}

	/**
	 * Short-circuit if we have no queued paths
	 */
	if (_abFailoverQueue.empty()) {
		return;
	}

	/**
	 * Fulfill primary re-select obligations
	 */
	if (! _paths[_abPathIdx].eligible) {   // Implicit ZT_BOND_RESELECTION_POLICY_FAILURE
		log("link %s has failed, select link from failover queue (%zu links in queue)", pathToStr(_paths[_abPathIdx].p).c_str(), _abFailoverQueue.size());
		if (! _abFailoverQueue.empty()) {
			dequeueNextActiveBackupPath(now);
			log("active link switched to %s", pathToStr(_paths[_abPathIdx].p).c_str());
		}
		else {
			log("failover queue is empty, no links to choose from");
		}
	}
	/**
	 * Detect change to prevent flopping during later optimization step.
	 */
	if (prevActiveBackupPathIdx != _abPathIdx) {
		_lastActiveBackupPathChange = now;
	}
	if (_abLinkSelectMethod == ZT_BOND_RESELECTION_POLICY_ALWAYS) {
		if (! getLink(_paths[_abPathIdx].p)->primary() && _paths[_abFailoverQueue.front()].p && getLink(_paths[_abFailoverQueue.front()].p)->primary()) {
			dequeueNextActiveBackupPath(now);
			log("switch back to available primary link %s (select mode: always)", pathToStr(_paths[_abPathIdx].p).c_str());
		}
	}
	if (_abLinkSelectMethod == ZT_BOND_RESELECTION_POLICY_BETTER) {
		if (! getLink(_paths[_abPathIdx].p)->primary()) {
			// Active backup has switched to "better" primary link according to re-select policy.
			if (_paths[_abFailoverQueue.front()].p && getLink(_paths[_abFailoverQueue.front()].p)->primary() && (_paths[_abFailoverQueue.front()].failoverScore > _paths[_abPathIdx].failoverScore)) {
				dequeueNextActiveBackupPath(now);
				log("switch back to user-defined primary link %s (select mode: better)", pathToStr(_paths[_abPathIdx].p).c_str());
			}
		}
	}
	if (_abLinkSelectMethod == ZT_BOND_RESELECTION_POLICY_OPTIMIZE && ! _abFailoverQueue.empty()) {
		/**
		 * Implement link negotiation that was previously-decided
		 */
		if (_paths[_abFailoverQueue.front()].negotiated) {
			dequeueNextActiveBackupPath(now);
			_lastPathNegotiationCheck = now;
			log("switch negotiated link %s (select mode: optimize)", pathToStr(_paths[_abPathIdx].p).c_str());
		}
		else {
			// Try to find a better path and automatically switch to it -- not too often, though.
			if ((now - _lastActiveBackupPathChange) > ZT_BOND_OPTIMIZE_INTERVAL) {
				if (! _abFailoverQueue.empty()) {
					int newFScore = _paths[_abFailoverQueue.front()].failoverScore;
					int prevFScore = _paths[_abPathIdx].failoverScore;
					// Establish a minimum switch threshold to prevent flapping
					int failoverScoreDifference = _paths[_abFailoverQueue.front()].failoverScore - _paths[_abPathIdx].failoverScore;
					int thresholdQuantity = (int)(ZT_BOND_ACTIVE_BACKUP_OPTIMIZE_MIN_THRESHOLD * (float)_paths[_abPathIdx].allocation);
					if ((failoverScoreDifference > 0) && (failoverScoreDifference > thresholdQuantity)) {
						SharedPtr<Path> oldPath = _paths[_abPathIdx].p;
						dequeueNextActiveBackupPath(now);
						log("switch from %s (score: %d) to better link %s (score: %d) (select mode: optimize)", pathToStr(oldPath).c_str(), prevFScore, pathToStr(_paths[_abPathIdx].p).c_str(), newFScore);
					}
				}
			}
		}
	}
}

void Bond::initTimers()
{
	_lastFlowExpirationCheck = 0;
	_lastFlowRebalance = 0;
	_lastSentPathNegotiationRequest = 0;
	_lastPathNegotiationCheck = 0;
	_lastPathNegotiationReceived = 0;
	_lastQoSRateCheck = 0;
	_lastQualityEstimation = 0;
	_lastBondStatusLog = 0;
	_lastSummaryDump = 0;
	_lastActiveBackupPathChange = 0;
	_lastFrame = 0;
	_lastBackgroundTaskCheck = 0;
}

void Bond::setBondParameters(int policy, SharedPtr<Bond> templateBond, bool useTemplate)
{
	// Sanity check for policy

	_defaultPolicy = (_defaultPolicy <= ZT_BOND_POLICY_NONE || _defaultPolicy > ZT_BOND_POLICY_BALANCE_AWARE) ? ZT_BOND_POLICY_NONE : _defaultPolicy;
	_policy = (policy <= ZT_BOND_POLICY_NONE || policy > ZT_BOND_POLICY_BALANCE_AWARE) ? ZT_BOND_POLICY_NONE : _defaultPolicy;

	// Check if non-leaf to prevent spamming infrastructure
	if (_peer) {
		ZT_PeerRole role = RR->topology->role(_peer->address());
		_isLeaf = (role != ZT_PEER_ROLE_PLANET && role != ZT_PEER_ROLE_MOON);
	}

	// Flows

	_allowFlowHashing = false;

	// Path negotiation

	_allowPathNegotiation = false;
	_pathNegotiationCutoffCount = 0;
	_localUtility = 0;
	_negotiatedPathIdx = 0;

	// QOS Verb (and related checks)

	_qosCutoffCount = 0;

	// User preferences which may override the default bonding algorithm's behavior

	_userHasSpecifiedPrimaryLink = false;
	_userHasSpecifiedFailoverInstructions = false;
	_userHasSpecifiedLinkSpeeds = 0;

	// Bond status

	_numAliveLinks = 0;
	_numTotalLinks = 0;
	_numBondedPaths = 0;

	// active-backup

	_abPathIdx = ZT_MAX_PEER_NETWORK_PATHS;

	// rr

	_rrPacketsSentOnCurrLink = 0;
	_rrIdx = 0;

	// General parameters

	_downDelay = 0;
	_upDelay = 0;
	_monitorInterval = 0;

	// (Sane?) limits

	_maxAcceptableLatency = 100;
	_maxAcceptablePacketDelayVariance = 50;
	_maxAcceptablePacketLossRatio = 0.10f;
	_maxAcceptablePacketErrorRatio = 0.10f;

	// balance-aware

	_totalBondUnderload = 0;
	_overheadBytes = 0;

	/**
	 * Policy-specific defaults
	 */
	switch (_policy) {
		case ZT_BOND_POLICY_ACTIVE_BACKUP:
			_abLinkSelectMethod = ZT_BOND_RESELECTION_POLICY_OPTIMIZE;
			break;
		case ZT_BOND_POLICY_BROADCAST:
			_downDelay = 30000;
			_upDelay = 0;
			break;
		case ZT_BOND_POLICY_BALANCE_RR:
			_packetsPerLink = 64;
			break;
		case ZT_BOND_POLICY_BALANCE_XOR:
			_allowFlowHashing = true;
			break;
		case ZT_BOND_POLICY_BALANCE_AWARE:
			_allowFlowHashing = true;
			break;
		default:
			break;
	}

	_qw[ZT_QOS_LAT_IDX] = 0.3f;
	_qw[ZT_QOS_LTM_IDX] = 0.1f;
	_qw[ZT_QOS_PDV_IDX] = 0.3f;
	_qw[ZT_QOS_PLR_IDX] = 0.1f;
	_qw[ZT_QOS_PER_IDX] = 0.1f;
	_qw[ZT_QOS_SCP_IDX] = 0.1f;

	_failoverInterval = ZT_BOND_FAILOVER_DEFAULT_INTERVAL;

	/* If a user has specified custom parameters for this bonding policy, overlay them onto the defaults */
	if (useTemplate) {
		_policyAlias = templateBond->_policyAlias;
		_policy = templateBond->policy();
		_failoverInterval = templateBond->_failoverInterval >= ZT_BOND_FAILOVER_MIN_INTERVAL ? templateBond->_failoverInterval : ZT_BOND_FAILOVER_MIN_INTERVAL;
		_downDelay = templateBond->_downDelay;
		_upDelay = templateBond->_upDelay;
		_abLinkSelectMethod = templateBond->_abLinkSelectMethod;
		memcpy(_qw, templateBond->_qw, ZT_QOS_WEIGHT_SIZE * sizeof(float));
	}

	if (! _isLeaf) {
		_policy = ZT_BOND_POLICY_ACTIVE_BACKUP;
	}

	// Timer geometry

	_monitorInterval = _failoverInterval / ZT_BOND_ECHOS_PER_FAILOVER_INTERVAL;
	_qualityEstimationInterval = _failoverInterval * 2;
	_qosSendInterval = _failoverInterval * 2;
	_qosCutoffCount = 0;
	_defaultPathRefractoryPeriod = 8000;
}

void Bond::setUserQualityWeights(float weights[], int len)
{
	if (len == ZT_QOS_WEIGHT_SIZE) {
		float weightTotal = 0.0;
		for (unsigned int i = 0; i < ZT_QOS_WEIGHT_SIZE; ++i) {
			weightTotal += weights[i];
		}
		if (weightTotal > 0.99 && weightTotal < 1.01) {
			memcpy(_qw, weights, len * sizeof(float));
		}
	}
}

SharedPtr<Link> Bond::getLink(const SharedPtr<Path>& path)
{
	return RR->bc->getLinkBySocket(_policyAlias, path->localSocket());
}

std::string Bond::pathToStr(const SharedPtr<Path>& path)
{
#ifdef ZT_TRACE
	if (path) {
		char pathStr[64] = { 0 };
		char fullPathStr[384] = { 0 };
		path->address().toString(pathStr);
		snprintf(fullPathStr, 384, "%.16llx-%s/%s", (unsigned long long)(path->localSocket()), getLink(path)->ifname().c_str(), pathStr);
		return std::string(fullPathStr);
	}
	return "";
#else
	return "";
#endif
}

void Bond::dumpPathStatus(int64_t now, int pathIdx)
{
#ifdef ZT_TRACE
	std::string aliveOrDead = _paths[pathIdx].alive ? std::string("alive") : std::string("dead");
	std::string eligibleOrNot = _paths[pathIdx].eligible ? std::string("eligible") : std::string("ineligible");
	std::string bondedOrNot = _paths[pathIdx].bonded ? std::string("bonded") : std::string("unbonded");
	log("path[%2d] --- %5s (in %7lld, out: %7lld), %10s, %8s, flows=%-6d lat=%-8.3f pdv=%-7.3f err=%-6.4f loss=%-6.4f alloc=%-3d --- (%s)",
		pathIdx,
		aliveOrDead.c_str(),
		static_cast<long long int>(_paths[pathIdx].p->age(now)),
		static_cast<long long int>(now - _paths[pathIdx].p->_lastOut),
		eligibleOrNot.c_str(),
		bondedOrNot.c_str(),
		_paths[pathIdx].assignedFlowCount,
		_paths[pathIdx].latencyMean,
		_paths[pathIdx].latencyVariance,
		_paths[pathIdx].packetErrorRatio,
		_paths[pathIdx].packetLossRatio,
		_paths[pathIdx].allocation,
		pathToStr(_paths[pathIdx].p).c_str());
#endif
}

void Bond::dumpInfo(int64_t now, bool force)
{
#ifdef ZT_TRACE
	uint64_t timeSinceLastDump = now - _lastSummaryDump;
	if (! force && timeSinceLastDump < ZT_BOND_STATUS_INTERVAL) {
		return;
	}
	_lastSummaryDump = now;
	float overhead = (_overheadBytes / (timeSinceLastDump / 1000.0f) / 1000.0f);
	_overheadBytes = 0;
	log("bond: bp=%d, fi=%d, mi=%d, ud=%d, dd=%d, flows=%lu, leaf=%d, overhead=%f KB/s, links=(%d/%d)",
		_policy,
		_failoverInterval,
		_monitorInterval,
		_upDelay,
		_downDelay,
		(unsigned long)_flows.size(),
		_isLeaf,
		overhead,
		_numAliveLinks,
		_numTotalLinks);
	for (int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i].p) {
			dumpPathStatus(now, i);
		}
	}
	log("");
#endif
}

}	// namespace ZeroTier
