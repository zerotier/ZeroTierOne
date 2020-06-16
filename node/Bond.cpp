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

#include <cmath>

#include "Peer.hpp"
#include "Bond.hpp"
#include "Switch.hpp"
#include "Flow.hpp"
#include "Path.hpp"

namespace ZeroTier {

Bond::Bond(const RuntimeEnvironment *renv, int policy, const SharedPtr<Peer>& peer) :
	RR(renv),
	_peer(peer)
{
	// TODO: Remove for production
	_header=false;
	_lastLogTS = RR->node->now();
	_lastPrintTS = RR->node->now();
	setReasonableDefaults(policy, SharedPtr<Bond>(), false);
	_policyAlias = BondController::getPolicyStrByCode(policy);
}

Bond::Bond(const RuntimeEnvironment *renv, std::string& basePolicy, std::string& policyAlias, const SharedPtr<Peer>& peer) :
	RR(renv),
	_policyAlias(policyAlias),
	_peer(peer)
{
	setReasonableDefaults(BondController::getPolicyCodeByStr(basePolicy), SharedPtr<Bond>(), false);
}

Bond::Bond(const RuntimeEnvironment *renv, SharedPtr<Bond> originalBond, const SharedPtr<Peer>& peer) :
	RR(renv),
	_peer(peer)
{
	// TODO: Remove for production
	_header=false;
	_lastLogTS = RR->node->now();
	_lastPrintTS = RR->node->now();
	setReasonableDefaults(originalBond->_bondingPolicy, originalBond, true);
}

void Bond::nominatePath(const SharedPtr<Path>& path, int64_t now)
{
	char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "nominatePath: %s %s\n", getSlave(path)->ifname().c_str(), pathStr);
	Mutex::Lock _l(_paths_m);
	if (!RR->bc->slaveAllowed(_policyAlias, getSlave(path))) {
		return;
	}
	bool alreadyPresent = false;
	for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (path.ptr() == _paths[i].ptr()) {
			fprintf(stderr, "previously encountered path, not notifying bond (%s)\n", pathStr);
			alreadyPresent = true;
			break;
		}
	}
	if (!alreadyPresent) {
		for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if (!_paths[i]) {
				fprintf(stderr, "notifyOfNewPath(): Setting path %s to idx=%d\n", pathStr, i);
				_paths[i] = path;
				//_paths[i]->slave = RR->bc->getSlaveBySocket(_policyAlias, path->localSocket());
				_paths[i]->startTrial(now);
				break;
			}
		}
	}
	curateBond(now, true);
	estimatePathQuality(now);
}

SharedPtr<Path> Bond::getAppropriatePath(int64_t now, int32_t flowId)
{
	Mutex::Lock _l(_paths_m);
	/**
	 * active-backup
	 */
	if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP) {
		if (_abPath) {
			return _abPath;
		}
	}
	/**
	 * broadcast
	 */
	if (_bondingPolicy == ZT_BONDING_POLICY_BROADCAST) {
		return SharedPtr<Path>(); // Handled in Switch::_trySend()
	}
	if (!_numBondedPaths) {
		return SharedPtr<Path>(); // No paths assigned to bond yet, cannot balance traffic
	}
	/**
	 * balance-rr
	 */
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_RR) {
		if (!_allowFlowHashing) {
			//fprintf(stderr, "_rrPacketsSentOnCurrSlave=%d, _numBondedPaths=%d, _rrIdx=%d\n", _rrPacketsSentOnCurrSlave, _numBondedPaths, _rrIdx);
			if (_packetsPerSlave == 0) {
				// Randomly select a path
				return _paths[_bondedIdx[_freeRandomByte % _numBondedPaths]]; // TODO: Optimize
			}
			if (_rrPacketsSentOnCurrSlave < _packetsPerSlave) {
				// Continue to use this slave
				++_rrPacketsSentOnCurrSlave;
				return _paths[_bondedIdx[_rrIdx]];
			}
			// Reset striping counter
			_rrPacketsSentOnCurrSlave = 0;
			if (_numBondedPaths == 1) {
				_rrIdx = 0;
			}
			else {
				int _tempIdx = _rrIdx;
				for (int searchCount = 0; searchCount < (_numBondedPaths-1); searchCount++) {
					_tempIdx = (_tempIdx == (_numBondedPaths-1)) ? 0 : _tempIdx+1;
					if (_bondedIdx[_tempIdx] != ZT_MAX_PEER_NETWORK_PATHS) {
						if (_paths[_bondedIdx[_tempIdx]] && _paths[_bondedIdx[_tempIdx]]->eligible(now,_ackSendInterval)) {
							_rrIdx = _tempIdx;
							break;
						}
					}
				}
			}
			if (_paths[_bondedIdx[_rrIdx]]) {
				return _paths[_bondedIdx[_rrIdx]];
			}
		}
	}
	/**
	 * balance-xor
	 */
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR || _bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
		if (!_allowFlowHashing || flowId == -1) {
			// No specific path required for unclassified traffic, send on anything
			return _paths[_bondedIdx[_freeRandomByte % _numBondedPaths]]; // TODO: Optimize
		}
		else if (_allowFlowHashing) {
			// TODO: Optimize
			Mutex::Lock _l(_flows_m);
			SharedPtr<Flow> flow;
			if (_flows.count(flowId)) {
				flow = _flows[flowId];
				flow->updateActivity(now);
			}
			else {
				unsigned char entropy;
				Utils::getSecureRandom(&entropy, 1);
				flow = createFlow(SharedPtr<Path>(), flowId, entropy, now);
			}
			if (flow) {
				return flow->assignedPath();
			}
		}
	}
	return SharedPtr<Path>();
}

void Bond::recordIncomingInvalidPacket(const SharedPtr<Path>& path)
{
	// char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "recordIncomingInvalidPacket() %s %s\n", getSlave(path)->ifname().c_str(), pathStr);
	Mutex::Lock _l(_paths_m);
	for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i] == path) {
			_paths[i]->packetValiditySamples.push(false);
		}
	}
}

void Bond::recordOutgoingPacket(const SharedPtr<Path> &path, const uint64_t packetId,
	uint16_t payloadLength, const Packet::Verb verb, const int32_t flowId, int64_t now)
{
	// char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "recordOutgoingPacket() %s %s, packetId=%llx, payloadLength=%d, verb=%x, flowId=%lx\n", getSlave(path)->ifname().c_str(), pathStr, packetId, payloadLength, verb, flowId);
	_freeRandomByte += (unsigned char)(packetId >> 8); // Grab entropy to use in path selection logic
	if (!_shouldCollectPathStatistics) {
		return;
	}
	bool isFrame = (verb == Packet::VERB_FRAME || verb == Packet::VERB_EXT_FRAME);
	bool shouldRecord = (packetId & (ZT_QOS_ACK_DIVISOR - 1)
		&& (verb != Packet::VERB_ACK)
		&& (verb != Packet::VERB_QOS_MEASUREMENT));
	if (isFrame || shouldRecord) {
		Mutex::Lock _l(_paths_m);
		if (isFrame) {
			++(path->_packetsOut);
			_lastFrame=now;
		}
		if (shouldRecord) {
			path->_unackedBytes += payloadLength;
			// Take note that we're expecting a VERB_ACK on this path as of a specific time
			if (path->qosStatsOut.size() < ZT_QOS_MAX_OUTSTANDING_RECORDS) {
				path->qosStatsOut[packetId] = now;
			}
		}
	}
	if (_allowFlowHashing) {
		if (_allowFlowHashing && (flowId != ZT_QOS_NO_FLOW)) {
			Mutex::Lock _l(_flows_m);
			if (_flows.count(flowId)) {
				_flows[flowId]->recordOutgoingBytes(payloadLength);
			}
		}
	}
}

void Bond::recordIncomingPacket(const SharedPtr<Path>& path, uint64_t packetId, uint16_t payloadLength,
	Packet::Verb verb, int32_t flowId, int64_t now)
{
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "recordIncomingPacket() %s %s, packetId=%llx, payloadLength=%d, verb=%x, flowId=%lx\n", getSlave(path)->ifname().c_str(), pathStr, packetId, payloadLength, verb, flowId);
	bool isFrame = (verb == Packet::VERB_FRAME || verb == Packet::VERB_EXT_FRAME);
	bool shouldRecord = (packetId & (ZT_QOS_ACK_DIVISOR - 1)
		&& (verb != Packet::VERB_ACK)
		&& (verb != Packet::VERB_QOS_MEASUREMENT));
	if (isFrame || shouldRecord) {
		Mutex::Lock _l(_paths_m);
		if (isFrame) {
			++(path->_packetsIn);
			_lastFrame=now;
		}
		if (shouldRecord) {
			path->ackStatsIn[packetId] = payloadLength;
			++(path->_packetsReceivedSinceLastAck);
			path->qosStatsIn[packetId] = now;
			++(path->_packetsReceivedSinceLastQoS);
			path->packetValiditySamples.push(true);
		}
	}
	/**
	 * Learn new flows and pro-actively create entries for them in the bond so
	 * that the next time we send a packet out that is part of a flow we know
	 * which path to use.
	 */
	if ((flowId != ZT_QOS_NO_FLOW)
		&& (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_RR
			|| _bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR
			|| _bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE)) {
		Mutex::Lock _l(_flows_m);
		SharedPtr<Flow> flow;
		if (!_flows.count(flowId)) {
			flow = createFlow(path, flowId, 0, now);
		} else {
			flow = _flows[flowId];
		}
		if (flow) {
			flow->recordIncomingBytes(payloadLength);
		}
	}
}

void Bond::receivedQoS(const SharedPtr<Path>& path, int64_t now, int count, uint64_t *rx_id, uint16_t *rx_ts)
{
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "receivedQoS() %s %s\n", getSlave(path)->ifname().c_str(), pathStr);
	Mutex::Lock _l(_paths_m);
	// Look up egress times and compute latency values for each record
	std::map<uint64_t,uint64_t>::iterator it;
	for (int j=0; j<count; j++) {
		it = path->qosStatsOut.find(rx_id[j]);
		if (it != path->qosStatsOut.end()) {
			path->latencySamples.push(((uint16_t)(now - it->second) - rx_ts[j]) / 2);
			path->qosStatsOut.erase(it);
		}
	}
	path->qosRecordSize.push(count);
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "receivedQoS() on path %s %s, count=%d, successful=%d, qosStatsOut.size()=%d\n", getSlave(path)->ifname().c_str(), pathStr, count, path->aknowledgedQoSRecordCountSinceLastCheck, path->qosStatsOut.size());
}

