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

#include "BondController.hpp"
#include "Peer.hpp"

namespace ZeroTier {

int BondController::_minReqPathMonitorInterval;
uint8_t BondController::_defaultBondingPolicy;

BondController::BondController(const RuntimeEnvironment *renv) :
	RR(renv)
{
	bondStartTime = RR->node->now();
}

bool BondController::slaveAllowed(std::string &policyAlias, SharedPtr<Slave> slave)
{
	bool foundInDefinitions = false;
	if (_slaveDefinitions.count(policyAlias)) {
		auto it = _slaveDefinitions[policyAlias].begin();
		while (it != _slaveDefinitions[policyAlias].end()) {
			if (slave->ifname() == (*it)->ifname()) {
				foundInDefinitions = true;
				break;
			}
			++it;
		}
	}
	return _slaveDefinitions[policyAlias].empty() || foundInDefinitions;
}

void BondController::addCustomSlave(std::string& policyAlias, SharedPtr<Slave> slave)
{
	Mutex::Lock _l(_slaves_m);
	_slaveDefinitions[policyAlias].push_back(slave);
	auto search = _interfaceToSlaveMap[policyAlias].find(slave->ifname());
	if (search == _interfaceToSlaveMap[policyAlias].end()) {
		slave->setAsUserSpecified(true);
		_interfaceToSlaveMap[policyAlias].insert(std::pair<std::string, SharedPtr<Slave>>(slave->ifname(), slave));
	} else {
		fprintf(stderr, "slave already exists=%s\n", slave->ifname().c_str());
		// Slave is already defined, overlay user settings
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
	fprintf(stderr, "createTransportTriggeredBond\n");
	Mutex::Lock _l(_bonds_m);
	int64_t identity = peer->identity().address().toInt();
	Bond *bond = nullptr;
	if (!_bonds.count(identity)) {
		std::string policyAlias;
		int _defaultBondingPolicy = defaultBondingPolicy();
		fprintf(stderr, "new bond, registering for %llx\n", identity);
		if (!_policyTemplateAssignments.count(identity)) {
			if (defaultBondingPolicy()) {
				fprintf(stderr, "  no assignment, using default (%d)\n", _defaultBondingPolicy);
				bond = new Bond(renv, _defaultBondingPolicy, peer);
			}
			if (!_defaultBondingPolicy && _defaultBondingPolicyStr.length()) {
				fprintf(stderr, "  no assignment, using default custom (%s)\n", _defaultBondingPolicyStr.c_str());
				bond = new Bond(renv, _bondPolicyTemplates[_defaultBondingPolicyStr].ptr(), peer);
			}
		}
		else {
			fprintf(stderr, "  assignment found for %llx, using it as a template (%s)\n", identity,_policyTemplateAssignments[identity].c_str());
			if (!_bondPolicyTemplates[_policyTemplateAssignments[identity]]) {
				fprintf(stderr, "unable to locate template (%s), ignoring assignment for (%llx), using defaults\n", _policyTemplateAssignments[identity].c_str(), identity);
				bond = new Bond(renv, _defaultBondingPolicy, peer);
			}
			else {
				bond = new Bond(renv, _bondPolicyTemplates[_policyTemplateAssignments[identity]].ptr(), peer);
			}
		}
	}
	else {
		fprintf(stderr, "bond already exists for %llx.\n", identity);
	}
	if (bond) {
		_bonds[identity] = bond;
		/**
		 * Determine if user has specified anything that could affect the bonding policy's decisions
		 */
		if (_interfaceToSlaveMap.count(bond->policyAlias())) {
			std::map<std::string, SharedPtr<Slave> >::iterator it = _interfaceToSlaveMap[bond->policyAlias()].begin();
			while (it != _interfaceToSlaveMap[bond->policyAlias()].end()) {
				if (it->second->isUserSpecified()) {
					bond->_userHasSpecifiedSlaves = true;
				}
				if (it->second->isUserSpecified() && it->second->primary()) {
					bond->_userHasSpecifiedPrimarySlave = true;
				}
				if (it->second->isUserSpecified() && it->second->userHasSpecifiedFailoverInstructions()) {
					bond->_userHasSpecifiedFailoverInstructions = true;
				}
				if (it->second->isUserSpecified() && (it->second->speed() > 0)) {
					bond->_userHasSpecifiedSlaveSpeeds = true;
				}
				++it;
			}
		}
		return bond;
	}
	return SharedPtr<Bond>();
}

SharedPtr<Slave> BondController::getSlaveBySocket(const std::string& policyAlias, uint64_t localSocket)
{
	Mutex::Lock _l(_slaves_m);
	char ifname[16];
	_phy->getIfName((PhySocket *) ((uintptr_t)localSocket), ifname, 16);
	std::string ifnameStr(ifname);
	auto search = _interfaceToSlaveMap[policyAlias].find(ifnameStr);
	if (search == _interfaceToSlaveMap[policyAlias].end()) {
		SharedPtr<Slave> s = new Slave(ifnameStr, 0, 0, 0, 0, 0, true, ZT_MULTIPATH_SLAVE_MODE_SPARE, "", 0.0);
		_interfaceToSlaveMap[policyAlias].insert(std::pair<std::string,SharedPtr<Slave> >(ifnameStr, s));
		return s;
	}
	else {
		return search->second;
	}
}

SharedPtr<Slave> BondController::getSlaveByName(const std::string& policyAlias, const std::string& ifname)
{
	Mutex::Lock _l(_slaves_m);
	auto search = _interfaceToSlaveMap[policyAlias].find(ifname);
	if (search != _interfaceToSlaveMap[policyAlias].end()) {
		return search->second;
	}
	return SharedPtr<Slave>();
}

bool BondController::allowedToBind(const std::string& ifname)
{
	return true;
	/*
	if (!_defaultBondingPolicy) {
		return true; // no restrictions
	}
	Mutex::Lock _l(_slaves_m);
	if (_interfaceToSlaveMap.empty()) {
		return true; // no restrictions
	}
	std::map<std::string, std::map<std::string, SharedPtr<Slave> > >::iterator policyItr = _interfaceToSlaveMap.begin();
	while (policyItr != _interfaceToSlaveMap.end()) {
		std::map<std::string, SharedPtr<Slave> >::iterator slaveItr = policyItr->second.begin();
		while (slaveItr != policyItr->second.end()) {
			if (slaveItr->first == ifname) {
				return true;
			}
			++slaveItr;
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