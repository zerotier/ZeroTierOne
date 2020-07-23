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

#include "Constants.hpp"
#include "BondController.hpp"
#include "Peer.hpp"

namespace ZeroTier {

int BondController::_minReqPathMonitorInterval;
uint8_t BondController::_defaultBondingPolicy;

BondController::BondController(const RuntimeEnvironment *renv) :
	RR(renv)
{
	bondStartTime = RR->node->now();
	_defaultBondingPolicy = ZT_BONDING_POLICY_NONE;
}

bool BondController::linkAllowed(std::string &policyAlias, SharedPtr<Link> link)
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

void BondController::addCustomLink(std::string& policyAlias, SharedPtr<Link> link)
{
	Mutex::Lock _l(_links_m);
	_linkDefinitions[policyAlias].push_back(link);
	auto search = _interfaceToLinkMap[policyAlias].find(link->ifname());
	if (search == _interfaceToLinkMap[policyAlias].end()) {
		link->setAsUserSpecified(true);
		_interfaceToLinkMap[policyAlias].insert(std::pair<std::string, SharedPtr<Link>>(link->ifname(), link));
	} else {
		//fprintf(stderr, "link already exists=%s\n", link->ifname().c_str());
		// Link is already defined, overlay user settings
	}
}

bool BondController::addCustomPolicy(const SharedPtr<Bond>& newBond)
{
	Mutex::Lock _l(_bonds_m);
	if (!_bondPolicyTemplates.count(newBond->policyAlias())) {
		_bondPolicyTemplates[newBond->policyAlias()] = newBond;
		return true;
	}
	return false;
}

bool BondController::assignBondingPolicyToPeer(int64_t identity, const std::string& policyAlias)
{
	Mutex::Lock _l(_bonds_m);
	if (!_policyTemplateAssignments.count(identity)) {
		_policyTemplateAssignments[identity] = policyAlias;
		return true;
	}
	return false;
}

SharedPtr<Bond> BondController::createTransportTriggeredBond(const RuntimeEnvironment *renv, const SharedPtr<Peer>& peer)
{
	Mutex::Lock _l(_bonds_m);
	int64_t identity = peer->identity().address().toInt();
	Bond *bond = nullptr;
	if (!_bonds.count(identity)) {
		std::string policyAlias;
		//fprintf(stderr, "new bond, registering for %llx\n", identity);
		if (!_policyTemplateAssignments.count(identity)) {
			if (_defaultBondingPolicy) {
				//fprintf(stderr, "  no assignment, using default (%d)\n", _defaultBondingPolicy);
				bond = new Bond(renv, _defaultBondingPolicy, peer);
			}
			if (!_defaultBondingPolicy && _defaultBondingPolicyStr.length()) {
				//fprintf(stderr, "  no assignment, using default custom (%s)\n", _defaultBondingPolicyStr.c_str());
				bond = new Bond(renv, _bondPolicyTemplates[_defaultBondingPolicyStr].ptr(), peer);
			}
		}
		else {
			//fprintf(stderr, "  assignment found for %llx, using it as a template (%s)\n", identity,_policyTemplateAssignments[identity].c_str());
			if (!_bondPolicyTemplates[_policyTemplateAssignments[identity]]) {
				//fprintf(stderr, "unable to locate template (%s), ignoring assignment for (%llx), using defaults\n", _policyTemplateAssignments[identity].c_str(), identity);
				bond = new Bond(renv, _defaultBondingPolicy, peer);
			}
			else {
				bond = new Bond(renv, _bondPolicyTemplates[_policyTemplateAssignments[identity]].ptr(), peer);
			}
		}
	}
	else {
		//fprintf(stderr, "bond already exists for %llx.\n", identity);
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

SharedPtr<Link> BondController::getLinkBySocket(const std::string& policyAlias, uint64_t localSocket)
{
	Mutex::Lock _l(_links_m);
	char ifname[16];
	_phy->getIfName((PhySocket *) ((uintptr_t)localSocket), ifname, 16);
	std::string ifnameStr(ifname);
	auto search = _interfaceToLinkMap[policyAlias].find(ifnameStr);
	if (search == _interfaceToLinkMap[policyAlias].end()) {
		SharedPtr<Link> s = new Link(ifnameStr, 0, 0, 0, 0, 0, true, ZT_MULTIPATH_SLAVE_MODE_SPARE, "", 0.0);
		_interfaceToLinkMap[policyAlias].insert(std::pair<std::string,SharedPtr<Link> >(ifnameStr, s));
		return s;
	}
	else {
		return search->second;
	}
}

SharedPtr<Link> BondController::getLinkByName(const std::string& policyAlias, const std::string& ifname)
{
	Mutex::Lock _l(_links_m);
	auto search = _interfaceToLinkMap[policyAlias].find(ifname);
	if (search != _interfaceToLinkMap[policyAlias].end()) {
		return search->second;
	}
	return SharedPtr<Link>();
}

bool BondController::allowedToBind(const std::string& ifname)
{
	return true;
	/*
	if (!_defaultBondingPolicy) {
		return true; // no restrictions
	}
	Mutex::Lock _l(_links_m);
	if (_interfaceToLinkMap.empty()) {
		return true; // no restrictions
	}
	std::map<std::string, std::map<std::string, SharedPtr<Link> > >::iterator policyItr = _interfaceToLinkMap.begin();
	while (policyItr != _interfaceToLinkMap.end()) {
		std::map<std::string, SharedPtr<Link> >::iterator linkItr = policyItr->second.begin();
		while (linkItr != policyItr->second.end()) {
			if (linkItr->first == ifname) {
				return true;
			}
			++linkItr;
		}
		++policyItr;
	}
	return false;
	*/
}

void BondController::processBackgroundTasks(void *tPtr, const int64_t now)
{
	Mutex::Lock _l(_bonds_m);
	std::map<int64_t,SharedPtr<Bond> >::iterator bondItr = _bonds.begin();
	while (bondItr != _bonds.end()) {
		bondItr->second->processBackgroundTasks(tPtr, now);
		++bondItr;
	}
}

} // namespace ZeroTier