void Bond::receivedAck(const SharedPtr<Path>& path, int64_t now, int32_t ackedBytes)
{
	Mutex::Lock _l(_paths_m);
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "receivedAck() %s %s, (ackedBytes=%d, lastAckReceived=%lld, ackAge=%lld)\n", getSlave(path)->ifname().c_str(), pathStr, ackedBytes, path->lastAckReceived, path->ackAge(now));
	path->_lastAckReceived = now;
	path->_unackedBytes = (ackedBytes > path->_unackedBytes) ? 0 : path->_unackedBytes - ackedBytes;
	int64_t timeSinceThroughputEstimate = (now - path->_lastThroughputEstimation);
	if (timeSinceThroughputEstimate >= throughputMeasurementInterval) {
		// TODO: See if this floating point math can be reduced
		uint64_t throughput = (uint64_t)((float)(path->_bytesAckedSinceLastThroughputEstimation) / ((float)timeSinceThroughputEstimate / (float)1000));
		throughput /= 1000;
		if (throughput > 0.0) {
			path->throughputSamples.push(throughput);
			path->_throughputMax = throughput > path->_throughputMax ? throughput : path->_throughputMax;
		}
		path->_lastThroughputEstimation = now;
		path->_bytesAckedSinceLastThroughputEstimation = 0;
	} else {
		path->_bytesAckedSinceLastThroughputEstimation += ackedBytes;
	}
}

int32_t Bond::generateQoSPacket(const SharedPtr<Path>& path, int64_t now, char *qosBuffer)
{
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "generateQoSPacket() %s %s\n", getSlave(path)->ifname().c_str(), pathStr);
	int32_t len = 0;
	std::map<uint64_t,uint64_t>::iterator it = path->qosStatsIn.begin();
	int i=0;
	int numRecords = std::min(path->_packetsReceivedSinceLastQoS,ZT_QOS_TABLE_SIZE);
	while (i<numRecords && it != path->qosStatsIn.end()) {
		uint64_t id = it->first;
		memcpy(qosBuffer, &id, sizeof(uint64_t));
		qosBuffer+=sizeof(uint64_t);
		uint16_t holdingTime = (uint16_t)(now - it->second);
		memcpy(qosBuffer, &holdingTime, sizeof(uint16_t));
		qosBuffer+=sizeof(uint16_t);
		len+=sizeof(uint64_t)+sizeof(uint16_t);
		path->qosStatsIn.erase(it++);
		++i;
	}
	return len;
}

bool Bond::assignFlowToBondedPath(SharedPtr<Flow> &flow, int64_t now)
{
	//fprintf(stderr, "assignFlowToBondedPath\n");
	char curPathStr[128];
	unsigned int idx = ZT_MAX_PEER_NETWORK_PATHS;
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR) {
		idx = abs((int)(flow->id() % (_numBondedPaths)));
		//fprintf(stderr, "flow->id()=%d, %x, _numBondedPaths=%d, idx=%d\n", flow->id(), flow->id(), _numBondedPaths, idx);
		flow->assignPath(_paths[_bondedIdx[idx]],now);
		++(_paths[_bondedIdx[idx]]->_assignedFlowCount);
	}
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
		unsigned char entropy;
		Utils::getSecureRandom(&entropy, 1);
		if (_totalBondUnderload) {
			entropy %= _totalBondUnderload;
		}
		if (!_numBondedPaths) {
			fprintf(stderr, "no bonded paths for flow assignment\n");
			return false;
		}
		/* Since there may be scenarios where a path is removed before we can re-estimate
		relative qualities (and thus allocations) we need to down-modulate the entropy
		value that we use to randomly assign among the surviving paths, otherwise we risk
		not being able to find a path to assign this flow to. */
		int totalIncompleteAllocation = 0;
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i] && _paths[i]->bonded()) {
				totalIncompleteAllocation += _paths[i]->_allocation;
			}
		}
		//fprintf(stderr, "entropy     = %d, totalIncompleteAllocation=%d\n", entropy, totalIncompleteAllocation);
		entropy %= totalIncompleteAllocation;
		//fprintf(stderr, "new entropy = %d\n", entropy);
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i] && _paths[i]->bonded()) {
				SharedPtr<Slave> slave = RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
				_paths[i]->address().toString(curPathStr);
				uint8_t probabilitySegment = (_totalBondUnderload > 0) ? _paths[i]->_affinity : _paths[i]->_allocation;
				//fprintf(stderr, "i=%2d, entropy=%3d, alloc=%3d, byteload=%4d, segment=%3d, _totalBondUnderload=%3d, ifname=%s, path=%20s\n", i, entropy, _paths[i]->_allocation, _paths[i]->_relativeByteLoad, probabilitySegment, _totalBondUnderload, slave->ifname().c_str(), curPathStr);
				if (entropy <= probabilitySegment) {
					idx = i;
					//fprintf(stderr, "\t is best path\n");
					break;
				}
				entropy -= probabilitySegment;
			}
		}
		if (idx < ZT_MAX_PEER_NETWORK_PATHS) {
			if (flow->_assignedPath) {
				flow->_previouslyAssignedPath = flow->_assignedPath;
			}
			flow->assignPath(_paths[idx],now);
			++(_paths[idx]->_assignedFlowCount);
		}
		else {
			fprintf(stderr, "could not assign flow?\n"); exit(0); // TODO: Remove for production
			return false;
		}
	}
	flow->assignedPath()->address().toString(curPathStr);
	SharedPtr<Slave> slave = RR->bc->getSlaveBySocket(_policyAlias, flow->assignedPath()->localSocket());
	fprintf(stderr, "assigned (tx) flow %x with peer %llx to path %s on %s (idx=%d)\n", flow->id(), _peer->_id.address().toInt(), curPathStr, slave->ifname().c_str(), idx);
	return true;
}

SharedPtr<Flow> Bond::createFlow(const SharedPtr<Path> &path, int32_t flowId, unsigned char entropy, int64_t now)
{
	//fprintf(stderr, "createFlow\n");
	char curPathStr[128];
	// ---
	if (!_numBondedPaths) {
		fprintf(stderr, "there are no bonded paths, cannot assign flow\n");
		return SharedPtr<Flow>();
	}
	if (_flows.size() >= ZT_FLOW_MAX_COUNT) {
		fprintf(stderr, "max number of flows reached (%d), forcibly forgetting oldest flow\n", ZT_FLOW_MAX_COUNT);
		forgetFlowsWhenNecessary(0,true,now);
	}
	SharedPtr<Flow> flow = new Flow(flowId, now);
	_flows[flowId] = flow;
	fprintf(stderr, "new flow %x detected with peer %llx, %lu active flow(s)\n", flowId, _peer->_id.address().toInt(), (_flows.size()));
	/**
	 * Add a flow with a given Path already provided. This is the case when a packet
	 * is received on a path but no flow exists, in this case we simply assign the path
	 * that the remote peer chose for us.
	 */
	if (path) {
		flow->assignPath(path,now);
		path->address().toString(curPathStr);
		path->_assignedFlowCount++;
		SharedPtr<Slave> slave = RR->bc->getSlaveBySocket(_policyAlias, flow->assignedPath()->localSocket());
		fprintf(stderr, "assigned (rx) flow %x with peer %llx to path %s on %s\n", flow->id(), _peer->_id.address().toInt(), curPathStr, slave->ifname().c_str());
	}
	/**
	 * Add a flow when no path was provided. This means that it is an outgoing packet
	 * and that it is up to the local peer to decide how to load-balance its transmission.
	 */
	else if (!path) {
		assignFlowToBondedPath(flow, now);
	}
	return flow;
}

void Bond::forgetFlowsWhenNecessary(uint64_t age, bool oldest, int64_t now)
{
	//fprintf(stderr, "forgetFlowsWhenNecessary\n");
	std::map<int32_t,SharedPtr<Flow> >::iterator it = _flows.begin();
	std::map<int32_t,SharedPtr<Flow> >::iterator oldestFlow = _flows.end();
	SharedPtr<Flow> expiredFlow;
	if (age) { // Remove by specific age
		while (it != _flows.end()) {
			if (it->second->age(now) > age) {
				fprintf(stderr, "forgetting flow %x between this node and %llx, %lu active flow(s)\n", it->first, _peer->_id.address().toInt(), (_flows.size()-1));
				it->second->assignedPath()->_assignedFlowCount--;
				it = _flows.erase(it);
			} else {
				++it;
			}
		}
	}
	else if (oldest) { // Remove single oldest by natural expiration
		uint64_t maxAge = 0;
		while (it != _flows.end()) {
			if (it->second->age(now) > maxAge) {
				maxAge = (now - it->second->age(now));
				oldestFlow = it;
			}
			++it;
		}
		if (oldestFlow != _flows.end()) {
			fprintf(stderr, "forgetting oldest flow %x (of age %llu) between this node and %llx, %lu active flow(s)\n", oldestFlow->first, oldestFlow->second->age(now), _peer->_id.address().toInt(), (_flows.size()-1));
			oldestFlow->second->assignedPath()->_assignedFlowCount--;
			_flows.erase(oldestFlow);
		}
	}
}

void Bond::processIncomingPathNegotiationRequest(uint64_t now, SharedPtr<Path> &path, int16_t remoteUtility)
{
	//fprintf(stderr, "processIncomingPathNegotiationRequest\n");
	if (_abSlaveSelectMethod != ZT_MULTIPATH_RESELECTION_POLICY_OPTIMIZE) {
		return;
	}
	Mutex::Lock _l(_paths_m);
	char pathStr[128];
	path->address().toString(pathStr);
	if (!_lastPathNegotiationCheck) {
		return;
	}
	SharedPtr<Slave> slave = RR->bc->getSlaveBySocket(_policyAlias, path->localSocket());
	if (remoteUtility > _localUtility) {
		fprintf(stderr, "peer suggests path, its utility (%d) is greater than ours (%d), we will switch to %s on %s (ls=%llx)\n", remoteUtility, _localUtility, pathStr, slave->ifname().c_str(), path->localSocket());
		negotiatedPath = path;
	}
	if (remoteUtility < _localUtility) {
		fprintf(stderr, "peer suggests path, its utility (%d) is less than ours (%d), we will NOT switch to %s on %s (ls=%llx)\n", remoteUtility, _localUtility, pathStr, slave->ifname().c_str(), path->localSocket());
	}
	if (remoteUtility == _localUtility) {
		fprintf(stderr, "peer suggest path, but utility is equal, picking choice made by peer with greater identity.\n");
		if (_peer->_id.address().toInt() > RR->node->identity().address().toInt()) {
			fprintf(stderr, "peer identity was greater, going with their choice of %s on %s (ls=%llx)\n", pathStr, slave->ifname().c_str(), path->localSocket());
			negotiatedPath = path;
		} else {
			fprintf(stderr, "our identity was greater, no change\n");
		}
	}
}

void Bond::pathNegotiationCheck(void *tPtr, const int64_t now)
{
	//fprintf(stderr, "pathNegotiationCheck\n");
	char pathStr[128];
	int maxInPathIdx = ZT_MAX_PEER_NETWORK_PATHS;
	int maxOutPathIdx = ZT_MAX_PEER_NETWORK_PATHS;
	uint64_t maxInCount = 0;
	uint64_t maxOutCount = 0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (!_paths[i]) {
			continue;
		}
		if (_paths[i]->_packetsIn > maxInCount) {
			maxInCount = _paths[i]->_packetsIn;
			maxInPathIdx = i;
		}
		if (_paths[i]->_packetsOut > maxOutCount) {
			maxOutCount = _paths[i]->_packetsOut;
			maxOutPathIdx = i;
		}
		_paths[i]->resetPacketCounts();
	}
	bool _peerLinksSynchronized = ((maxInPathIdx != ZT_MAX_PEER_NETWORK_PATHS)
		&& (maxOutPathIdx != ZT_MAX_PEER_NETWORK_PATHS)
		&& (maxInPathIdx != maxOutPathIdx)) ? false : true;
	/**
	 * Determine utility and attempt to petition remote peer to switch to our chosen path
	 */
	if (!_peerLinksSynchronized) {
		_localUtility = _paths[maxOutPathIdx]->_failoverScore - _paths[maxInPathIdx]->_failoverScore;
		if (_paths[maxOutPathIdx]->_negotiated) {
			_localUtility -= ZT_MULTIPATH_FAILOVER_HANDICAP_NEGOTIATED;
		}
		if ((now - _lastSentPathNegotiationRequest) > ZT_PATH_NEGOTIATION_CUTOFF_TIME) {
			fprintf(stderr, "BT: (sync) it's been long enough, sending more requests.\n");
			_numSentPathNegotiationRequests = 0;
		}
		if (_numSentPathNegotiationRequests < ZT_PATH_NEGOTIATION_TRY_COUNT) {
			if (_localUtility >= 0) {
				fprintf(stderr, "BT: (sync) paths appear to be out of sync (utility=%d)\n", _localUtility);
				sendPATH_NEGOTIATION_REQUEST(tPtr, _paths[maxOutPathIdx]);
				++_numSentPathNegotiationRequests;
				_lastSentPathNegotiationRequest = now;
				_paths[maxOutPathIdx]->address().toString(pathStr);
				SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[maxOutPathIdx]->localSocket());
				fprintf(stderr, "sending request to use %s on %s, ls=%llx, utility=%d\n", pathStr, slave->ifname().c_str(), _paths[maxOutPathIdx]->localSocket(), _localUtility);
			}
		}
		/**
		 * Give up negotiating and consider switching
		 */
		else if ((now - _lastSentPathNegotiationRequest) > (2 * ZT_PATH_NEGOTIATION_CHECK_INTERVAL)) {
			if (_localUtility == 0) {
				// There's no loss to us, just switch without sending a another request
				fprintf(stderr, "BT: (sync) giving up, switching to remote peer's path.\n");
				negotiatedPath = _paths[maxInPathIdx];
			}
		}
	}
}

void Bond::sendPATH_NEGOTIATION_REQUEST(void *tPtr, const SharedPtr<Path> &path)
{
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "sendPATH_NEGOTIATION_REQUEST() %s %s\n", getSlave(path)->ifname().c_str(), pathStr);
	if (_abSlaveSelectMethod != ZT_MULTIPATH_RESELECTION_POLICY_OPTIMIZE) {
		return;
	}
	Packet outp(_peer->_id.address(),RR->identity.address(),Packet::VERB_PATH_NEGOTIATION_REQUEST);
	outp.append<int16_t>(_localUtility);
	if (path->address()) {
		outp.armor(_peer->key(),false);
		RR->node->putPacket(tPtr,path->localSocket(),path->address(),outp.data(),outp.size());
	}
}

void Bond::sendACK(void *tPtr,const SharedPtr<Path> &path,const int64_t localSocket,
	const InetAddress &atAddress,int64_t now)
{
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "sendACK() %s %s\n", getSlave(path)->ifname().c_str(), pathStr);
	Packet outp(_peer->_id.address(),RR->identity.address(),Packet::VERB_ACK);
	int32_t bytesToAck = 0;
	std::map<uint64_t,uint16_t>::iterator it = path->ackStatsIn.begin();
	while (it != path->ackStatsIn.end()) {
		bytesToAck += it->second;
		++it;
	}
	outp.append<uint32_t>(bytesToAck);
	if (atAddress) {
		outp.armor(_peer->key(),false);
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false);
	}
	path->ackStatsIn.clear();
	path->_packetsReceivedSinceLastAck = 0;
	path->_lastAckSent = now;
}

void Bond::sendQOS_MEASUREMENT(void *tPtr,const SharedPtr<Path> &path,const int64_t localSocket,
	const InetAddress &atAddress,int64_t now)
{
	//char pathStr[128];path->address().toString(pathStr);fprintf(stderr, "sendQOS() %s %s\n", getSlave(path)->ifname().c_str(), pathStr);
	const int64_t _now = RR->node->now();
	Packet outp(_peer->_id.address(),RR->identity.address(),Packet::VERB_QOS_MEASUREMENT);
	char qosData[ZT_QOS_MAX_PACKET_SIZE];
	int16_t len = generateQoSPacket(path, _now,qosData);
	outp.append(qosData,len);
	if (atAddress) {
		outp.armor(_peer->key(),false);
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false);
	}
	// Account for the fact that a VERB_QOS_MEASUREMENT was just sent. Reset timers.
	path->_packetsReceivedSinceLastQoS = 0;
	path->_lastQoSMeasurement = now;
}

void Bond::processBackgroundTasks(void *tPtr, const int64_t now)
{
	Mutex::Lock _l(_paths_m);
	if (!_peer->_canUseMultipath || (now - _lastBackgroundTaskCheck) < ZT_BOND_BACKGROUND_TASK_MIN_INTERVAL) {
		return;
	}
	_lastBackgroundTaskCheck = now;

	// Compute dynamic path monitor timer interval
	if (_slaveMonitorStrategy == ZT_MULTIPATH_SLAVE_MONITOR_STRATEGY_DYNAMIC) {
		int suggestedMonitorInterval  = (now - _lastFrame) / 100;
		_dynamicPathMonitorInterval = std::min(ZT_PATH_HEARTBEAT_PERIOD, ((suggestedMonitorInterval > _bondMonitorInterval) ? suggestedMonitorInterval : _bondMonitorInterval));
		//fprintf(stderr, "_lastFrame=%llu, suggestedMonitorInterval=%d, _dynamicPathMonitorInterval=%d\n",
		//	(now-_lastFrame), suggestedMonitorInterval, _dynamicPathMonitorInterval);
	}
	// TODO: Clarify and generalize this logic
	if (_slaveMonitorStrategy == ZT_MULTIPATH_SLAVE_MONITOR_STRATEGY_DYNAMIC) {
		_shouldCollectPathStatistics = true;
	}

	// Memoize oft-used properties in the packet ingress/egress logic path
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
		// Required for real-time balancing
		_shouldCollectPathStatistics = true;
	}
	if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP) {
		if (_abSlaveSelectMethod == ZT_MULTIPATH_RESELECTION_POLICY_BETTER) {
			// Required for judging suitability of primary slave after recovery
			_shouldCollectPathStatistics = true;
		}
		if (_abSlaveSelectMethod == ZT_MULTIPATH_RESELECTION_POLICY_OPTIMIZE) {
			// Required for judging suitability of new candidate primary
			_shouldCollectPathStatistics = true;
		}
	}
	if ((now - _lastCheckUserPreferences) > 1000) {
		_lastCheckUserPreferences = now;
		applyUserPrefs();
	}

	curateBond(now,false);
	if ((now - _lastQualityEstimation) > _qualityEstimationInterval) {
		_lastQualityEstimation = now;
		estimatePathQuality(now);
	}
	dumpInfo(now);

	// Send QOS/ACK packets as needed
	if (_shouldCollectPathStatistics) {
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i] && _paths[i]->allowed()) {
				if (_paths[i]->needsToSendQoS(now,_qosSendInterval)) {
					sendQOS_MEASUREMENT(tPtr, _paths[i], _paths[i]->localSocket(), _paths[i]->address(), now);
				}
				if (_paths[i]->needsToSendAck(now,_ackSendInterval)) {
					sendACK(tPtr, _paths[i], _paths[i]->localSocket(), _paths[i]->address(), now);
				}
			}
		}
	}
	// Perform periodic background tasks unique to each bonding policy
	switch (_bondingPolicy)
	{
		case ZT_BONDING_POLICY_ACTIVE_BACKUP:
			processActiveBackupTasks(now);
			break;
		case ZT_BONDING_POLICY_BROADCAST:
			break;
		case ZT_BONDING_POLICY_BALANCE_RR:
		case ZT_BONDING_POLICY_BALANCE_XOR:
		case ZT_BONDING_POLICY_BALANCE_AWARE:
			processBalanceTasks(now);
			break;
		default:
			break;
	}
	// Check whether or not a path negotiation needs to be performed
	if (((now - _lastPathNegotiationCheck) > ZT_PATH_NEGOTIATION_CHECK_INTERVAL) && _allowPathNegotiation) {
		_lastPathNegotiationCheck = now;
		pathNegotiationCheck(tPtr, now);
	}
}

void Bond::applyUserPrefs()
{
	//fprintf(stderr, "applyUserPrefs, _minReqPathMonitorInterval=%d\n", RR->bc->minReqPathMonitorInterval());
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (!_paths[i]) {
			continue;
		}
		SharedPtr<Slave> sl = getSlave(_paths[i]);
		if (sl) {
			if (sl->monitorInterval() == 0) { // If no interval was specified for this slave, use more generic bond-wide interval
				sl->setMonitorInterval(_bondMonitorInterval);
			}
			RR->bc->setMinReqPathMonitorInterval((sl->monitorInterval() < RR->bc->minReqPathMonitorInterval()) ? sl->monitorInterval() : RR->bc->minReqPathMonitorInterval());
			bool bFoundCommonSlave = false;
			SharedPtr<Slave> commonSlave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
			for(unsigned int j=0;j<ZT_MAX_PEER_NETWORK_PATHS;++j) {
				if (_paths[j] && _paths[j].ptr() != _paths[i].ptr()) {
					if (RR->bc->getSlaveBySocket(_policyAlias, _paths[j]->localSocket()) == commonSlave) {
						bFoundCommonSlave = true;
					}
				}
			}
			_paths[i]->_monitorInterval = sl->monitorInterval();
			_paths[i]->_upDelay = sl->upDelay() ? sl->upDelay() : _upDelay;
			_paths[i]->_downDelay = sl->downDelay() ? sl->downDelay() : _downDelay;
			_paths[i]->_ipvPref = sl->ipvPref();
			_paths[i]->_mode = sl->mode();
			_paths[i]->_enabled = sl->enabled();
			_paths[i]->_onlyPathOnSlave = !bFoundCommonSlave;
		}
	}
	if (_peer) {
		_peer->_shouldCollectPathStatistics = _shouldCollectPathStatistics;
		_peer->_bondingPolicy = _bondingPolicy;
	}
}

void Bond::curateBond(const int64_t now, bool rebuildBond)
{
	//fprintf(stderr, "%lu curateBond (rebuildBond=%d), _numBondedPaths=%d\n", ((now - RR->bc->getBondStartTime())), rebuildBond, _numBondedPaths);
	char pathStr[128];
	/**
	 * Update path states
	 */
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (!_paths[i]) {
			continue;
		}
		bool currEligibility = _paths[i]->eligible(now,_ackSendInterval);
		//_paths[i]->address().toString(pathStr);
		//fprintf(stderr, "\n\n%ld path eligibility (for %s, %s):\n", (RR->node->now() - RR->bc->getBondStartTime()), getSlave(_paths[i])->ifname().c_str(), pathStr);
		//_paths[i]->printEligible(now,_ackSendInterval);
		if (currEligibility != _paths[i]->_lastEligibilityState) {
			_paths[i]->address().toString(pathStr);
			//fprintf(stderr, "\n\n%ld path eligibility (for %s, %s) has changed (from %d to %d)\n", (RR->node->now() - RR->bc->getBondStartTime()), getSlave(_paths[i])->ifname().c_str(), pathStr, _paths[i]->lastCheckedEligibility, _paths[i]->eligible(now,_ackSendInterval));
			if (currEligibility) {
				rebuildBond = true;
			}
			if (!currEligibility) {
				_paths[i]->adjustRefractoryPeriod(now, _defaultPathRefractoryPeriod, !currEligibility);
				if (_paths[i]->bonded()) {
					//fprintf(stderr, "the path was bonded, reallocation of its flows will occur soon\n");
					rebuildBond = true;
					_paths[i]->_shouldReallocateFlows = _paths[i]->bonded();
					_paths[i]->setBonded(false);
				} else {
					//fprintf(stderr, "the path was not bonded, no consequences\n");
				}
			}
		}
		if (currEligibility) {
			_paths[i]->adjustRefractoryPeriod(now, _defaultPathRefractoryPeriod, false);
		}
		_paths[i]->_lastEligibilityState = currEligibility;
	}
	/**
	 * Curate the set of paths that are part of the bond proper. Selects a single path
	 * per logical slave according to eligibility and user-specified constraints.
	 */
	if ((_bondingPolicy == ZT_BONDING_POLICY_BALANCE_RR)
			|| (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR)
			|| (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE)) {
		if (!_numBondedPaths) {
			rebuildBond = true;
		}
		// TODO: Optimize
		if (rebuildBond) {
			int updatedBondedPathCount = 0;
			std::map<SharedPtr<Slave>,int> slaveMap;
			for (int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (_paths[i] && _paths[i]->allowed() && (_paths[i]->eligible(now,_ackSendInterval) || !_numBondedPaths)) {
					SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
					if (!slaveMap.count(slave)) {
						slaveMap[slave] = i;
					}
					else {
						bool overriden = false;
						_paths[i]->address().toString(pathStr);
						//fprintf(stderr, " slave representative path already exists! (%s %s)\n", getSlave(_paths[i])->ifname().c_str(), pathStr);
						if (_paths[i]->preferred() && !_paths[slaveMap[slave]]->preferred()) {
							// Override previous choice if preferred
							//fprintf(stderr, "overriding since its preferred!\n");
							if (_paths[slaveMap[slave]]->_assignedFlowCount) {
								_paths[slaveMap[slave]]->_deprecated = true;
							}
							else {
								_paths[slaveMap[slave]]->_deprecated = true;
								_paths[slaveMap[slave]]->setBonded(false);
							}
							slaveMap[slave] = i;
							overriden = true;
						}
						if ((_paths[i]->preferred() && _paths[slaveMap[slave]]->preferred())
							|| (!_paths[i]->preferred() && !_paths[slaveMap[slave]]->preferred())) {
							if (_paths[i]->preferenceRank() > _paths[slaveMap[slave]]->preferenceRank()) {
								// Override if higher preference
								//fprintf(stderr, "overriding according to preference preferenceRank!\n");
								if (_paths[slaveMap[slave]]->_assignedFlowCount) {
									_paths[slaveMap[slave]]->_deprecated = true;
								}
								else {
									_paths[slaveMap[slave]]->_deprecated = true;
									_paths[slaveMap[slave]]->setBonded(false);
								}
								slaveMap[slave] = i;
							}
						}
					}
				}
			}
			std::map<SharedPtr<Slave>,int>::iterator it = slaveMap.begin();
			for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (!_paths[i]) {
					continue;
				}
				_bondedIdx[i] = ZT_MAX_PEER_NETWORK_PATHS;
				if (it != slaveMap.end()) {
					_bondedIdx[i] = it->second;
					_paths[_bondedIdx[i]]->setBonded(true);
					++it;
					++updatedBondedPathCount;
					_paths[_bondedIdx[i]]->address().toString(pathStr);
					//fprintf(stderr, "setting i=%d, _bondedIdx[%d]=%d to bonded (%s %s)\n", i, i, _bondedIdx[i], getSlave(_paths[_bondedIdx[i]])->ifname().c_str(), pathStr);
				}
			}
			_numBondedPaths = updatedBondedPathCount;

			if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_RR) {
				// Cause a RR reset since the currently used index might no longer be valid
				_rrPacketsSentOnCurrSlave = _packetsPerSlave;
			}
		}
	}
}

void Bond::estimatePathQuality(const int64_t now)
{
	char pathStr[128];
	uint32_t totUserSpecifiedSlaveSpeed = 0;
	if (_numBondedPaths) { // Compute relative user-specified speeds of slaves
		for(unsigned int i=0;i<_numBondedPaths;++i) {
			SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
			if (_paths[i] && _paths[i]->allowed()) {
				totUserSpecifiedSlaveSpeed += slave->speed();
			}
		}
		for(unsigned int i=0;i<_numBondedPaths;++i) {
				SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
			if (_paths[i] && _paths[i]->allowed()) {
				slave->setRelativeSpeed(round( ((float)slave->speed() / (float)totUserSpecifiedSlaveSpeed) * 255));
			}
		}
	}

	float lat[ZT_MAX_PEER_NETWORK_PATHS];
	float pdv[ZT_MAX_PEER_NETWORK_PATHS];
	float plr[ZT_MAX_PEER_NETWORK_PATHS];
	float per[ZT_MAX_PEER_NETWORK_PATHS];

	float maxLAT = 0;
	float maxPDV = 0;
	float maxPLR = 0;
	float maxPER = 0;

	float quality[ZT_MAX_PEER_NETWORK_PATHS];
	uint8_t alloc[ZT_MAX_PEER_NETWORK_PATHS];

	float totQuality = 0.0f;

	memset(&lat, 0, sizeof(lat));
	memset(&pdv, 0, sizeof(pdv));
	memset(&plr, 0, sizeof(plr));
	memset(&per, 0, sizeof(per));
	memset(&quality, 0, sizeof(quality));
	memset(&alloc, 0, sizeof(alloc));

	// Compute initial summary statistics
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (!_paths[i] || !_paths[i]->allowed()) {
			continue;
		}
		// Compute/Smooth average of real-world observations
		_paths[i]->_latencyMean = _paths[i]->latencySamples.mean();
		_paths[i]->_latencyVariance = _paths[i]->latencySamples.stddev();
		_paths[i]->_packetErrorRatio = 1.0 - (_paths[i]->packetValiditySamples.count() ? _paths[i]->packetValiditySamples.mean() : 1.0);

		if (userHasSpecifiedSlaveSpeeds()) {
			// Use user-reported metrics
			SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
			if (slave) {
				_paths[i]->_throughputMean = slave->speed();
				_paths[i]->_throughputVariance = 0;
			}
		}
		// Drain unacknowledged QoS records
		std::map<uint64_t,uint64_t>::iterator it = _paths[i]->qosStatsOut.begin();
		uint64_t currentLostRecords = 0;
		while (it != _paths[i]->qosStatsOut.end()) {
			int qosRecordTimeout = 5000; //_paths[i]->monitorInterval() * ZT_MULTIPATH_QOS_ACK_INTERVAL_MULTIPLIER * 8;
			if ((now - it->second) >= qosRecordTimeout) {
				//fprintf(stderr, "packetId=%llx was lost\n", it->first);
				it = _paths[i]->qosStatsOut.erase(it);
				++currentLostRecords;
			} else { ++it; }
		}

		quality[i]=0;
		totQuality=0;
		// Normalize raw observations according to sane limits and/or user specified values
		lat[i] = 1.0 / expf(4*Utils::normalize(_paths[i]->_latencyMean,      0, _maxAcceptableLatency,             0, 1));
		pdv[i] = 1.0 / expf(4*Utils::normalize(_paths[i]->_latencyVariance,  0, _maxAcceptablePacketDelayVariance, 0, 1));
		plr[i] = 1.0 / expf(4*Utils::normalize(_paths[i]->_packetLossRatio,  0, _maxAcceptablePacketLossRatio,     0, 1));
		per[i] = 1.0 / expf(4*Utils::normalize(_paths[i]->_packetErrorRatio, 0, _maxAcceptablePacketErrorRatio,    0, 1));
		//scp[i] = _paths[i]->ipvPref != 0 ? 1.0 : Utils::normalize(_paths[i]->ipScope(), InetAddress::IP_SCOPE_NONE, InetAddress::IP_SCOPE_PRIVATE, 0, 1);
		// Record bond-wide maximums to determine relative values
		maxLAT = lat[i] > maxLAT ? lat[i] : maxLAT;
		maxPDV = pdv[i] > maxPDV ? pdv[i] : maxPDV;
		maxPLR = plr[i] > maxPLR ? plr[i] : maxPLR;
		maxPER = per[i] > maxPER ? per[i] : maxPER;
		//fprintf(stdout, "EH   %d: lat=%8.3f,  ltm=%8.3f,  pdv=%8.3f,  plr=%5.3f,  per=%5.3f,  thr=%8f,  thm=%5.3f,  thv=%5.3f,  avl=%5.3f,  age=%8.2f,  scp=%4d,  q=%5.3f,  qtot=%5.3f,  ac=%d if=%s, path=%s\n",
		//	              i,   lat[i],     ltm[i],     pdv[i],     plr[i],     per[i],     thr[i],     thm[i],     thv[i],     avl[i],     age[i],     scp[i], quality[i], totQuality, alloc[i], getSlave(_paths[i])->ifname().c_str(), pathStr);

	}
	// Convert metrics to relative quantities and apply contribution weights
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i] && _paths[i]->bonded()) {
			quality[i] += ((maxLAT > 0.0f ? lat[i] / maxLAT : 0.0f) * _qualityWeights[ZT_QOS_LAT_IDX]);
			quality[i] += ((maxPDV > 0.0f ? pdv[i] / maxPDV : 0.0f) * _qualityWeights[ZT_QOS_PDV_IDX]);
			quality[i] += ((maxPLR > 0.0f ? plr[i] / maxPLR : 0.0f) * _qualityWeights[ZT_QOS_PLR_IDX]);
			quality[i] += ((maxPER > 0.0f ? per[i] / maxPER : 0.0f) * _qualityWeights[ZT_QOS_PER_IDX]);
			//quality[i] += (scp[i] * _qualityWeights[ZT_QOS_SCP_IDX]);
			totQuality += quality[i];
		}
	}
	//
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i] && _paths[i]->bonded()) {
			alloc[i] = std::ceil((quality[i] / totQuality) * (float)255);
			_paths[i]->_allocation = alloc[i];
		}
	}
	if ((now - _lastLogTS) > 500) {
		if (!relevant()) {return;}
		//fprintf(stderr, "\n");
		_lastLogTS = now;
		int numPlottablePaths=0;
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i]) {
				++numPlottablePaths;
				_paths[i]->address().toString(pathStr);
				//fprintf(stderr, "%lu FIN [%d/%d]: pmi=%5d, lat=%4.3f, ltm=%4.3f, pdv=%4.3f, plr=%4.3f, per=%4.3f, thr=%4.3f, thm=%4.3f, thv=%4.3f, age=%4.3f, scp=%4d, q=%4.3f, qtot=%4.3f, ac=%4d, asf=%3d, if=%s, path=%20s, bond=%d, qosout=%d, plrraw=%d\n",
				//	((now - RR->bc->getBondStartTime())), i, _numBondedPaths,   _paths[i]->monitorInterval,
				//	lat[i],     ltm[i],     pdv[i],     plr[i],     per[i],     thr[i],     thm[i],     thv[i],     age[i],     scp[i],
				//	quality[i], totQuality, alloc[i], _paths[i]->assignedFlowCount, getSlave(_paths[i])->ifname().c_str(), pathStr, _paths[i]->bonded(), _paths[i]->qosStatsOut.size(), _paths[i]->packetLossRatio);
			}
		}
		if (numPlottablePaths < 2) {
			return;
		}
		if (!_header) {
			fprintf(stdout, "now, bonded, relativeUnderload, flows, ");
			for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (_paths[i]) {
					_paths[i]->address().toString(pathStr);
					std::string label = std::string((pathStr)) + " " + getSlave(_paths[i])->ifname();
					for (int i=0; i<19; ++i) {
						fprintf(stdout, "%s, ", label.c_str());
					}
				}
			}
			_header=true;
		}
		/*
		fprintf(stdout, "%ld, %d, %d, %d, ",((now - RR->bc->getBondStartTime())),_numBondedPaths,_totalBondUnderload, _flows.size());
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i]) {
				_paths[i]->address().toString(pathStr);
				fprintf(stdout, "%s, %s, %8.3f, %8.3f, %8.3f, %5.3f, %5.3f, %5.3f, %8f, %5.3f, %5.3f, %d, %5.3f, %d, %d, %d, %d, %d, %d, ",
								  getSlave(_paths[i])->ifname().c_str(), pathStr, _paths[i]->_latencyMean, lat[i],pdv[i], _paths[i]->_packetLossRatio, plr[i],per[i],thr[i],thm[i],thv[i],(now - _paths[i]->lastIn()),quality[i],alloc[i],
								  _paths[i]->_relativeByteLoad, _paths[i]->_assignedFlowCount, _paths[i]->alive(now, true), _paths[i]->eligible(now,_ackSendInterval), _paths[i]->qosStatsOut.size());
			}
		}*/
		//fprintf(stdout, "\n");
	}
}

void Bond::processBalanceTasks(const int64_t now)
{
	char curPathStr[128];

	// TODO: Generalize
	int totalAllocation = 0;
	for (int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (!_paths[i]) {
			continue;
		}
		if (_paths[i] && _paths[i]->bonded() && _paths[i]->eligible(now,_ackSendInterval)) {
			totalAllocation+=_paths[i]->_allocation;
		}
	}
	unsigned char minimumAllocationValue = 0.33 * ((float)totalAllocation / (float)_numBondedPaths);

	if (_allowFlowHashing) {
		/**
		 * Clean up and reset flows if necessary
		 */
		if ((now - _lastFlowExpirationCheck) > ZT_MULTIPATH_FLOW_CHECK_INTERVAL) {
			Mutex::Lock _l(_flows_m);
			forgetFlowsWhenNecessary(ZT_MULTIPATH_FLOW_EXPIRATION_INTERVAL,false,now);
			_lastFlowExpirationCheck = now;
		}
		if ((now - _lastFlowStatReset) > ZT_FLOW_STATS_RESET_INTERVAL) {
			Mutex::Lock _l(_flows_m);
			_lastFlowStatReset = now;
			std::map<int32_t,SharedPtr<Flow> >::iterator it = _flows.begin();
			while (it != _flows.end()) {
				it->second->resetByteCounts();
				++it;
			}
		}
		/**
		 * Re-allocate flows from dead paths
		 */
		if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR || _bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
			Mutex::Lock _l(_flows_m);
			for (int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (!_paths[i]) {
					continue;
				}
				if (!_paths[i]->eligible(now,_ackSendInterval) && _paths[i]->_shouldReallocateFlows) {
					_paths[i]->address().toString(curPathStr);
					fprintf(stderr, "%d reallocating flows from dead path %s on %s\n", (RR->node->now() - RR->bc->getBondStartTime()), curPathStr, getSlave(_paths[i])->ifname().c_str());
					std::map<int32_t,SharedPtr<Flow> >::iterator flow_it = _flows.begin();
					while (flow_it != _flows.end()) {
						if (flow_it->second->assignedPath() == _paths[i]) {
							if(assignFlowToBondedPath(flow_it->second, now)) {
								_paths[i]->_assignedFlowCount--;
							}
						}
						++flow_it;
					}
					_paths[i]->_shouldReallocateFlows = false;
				}
			}
		}
		/**
		 * Re-allocate flows from under-performing
		 * NOTE: This could be part of the above block but was kept separate for clarity.
		 */
		if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR || _bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
			Mutex::Lock _l(_flows_m);
			for (int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (!_paths[i]) {
					continue;
				}
				if (_paths[i] && _paths[i]->bonded() && _paths[i]->eligible(now,_ackSendInterval) && (_paths[i]->_allocation < minimumAllocationValue) && _paths[i]->_assignedFlowCount) {
					_paths[i]->address().toString(curPathStr);
					fprintf(stderr, "%d reallocating flows from under-performing path %s on %s\n", (RR->node->now() - RR->bc->getBondStartTime()), curPathStr, getSlave(_paths[i])->ifname().c_str());
					std::map<int32_t,SharedPtr<Flow> >::iterator flow_it = _flows.begin();
					while (flow_it != _flows.end()) {
						if (flow_it->second->assignedPath() == _paths[i]) {
							if(assignFlowToBondedPath(flow_it->second, now)) {
								_paths[i]->_assignedFlowCount--;
							}
						}
						++flow_it;
					}
					_paths[i]->_shouldReallocateFlows = false;
				}
			}
		}
	}
	/**
	 * Tasks specific to (Balance Round Robin)
	 */
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_RR) {
		if (_allowFlowHashing) {
			// TODO: Should ideally failover from (idx) to a random slave, this is so that (idx+1) isn't overloaded
		}
		else if (!_allowFlowHashing) {
			// Nothing
		}
	}
	/**
	 * Tasks specific to (Balance XOR)
	 */
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR) {
		// Nothing specific for XOR
	}
	/**
	 * Tasks specific to (Balance Aware)
	 */
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
		if (_allowFlowHashing) {
			Mutex::Lock _l(_flows_m);
			if (_flowRebalanceStrategy == ZT_MULTIPATH_FLOW_REBALANCE_STRATEGY_PASSIVE) {
				// Do nothing here, this is taken care of in the more general case above.
			}
			if (_flowRebalanceStrategy == ZT_MULTIPATH_FLOW_REBALANCE_STRATEGY_OPPORTUNISTIC) {
				// If the flow is temporarily inactive we should take this opportunity to re-assign the flow if needed.
			}
			if (_flowRebalanceStrategy == ZT_MULTIPATH_FLOW_REBALANCE_STRATEGY_AGGRESSIVE) {
				/**
				 * Return flows to the original path if it has once again become available
				 */
				if ((now - _lastFlowRebalance) > ZT_FLOW_REBALANCE_INTERVAL) {
					std::map<int32_t,SharedPtr<Flow> >::iterator flow_it = _flows.begin();
					while (flow_it != _flows.end()) {
						if (flow_it->second->_previouslyAssignedPath && flow_it->second->_previouslyAssignedPath->eligible(now, _ackSendInterval)
								&& (flow_it->second->_previouslyAssignedPath->_allocation >= (minimumAllocationValue * 2))) {
							fprintf(stderr, "moving flow back onto its previous path assignment (based on eligibility)\n");
							(flow_it->second->_assignedPath->_assignedFlowCount)--;
							flow_it->second->assignPath(flow_it->second->_previouslyAssignedPath,now);
							(flow_it->second->_previouslyAssignedPath->_assignedFlowCount)++;
						}
						++flow_it;
					}
					_lastFlowRebalance = now;
				}
				/**
				 * Return flows to the original path if it has once again become (performant)
				 */
				if ((now - _lastFlowRebalance) > ZT_FLOW_REBALANCE_INTERVAL) {
					std::map<int32_t,SharedPtr<Flow> >::iterator flow_it = _flows.begin();
					while (flow_it != _flows.end()) {
						if (flow_it->second->_previouslyAssignedPath && flow_it->second->_previouslyAssignedPath->eligible(now, _ackSendInterval)
								&& (flow_it->second->_previouslyAssignedPath->_allocation >= (minimumAllocationValue * 2))) {
							fprintf(stderr, "moving flow back onto its previous path assignment (based on performance)\n");
							(flow_it->second->_assignedPath->_assignedFlowCount)--;
							flow_it->second->assignPath(flow_it->second->_previouslyAssignedPath,now);
							(flow_it->second->_previouslyAssignedPath->_assignedFlowCount)++;
						}
						++flow_it;
					}
					_lastFlowRebalance = now;
				}
			}
		}
		else if (!_allowFlowHashing) {
			// Nothing
		}
	}
}

void Bond::dequeueNextActiveBackupPath(const uint64_t now)
{
	//fprintf(stderr, "dequeueNextActiveBackupPath\n");
	if (_abFailoverQueue.empty()) {
		return;
	}
	_abPath = _abFailoverQueue.front();
	_abFailoverQueue.pop_front();
	_lastActiveBackupPathChange = now;
	for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i]) {
			_paths[i]->resetPacketCounts();
		}
	}
}

void Bond::processActiveBackupTasks(const int64_t now)
{
	//fprintf(stderr, "%llu processActiveBackupTasks\n", (now - RR->bc->getBondStartTime()));
	char pathStr[128]; char prevPathStr[128]; char curPathStr[128];

	SharedPtr<Path> prevActiveBackupPath = _abPath;
	SharedPtr<Path> nonPreferredPath;
	bool bFoundPrimarySlave = false;

	/**
	 * Select initial "active" active-backup slave
	 */
	if (!_abPath) {
		fprintf(stderr, "%llu no active backup path yet...\n", ((now - RR->bc->getBondStartTime())));
		/**
		 * [Automatic mode]
		 * The user has not explicitly specified slaves or their failover schedule,
		 * the bonding policy will now select the first eligible path and set it as
		 * its active backup path, if a substantially better path is detected the bonding
		 * policy will assign it as the new active backup path. If the path fails it will
		 * simply find the next eligible path.
		 */
		if (!userHasSpecifiedSlaves()) {
			fprintf(stderr, "%llu AB: (auto) user did not specify any slaves. waiting until we know more\n", ((now - RR->bc->getBondStartTime())));
			for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (_paths[i] && _paths[i]->eligible(now,_ackSendInterval)) {
					_paths[i]->address().toString(curPathStr);
					SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
					if (slave) {
						fprintf(stderr, "%llu AB: (initial) [%d] found eligible path %s on: %s\n", ((now - RR->bc->getBondStartTime())), i, curPathStr, slave->ifname().c_str());
					}
					_abPath = _paths[i];
					break;
				}
			}
		}
		/**
	 	 * [Manual mode]
	 	 * The user has specified slaves or failover rules that the bonding policy should adhere to.
	 	 */
		else if (userHasSpecifiedSlaves()) {
			fprintf(stderr, "%llu AB: (manual) no active backup slave, checking local.conf\n", ((now - RR->bc->getBondStartTime())));
			if (userHasSpecifiedPrimarySlave()) {
				fprintf(stderr, "%llu AB: (manual) user has specified primary slave, looking for it.\n", ((now - RR->bc->getBondStartTime())));
				for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
					if (!_paths[i]) {
						continue;
					}
					SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
					if (_paths[i]->eligible(now,_ackSendInterval) && slave->primary()) {
						if (!_paths[i]->preferred()) {
							_paths[i]->address().toString(curPathStr);
							fprintf(stderr, "%llu AB: (initial) [%d] found path on primary slave, taking note in case we don't find a preferred path\n", ((now - RR->bc->getBondStartTime())), i);
							nonPreferredPath = _paths[i];
							bFoundPrimarySlave = true;
						}
						if (_paths[i]->preferred()) {
							_abPath = _paths[i];
							_abPath->address().toString(curPathStr);
							SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
							if (slave) {
								fprintf(stderr, "%llu AB: (initial) [%d] found preferred path %s on primary slave: %s\n", ((now - RR->bc->getBondStartTime())), i, curPathStr, slave->ifname().c_str());
							}
							bFoundPrimarySlave = true;
							break;
						}
					}
				}
				if (_abPath) {
					_abPath->address().toString(curPathStr);
					SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _abPath->localSocket());
					if (slave) {
						fprintf(stderr, "%llu AB: (initial) found preferred primary path: %s on %s\n", ((now - RR->bc->getBondStartTime())), curPathStr, slave->ifname().c_str());
					}
				}
				else {
					if (bFoundPrimarySlave && nonPreferredPath) {
						fprintf(stderr, "%llu AB: (initial) found a non-preferred primary path\n", ((now - RR->bc->getBondStartTime())));
						_abPath = nonPreferredPath;
					}
				}
				if (!_abPath) {
					fprintf(stderr, "%llu AB: (initial) designated primary slave is not yet ready\n", ((now - RR->bc->getBondStartTime())));
					// TODO: Should fail-over to specified backup or just wait?
				}
			}
			else if (!userHasSpecifiedPrimarySlave()) {
				int _abIdx = ZT_MAX_PEER_NETWORK_PATHS;
				fprintf(stderr, "%llu AB: (initial) user did not specify primary slave, just picking something\n", ((now - RR->bc->getBondStartTime())));
				for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
					if (_paths[i] && _paths[i]->eligible(now,_ackSendInterval)) {
						_abIdx = i;
						break;
					}
				}
				if (_abIdx == ZT_MAX_PEER_NETWORK_PATHS) {
					fprintf(stderr, "%llu AB: (initial) unable to find a candidate next-best, no change\n", ((now - RR->bc->getBondStartTime())));
				}
				else {
					_abPath = _paths[_abIdx];
					SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _abPath->localSocket());
					if (slave) {
						fprintf(stderr, "%llu AB: (initial) selected non-primary slave idx=%d, %s on %s\n", ((now - RR->bc->getBondStartTime())), _abIdx, pathStr, slave->ifname().c_str());
					}
				}
			}
		}
	}
	/**
	 * Update and maintain the active-backup failover queue
	 */
	if (_abPath) {
		// Don't worry about the failover queue until we have an active slave
		// Remove ineligible paths from the failover slave queue
		for (std::list<SharedPtr<Path> >::iterator it(_abFailoverQueue.begin()); it!=_abFailoverQueue.end();) {
			if ((*it) && !(*it)->eligible(now,_ackSendInterval)) {
				(*it)->address().toString(curPathStr);
				SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, (*it)->localSocket());
				if (slave) {
					fprintf(stderr, "%llu AB: (fq) %s on %s is now ineligible, removing from failover queue\n", ((now - RR->bc->getBondStartTime())), curPathStr, slave->ifname().c_str());
				}
				it = _abFailoverQueue.erase(it);
			} else {
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
			for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (_paths[i]) {
					_paths[i]->_failoverScore = 0;
				}
			}
			//fprintf(stderr, "AB: (fq) user has specified specific failover instructions, will follow them.\n");
			for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (!_paths[i] || !_paths[i]->allowed() || !_paths[i]->eligible(now,_ackSendInterval)) {
					continue;
				}
				SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
				_paths[i]->address().toString(pathStr);

				int failoverScoreHandicap = _paths[i]->_failoverScore;
				if (_paths[i]->preferred())
				{
					failoverScoreHandicap += ZT_MULTIPATH_FAILOVER_HANDICAP_PREFERRED;
					//fprintf(stderr, "%s on %s ----> %d for preferred\n", pathStr, _paths[i]->ifname().c_str(), failoverScoreHandicap);
				}
				if (slave->primary()) {
					// If using "optimize" primary reselect mode, ignore user slave designations
					failoverScoreHandicap += ZT_MULTIPATH_FAILOVER_HANDICAP_PRIMARY;
					//fprintf(stderr, "%s on %s ----> %d for primary\n", pathStr, _paths[i]->ifname().c_str(), failoverScoreHandicap);
				}
				if (!_paths[i]->_failoverScore) {
					// If we didn't inherit a failover score from a "parent" that wants to use this path as a failover
					int newHandicap = failoverScoreHandicap ? failoverScoreHandicap : _paths[i]->_allocation;
					_paths[i]->_failoverScore = newHandicap;
					//fprintf(stderr, "%s on %s ----> %d for allocation\n", pathStr, _paths[i]->ifname().c_str(), newHandicap);
				}
				SharedPtr<Slave> failoverSlave;
				if (slave->failoverToSlave().length()) {
					failoverSlave = RR->bc->getSlaveByName(_policyAlias, slave->failoverToSlave());
				}
				if (failoverSlave) {
					for (int j=0; j<ZT_MAX_PEER_NETWORK_PATHS; j++) {
						if (_paths[j] && getSlave(_paths[j]) == failoverSlave.ptr()) {
							_paths[j]->address().toString(pathStr);
							int inheritedHandicap = failoverScoreHandicap - 10;
							int newHandicap = _paths[j]->_failoverScore > inheritedHandicap ? _paths[j]->_failoverScore : inheritedHandicap;
							//fprintf(stderr, "\thanding down %s on %s ----> %d\n", pathStr, getSlave(_paths[j])->ifname().c_str(), newHandicap);
							if (!_paths[j]->preferred()) {
								newHandicap--;
							}
							_paths[j]->_failoverScore = newHandicap;
						}
					}
				}
				if (_paths[i].ptr() != _abPath.ptr()) {
					bool bFoundPathInQueue = false;
					for (std::list<SharedPtr<Path> >::iterator it(_abFailoverQueue.begin()); it!=_abFailoverQueue.end();++it) {
						if (_paths[i].ptr() == (*it).ptr()) {
							bFoundPathInQueue = true;
						}
					}
					if (!bFoundPathInQueue) {
						_paths[i]->address().toString(curPathStr);
						fprintf(stderr, "%llu AB: (fq) [%d] added %s on %s to queue\n", ((now - RR->bc->getBondStartTime())), i, curPathStr, getSlave(_paths[i])->ifname().c_str());
						_abFailoverQueue.push_front(_paths[i]);
					}
				}
			}
		}
		/**
		 * No failover instructions provided by user, build queue according to performance
		 * and IPv preference.
		 */
		else if (!userHasSpecifiedFailoverInstructions()) {
			for (int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
				if (!_paths[i]
					|| !_paths[i]->allowed()
					|| !_paths[i]->eligible(now,_ackSendInterval)) {
					continue;
				}
				int failoverScoreHandicap = 0;
				if (_paths[i]->preferred()) {
					failoverScoreHandicap = ZT_MULTIPATH_FAILOVER_HANDICAP_PREFERRED;
				}
				bool includeRefractoryPeriod = true;
				if (!_paths[i]->eligible(now,includeRefractoryPeriod)) {
					failoverScoreHandicap = -10000;
				}
				if (getSlave(_paths[i])->primary() && _abSlaveSelectMethod != ZT_MULTIPATH_RESELECTION_POLICY_OPTIMIZE) {
					// If using "optimize" primary reselect mode, ignore user slave designations
					failoverScoreHandicap = ZT_MULTIPATH_FAILOVER_HANDICAP_PRIMARY;
				}
				if (_paths[i].ptr() == negotiatedPath.ptr()) {
					_paths[i]->_negotiated = true;
					failoverScoreHandicap = ZT_MULTIPATH_FAILOVER_HANDICAP_NEGOTIATED;
				} else {
					_paths[i]->_negotiated = false;
				}
				_paths[i]->_failoverScore = _paths[i]->_allocation + failoverScoreHandicap;
				if (_paths[i].ptr() != _abPath.ptr()) {
					bool bFoundPathInQueue = false;
					for (std::list<SharedPtr<Path> >::iterator it(_abFailoverQueue.begin()); it!=_abFailoverQueue.end();++it) {
						if (_paths[i].ptr() == (*it).ptr()) {
							bFoundPathInQueue = true;
						}
					}
					if (!bFoundPathInQueue) {
						_paths[i]->address().toString(curPathStr);
						fprintf(stderr, "%llu AB: (fq) [%d] added %s on %s to queue\n", ((now - RR->bc->getBondStartTime())), i, curPathStr, getSlave(_paths[i])->ifname().c_str());
						_abFailoverQueue.push_front(_paths[i]);
					}
				}
			}
		}
		_abFailoverQueue.sort(PathQualityComparator());
		if (_abFailoverQueue.empty()) {
			fprintf(stderr, "%llu AB: (fq) the failover queue is empty, the active-backup bond is no longer fault-tolerant\n", ((now - RR->bc->getBondStartTime())));
		}
	}
	/**
	 * Short-circuit if we have no queued paths
	 */
	if (_abFailoverQueue.empty()) {
		return;
	}
	/**
	 * Fulfill primary reselect obligations
	 */
	if (_abPath && !_abPath->eligible(now,_ackSendInterval)) { // Implicit ZT_MULTIPATH_RESELECTION_POLICY_FAILURE
		_abPath->address().toString(curPathStr); fprintf(stderr, "%llu AB: (failure) failover event!, active backup path (%s) is no-longer eligible\n", ((now - RR->bc->getBondStartTime())), curPathStr);
		if (!_abFailoverQueue.empty()) {
			fprintf(stderr, "%llu AB: (failure) there are (%lu) slaves in queue to choose from...\n", ((now - RR->bc->getBondStartTime())), _abFailoverQueue.size());
			dequeueNextActiveBackupPath(now);
			_abPath->address().toString(curPathStr); fprintf(stderr, "%llu AB: (failure) switched to %s on %s\n", ((now - RR->bc->getBondStartTime())), curPathStr, getSlave(_abPath)->ifname().c_str());
		} else {
			fprintf(stderr, "%llu AB: (failure) nothing available in the slave queue, doing nothing.\n", ((now - RR->bc->getBondStartTime())));
		}
	}
	/**
	 * Detect change to prevent flopping during later optimization step.
	 */
	if (prevActiveBackupPath != _abPath) {
		_lastActiveBackupPathChange = now;
	}
	if (_abSlaveSelectMethod == ZT_MULTIPATH_RESELECTION_POLICY_ALWAYS) {
		if (_abPath && !getSlave(_abPath)->primary()
			&& getSlave(_abFailoverQueue.front())->primary()) {
			fprintf(stderr, "%llu AB: (always) switching to available primary\n", ((now - RR->bc->getBondStartTime())));
			dequeueNextActiveBackupPath(now);
		}
	}
	if (_abSlaveSelectMethod == ZT_MULTIPATH_RESELECTION_POLICY_BETTER) {
		if (_abPath && !getSlave(_abPath)->primary()) {
			fprintf(stderr, "%llu AB: (better) active backup has switched to \"better\" primary slave according to re-select policy.\n", ((now - RR->bc->getBondStartTime())));
			if (getSlave(_abFailoverQueue.front())->primary()
				&& (_abFailoverQueue.front()->_failoverScore > _abPath->_failoverScore)) {
				dequeueNextActiveBackupPath(now);
				fprintf(stderr, "%llu AB: (better) switched back to user-defined primary\n", ((now - RR->bc->getBondStartTime())));
			}
		}
	}
	if (_abSlaveSelectMethod == ZT_MULTIPATH_RESELECTION_POLICY_OPTIMIZE && !_abFailoverQueue.empty()) {
		/**
		 * Implement link negotiation that was previously-decided
		 */
		if (_abFailoverQueue.front()->_negotiated) {
			dequeueNextActiveBackupPath(now);
			_abPath->address().toString(prevPathStr);
			fprintf(stderr, "%llu AB: (optimize) switched to negotiated path %s on %s\n", ((now - RR->bc->getBondStartTime())), prevPathStr, getSlave(_abPath)->ifname().c_str());
			_lastPathNegotiationCheck = now;
		}
		else {
			// Try to find a better path and automatically switch to it -- not too often, though.
			if ((now - _lastActiveBackupPathChange) > ZT_MULTIPATH_MIN_ACTIVE_BACKUP_AUTOFLOP_INTERVAL) {
				if (!_abFailoverQueue.empty()) {
					//fprintf(stderr, "AB: (optimize) there are (%d) slaves in queue to choose from...\n", _abFailoverQueue.size());
					int newFScore = _abFailoverQueue.front()->_failoverScore;
					int prevFScore = _abPath->_failoverScore;
					// Establish a minimum switch threshold to prevent flapping
					int failoverScoreDifference = _abFailoverQueue.front()->_failoverScore - _abPath->_failoverScore;
					int thresholdQuantity = (ZT_MULTIPATH_ACTIVE_BACKUP_OPTIMIZE_MIN_THRESHOLD * (float)_abPath->_allocation);
					if ((failoverScoreDifference > 0) && (failoverScoreDifference > thresholdQuantity)) {
						SharedPtr<Path> oldPath = _abPath;
						_abPath->address().toString(prevPathStr);
						dequeueNextActiveBackupPath(now);
						_abPath->address().toString(curPathStr);
						fprintf(stderr, "%llu AB: (optimize) switched from %s on %s (fs=%d) to %s on %s (fs=%d)\n", ((now - RR->bc->getBondStartTime())), prevPathStr, getSlave(oldPath)->ifname().c_str(), prevFScore, curPathStr, getSlave(_abPath)->ifname().c_str(), newFScore);
					}
				}
			}
		}
	}
}

void Bond::setReasonableDefaults(int policy, SharedPtr<Bond> templateBond, bool useTemplate)
{
	// If invalid bonding policy, try default
	int _defaultBondingPolicy = BondController::defaultBondingPolicy();
	if (policy <= ZT_BONDING_POLICY_NONE || policy > ZT_BONDING_POLICY_BALANCE_AWARE) {
		// If no default set, use NONE (effectively disabling this bond)
		if (_defaultBondingPolicy < ZT_BONDING_POLICY_NONE || _defaultBondingPolicy > ZT_BONDING_POLICY_BALANCE_AWARE) {
			_bondingPolicy= ZT_BONDING_POLICY_NONE;
		}
		_bondingPolicy= _defaultBondingPolicy;
	} else {
		_bondingPolicy= policy;
	}

	_freeRandomByte = 0;
	_lastCheckUserPreferences = 0;
	_lastBackgroundTaskCheck = 0;

	_downDelay = 0;
	_upDelay = 0;
	_allowFlowHashing=false;
	_bondMonitorInterval=0;
	_shouldCollectPathStatistics=false;


	// Path negotiation
	_allowPathNegotiation=false;
	_lastPathNegotiationReceived=0;
	_lastPathNegotiationCheck=0;
	_pathNegotiationCutoffCount=0;
	_localUtility=0;

	_lastFlowStatReset=0;
	_lastFlowExpirationCheck=0;

	_numBondedPaths=0;
	_rrPacketsSentOnCurrSlave=0;
	_rrIdx=0;

	_lastFlowRebalance=0;
	_totalBondUnderload = 0;

	_maxAcceptableLatency = 100;
	_maxAcceptablePacketDelayVariance = 50;
	_maxAcceptablePacketLossRatio = 0.10;
	_maxAcceptablePacketErrorRatio = 0.10;
	_userHasSpecifiedSlaveSpeeds=0;

	_lastFrame=0;



	/* ZT_MULTIPATH_FLOW_REBALANCE_STRATEGY_PASSIVE is the most conservative strategy and is
	least likely to cause unexpected behavior */
	_flowRebalanceStrategy = ZT_MULTIPATH_FLOW_REBALANCE_STRATEGY_AGGRESSIVE;

	/**
	 * Paths are actively monitored to provide a real-time quality/preference-ordered rapid failover queue.
	 */
	switch (policy) {
		case ZT_BONDING_POLICY_ACTIVE_BACKUP:
			_failoverInterval = 500;
			_abSlaveSelectMethod = ZT_MULTIPATH_RESELECTION_POLICY_OPTIMIZE;
			_slaveMonitorStrategy = ZT_MULTIPATH_SLAVE_MONITOR_STRATEGY_DYNAMIC;
			_qualityWeights[ZT_QOS_LAT_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_LTM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_PDV_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_PLR_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_PER_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_THR_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_THM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_THV_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_SCP_IDX] = 0.0f;
			break;
		/**
		 * All seemingly-alive paths are used. Paths are not actively monitored.
		 */
		case ZT_BONDING_POLICY_BROADCAST:
			_downDelay = 30000;
			_upDelay = 0;
			break;
		/**
		 * Paths are monitored to determine when/if one needs to be added or removed from the rotation
		 */
		case ZT_BONDING_POLICY_BALANCE_RR:
			_failoverInterval = 500;
			_allowFlowHashing = false;
			_packetsPerSlave = 1024;
			_slaveMonitorStrategy = ZT_MULTIPATH_SLAVE_MONITOR_STRATEGY_DYNAMIC;
			_qualityWeights[ZT_QOS_LAT_IDX] = 0.4f;
			_qualityWeights[ZT_QOS_LTM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_PDV_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_PLR_IDX] = 0.1f;
			_qualityWeights[ZT_QOS_PER_IDX] = 0.1f;
			_qualityWeights[ZT_QOS_THR_IDX] = 0.1f;
			_qualityWeights[ZT_QOS_THM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_THV_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_SCP_IDX] = 0.0f;
			break;
		/**
		 * Path monitoring is used to determine the capacity of each
		 * path and where to place the next flow.
		 */
		case ZT_BONDING_POLICY_BALANCE_XOR:
			_failoverInterval = 500;
			_upDelay = _bondMonitorInterval * 2;
			_allowFlowHashing = true;
			_slaveMonitorStrategy = ZT_MULTIPATH_SLAVE_MONITOR_STRATEGY_DYNAMIC;
			_qualityWeights[ZT_QOS_LAT_IDX] = 0.4f;
			_qualityWeights[ZT_QOS_LTM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_PDV_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_PLR_IDX] = 0.1f;
			_qualityWeights[ZT_QOS_PER_IDX] = 0.1f;
			_qualityWeights[ZT_QOS_THR_IDX] = 0.1f;
			_qualityWeights[ZT_QOS_THM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_THV_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_SCP_IDX] = 0.0f;
			break;
		/**
		 * Path monitoring is used to determine the capacity of each
		 * path and where to place the next flow. Additionally, re-shuffling
		 * of flows may take place.
		 */
		case ZT_BONDING_POLICY_BALANCE_AWARE:
			_failoverInterval = 3000;
			_allowFlowHashing = true;
			_slaveMonitorStrategy = ZT_MULTIPATH_SLAVE_MONITOR_STRATEGY_DYNAMIC;
			_qualityWeights[ZT_QOS_LAT_IDX] = 0.4f;
			_qualityWeights[ZT_QOS_LTM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_PDV_IDX] = 0.4f;
			_qualityWeights[ZT_QOS_PLR_IDX] = 0.2f;
			_qualityWeights[ZT_QOS_PER_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_THR_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_THM_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_THV_IDX] = 0.0f;
			_qualityWeights[ZT_QOS_SCP_IDX] = 0.0f;
			break;
		default:
			break;
	}

	/* If a user has specified custom parameters for this bonding policy, overlay
	them onto the defaults that were previously set */
	if (useTemplate) {
		_policyAlias = templateBond->_policyAlias;
		_failoverInterval = templateBond->_failoverInterval;
		_downDelay = templateBond->_downDelay;
		_upDelay = templateBond->_upDelay;

		fprintf(stderr, "TIMERS: strat=%d, fi= %d, bmi= %d, qos= %d, ack= %d, estimateInt= %d, refractory= %d, ud= %d, dd= %d\n",
			_slaveMonitorStrategy,
			_failoverInterval,
			_bondMonitorInterval,
			_qosSendInterval,
			_ackSendInterval,
			_qualityEstimationInterval,
			_defaultPathRefractoryPeriod,
			_upDelay,
			_downDelay);

		if (templateBond->_slaveMonitorStrategy == ZT_MULTIPATH_SLAVE_MONITOR_STRATEGY_PASSIVE
			&& templateBond->_failoverInterval != 0) {
			fprintf(stderr, "warning: passive path monitoring was specified, this will prevent failovers from happening in a timely manner.\n");
		}
		_abSlaveSelectMethod = templateBond->_abSlaveSelectMethod;
		memcpy(_qualityWeights, templateBond->_qualityWeights, ZT_QOS_WEIGHT_SIZE * sizeof(float));
	}


	//
	// Second, apply user specified values (only if they make sense)

	/**
	 * Timer geometries and counters
	 */
	// TODO: Think more about the maximum
	/*
	if (originalBond._failoverInterval > 250 && originalBond._failoverInterval < 65535) {
		_failoverInterval = originalBond._failoverInterval;
	}
	else {
		fprintf(stderr, "warning: _failoverInterval (%d) is out of range, using default (%d)\n", originalBond._failoverInterval, _failoverInterval);
	}
	*/

	_bondMonitorInterval = _failoverInterval / 3;
	BondController::setMinReqPathMonitorInterval(_bondMonitorInterval);
	_ackSendInterval = _failoverInterval;
	_qualityEstimationInterval = _failoverInterval * 2;
	_dynamicPathMonitorInterval = 0;
	_ackCutoffCount = 0;
	_lastAckRateCheck = 0;
	_qosSendInterval = _bondMonitorInterval * 4;
	_qosCutoffCount = 0;
	_lastQoSRateCheck = 0;
	_lastQualityEstimation=0;
	throughputMeasurementInterval = _ackSendInterval * 2;
	_defaultPathRefractoryPeriod = 8000;
}

void Bond::setUserQualityWeights(float weights[], int len)
{
	if (len == ZT_QOS_WEIGHT_SIZE) {
		float weightTotal = 0.0;
		for (unsigned int i=0; i<ZT_QOS_WEIGHT_SIZE; ++i) {
			weightTotal += weights[i];
		}
		if (weightTotal > 0.99 && weightTotal < 1.01) {
			memcpy(_qualityWeights, weights, len * sizeof(float));
		}
	}
}


bool Bond::relevant() {
	return _peer->identity().address().toInt() == 0x16a03a3d03
		|| _peer->identity().address().toInt() == 0x4410300d03
		|| _peer->identity().address().toInt() == 0x795cbf86fa;
}

SharedPtr<Slave> Bond::getSlave(const SharedPtr<Path>& path)
{
	return RR->bc->getSlaveBySocket(_policyAlias, path->localSocket());
}

void Bond::dumpInfo(const int64_t now)
{
	char pathStr[128];
	//char oldPathStr[128];
	char currPathStr[128];

	if (!relevant()) {
		return;
	}
	/*
	fprintf(stderr, "---[ bp=%d, id=%llx, dd=%d, up=%d, pmi=%d, specifiedSlaves=%d, _specifiedPrimarySlave=%d, _specifiedFailInst=%d ]\n",
			_policy, _peer->identity().address().toInt(), _downDelay, _upDelay, _monitorInterval, _userHasSpecifiedSlaves, _userHasSpecifiedPrimarySlave, _userHasSpecifiedFailoverInstructions);

	if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP) {
		fprintf(stderr, "Paths (bp=%d, stats=%d, primaryReselect=%d) :\n",
			_policy, _shouldCollectPathStatistics, _abSlaveSelectMethod);
	}
	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_RR
		|| _bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR
		|| _bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
		fprintf(stderr, "Paths (bp=%d, stats=%d, fh=%d) :\n",
			_policy, _shouldCollectPathStatistics, _allowFlowHashing);
	}*/
	if ((now - _lastPrintTS) < 2000) {
		return;
	}
	_lastPrintTS = now;

	fprintf(stderr, "\n\n");

	for(int i=0; i<ZT_MAX_PEER_NETWORK_PATHS; ++i) {
		if (_paths[i]) {
			SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[i]->localSocket());
			_paths[i]->address().toString(pathStr);
			fprintf(stderr, " %2d: lat=%8.3f, ac=%3d, fail%5s, fscore=%6d, in=%7d, out=%7d, age=%7ld, ack=%7ld, ref=%6d, ls=%llx",
				i,
				_paths[i]->_latencyMean,
				_paths[i]->_allocation,
				slave->failoverToSlave().c_str(),
				_paths[i]->_failoverScore,
				_paths[i]->_packetsIn,
				_paths[i]->_packetsOut,
				(long)_paths[i]->age(now),
				(long)_paths[i]->ackAge(now),
				_paths[i]->_refractoryPeriod,
				_paths[i]->localSocket()
			);
			if (slave->spare()) {
				fprintf(stderr, " SPR.");
			} else {
				fprintf(stderr, "     ");
			}
			if (slave->primary()) {
				fprintf(stderr, " PRIM.");
			} else {
				fprintf(stderr, "      ");
			}
			if (_paths[i]->allowed()) {
				fprintf(stderr, " ALL.");
			} else {
				fprintf(stderr, "     ");
			}
			if (_paths[i]->eligible(now,_ackSendInterval)) {
				fprintf(stderr, " ELI.");
			} else {
				fprintf(stderr, "     ");
			}
			if (_paths[i]->preferred()) {
				fprintf(stderr, " PREF.");
			} else {
				fprintf(stderr, "      ");
			}
			if (_paths[i]->_negotiated) {
				fprintf(stderr, " NEG.");
			} else {
				fprintf(stderr, "     ");
			}
			if (_paths[i]->bonded()) {
				fprintf(stderr, " BOND ");
			} else {
				fprintf(stderr, "      ");
			}
			if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP && _abPath && (_abPath == _paths[i].ptr())) {
				fprintf(stderr, " ACTIVE  ");
			} else if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP) {
				fprintf(stderr, "         ");
			}
			if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP && _abFailoverQueue.size() && (_abFailoverQueue.front().ptr() == _paths[i].ptr())) {
				fprintf(stderr, " NEXT    ");
			} else  if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP) {
				fprintf(stderr, "         ");
			}
			fprintf(stderr, "%5s %s\n", slave->ifname().c_str(), pathStr);
		}
	}

	if (_bondingPolicy == ZT_BONDING_POLICY_ACTIVE_BACKUP) {
		if (!_abFailoverQueue.empty()) {
			fprintf(stderr, "\nFailover Queue:\n");
			for (std::list<SharedPtr<Path> >::iterator it(_abFailoverQueue.begin()); it!=_abFailoverQueue.end();++it) {
				(*it)->address().toString(currPathStr);
				SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, (*it)->localSocket());
				fprintf(stderr, "\t%8s\tspeed=%7d\trelSpeed=%3d\tipvPref=%3d\tfscore=%9d\t\t%s\n",
					slave->ifname().c_str(),
					slave->speed(),
					slave->relativeSpeed(),
					slave->ipvPref(),
					(*it)->_failoverScore,
					currPathStr);
			}
		}
		else
		{
			fprintf(stderr, "\nFailover Queue size = %lu\n", _abFailoverQueue.size());
		}
	}

	if (_bondingPolicy == ZT_BONDING_POLICY_BALANCE_RR
		|| _bondingPolicy == ZT_BONDING_POLICY_BALANCE_XOR
		|| _bondingPolicy == ZT_BONDING_POLICY_BALANCE_AWARE) {
		if (_numBondedPaths) {
			fprintf(stderr, "\nBonded Paths:\n");
			for (int i=0; i<_numBondedPaths; ++i) {
				_paths[_bondedIdx[i]]->address().toString(currPathStr);
				SharedPtr<Slave> slave =RR->bc->getSlaveBySocket(_policyAlias, _paths[_bondedIdx[i]]->localSocket());
				fprintf(stderr, " [%d]\t%8s\tflows=%3d\tspeed=%7d\trelSpeed=%3d\tipvPref=%3d\tfscore=%9d\t\t%s\n", i,
				//fprintf(stderr, " [%d]\t%8s\tspeed=%7d\trelSpeed=%3d\tflowCount=%2d\tipvPref=%3d\tfscore=%9d\t\t%s\n", i,
					slave->ifname().c_str(),
					_paths[_bondedIdx[i]]->_assignedFlowCount,
					slave->speed(),
					slave->relativeSpeed(),
					//_paths[_bondedIdx[i]].p->assignedFlows.size(),
					slave->ipvPref(),
					_paths[_bondedIdx[i]]->_failoverScore,
					currPathStr);
			}
		}
	}
}

} // namespace ZeroTier