/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <list>
#include <vector>
#include <string>

#include "Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/file.h>
#endif

#include "../version.h"

#include "Node.hpp"
#include "RuntimeEnvironment.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Defaults.hpp"
#include "Identity.hpp"
#include "Topology.hpp"
#include "SocketManager.hpp"
#include "Packet.hpp"
#include "Switch.hpp"
#include "EthernetTap.hpp"
#include "CMWC4096.hpp"
#include "NodeConfig.hpp"
#include "Network.hpp"
#include "MulticastGroup.hpp"
#include "Multicaster.hpp"
#include "Mutex.hpp"
#include "SoftwareUpdater.hpp"
#include "Buffer.hpp"
#include "AntiRecursion.hpp"
#include "RoutingTable.hpp"
#include "HttpClient.hpp"
#include "NetworkConfigMaster.hpp"

namespace ZeroTier {

struct _NodeImpl
{
	RuntimeEnvironment renv;

	std::string reasonForTerminationStr;
	volatile Node::ReasonForTermination reasonForTermination;

	volatile bool started;
	volatile bool running;
	volatile bool resynchronize;

	volatile bool disableRootTopologyUpdates;
	std::string overrideRootTopology;

	// This function performs final node tear-down
	inline Node::ReasonForTermination terminate()
	{
		RuntimeEnvironment *RR = &renv;
		LOG("terminating: %s",reasonForTerminationStr.c_str());

		running = false;

		delete renv.updater;       renv.updater = (SoftwareUpdater *)0;
		delete renv.nc;            renv.nc = (NodeConfig *)0;            // shut down all networks, close taps, etc.
		delete renv.topology;      renv.topology = (Topology *)0;        // now we no longer need routing info
		delete renv.mc;            renv.mc = (Multicaster *)0;
		delete renv.antiRec;       renv.antiRec = (AntiRecursion *)0;
		delete renv.sw;            renv.sw = (Switch *)0;                // order matters less from here down
		delete renv.netconfMaster; renv.netconfMaster = (NetworkConfigMaster *)0;
		delete renv.http;          renv.http = (HttpClient *)0;
		delete renv.prng;          renv.prng = (CMWC4096 *)0;
		delete renv.log;           renv.log = (Logger *)0;               // but stop logging last of all

		return reasonForTermination;
	}

	inline Node::ReasonForTermination terminateBecause(Node::ReasonForTermination r,const char *rstr)
	{
		reasonForTerminationStr = rstr;
		reasonForTermination = r;
		return terminate();
	}
};

Node::Node(
	const char *hp,
	EthernetTapFactory *tf,
	RoutingTable *rt,
	SocketManager *sm,
	bool resetIdentity,
	const char *overrideRootTopology) throw() :
	_impl(new _NodeImpl)
{
	_NodeImpl *impl = (_NodeImpl *)_impl;

	if ((hp)&&(hp[0]))
		impl->renv.homePath = hp;
	else impl->renv.homePath = ZT_DEFAULTS.defaultHomePath;

	impl->renv.tapFactory = tf;
	impl->renv.routingTable = rt;
	impl->renv.sm = sm;

	if (resetIdentity) {
		// Forget identity and peer database, peer keys, etc.
		Utils::rm((impl->renv.homePath + ZT_PATH_SEPARATOR_S + "identity.public").c_str());
		Utils::rm((impl->renv.homePath + ZT_PATH_SEPARATOR_S + "identity.secret").c_str());
		Utils::rm((impl->renv.homePath + ZT_PATH_SEPARATOR_S + "peers.persist").c_str());

		// Truncate network config information in networks.d but leave the files since we
		// still want to remember any networks we have joined. This will force those networks
		// to be reconfigured with our newly regenerated identity after startup.
		std::string networksDotD(impl->renv.homePath + ZT_PATH_SEPARATOR_S + "networks.d");
		std::map< std::string,bool > nwfiles(Utils::listDirectory(networksDotD.c_str()));
		for(std::map<std::string,bool>::iterator nwf(nwfiles.begin());nwf!=nwfiles.end();++nwf) {
			FILE *trun = fopen((networksDotD + ZT_PATH_SEPARATOR_S + nwf->first).c_str(),"w");
			if (trun)
				fclose(trun);
		}
	}

	impl->reasonForTermination = Node::NODE_RUNNING;
	impl->started = false;
	impl->running = false;
	impl->resynchronize = false;

	if (overrideRootTopology) {
		impl->disableRootTopologyUpdates = true;
		impl->overrideRootTopology = overrideRootTopology;
	} else {
		impl->disableRootTopologyUpdates = false;
	}
}

Node::~Node()
{
	delete (_NodeImpl *)_impl;
}

static void _CBztTraffic(const SharedPtr<Socket> &fromSock,void *arg,const InetAddress &from,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &data)
{
	((const RuntimeEnvironment *)arg)->sw->onRemotePacket(fromSock,from,data);
}

static void _cbHandleGetRootTopology(void *arg,int code,const std::string &url,const std::string &body)
{
	RuntimeEnvironment *RR = (RuntimeEnvironment *)arg;

	if ((code != 200)||(body.length() == 0)) {
		TRACE("failed to retrieve %s",url.c_str());
		return;
	}

	try {
		Dictionary rt(body);
		if (!Topology::authenticateRootTopology(rt)) {
			LOG("discarded invalid root topology update from %s (signature check failed)",url.c_str());
			return;
		}

		{
			std::string rootTopologyPath(RR->homePath + ZT_PATH_SEPARATOR_S + "root-topology");
			std::string rootTopology;
			if (Utils::readFile(rootTopologyPath.c_str(),rootTopology)) {
				Dictionary alreadyHave(rootTopology);
				if (alreadyHave == rt) {
					TRACE("retrieved root topology from %s but no change (same as on disk)",url.c_str());
					return;
				} else if (alreadyHave.signatureTimestamp() > rt.signatureTimestamp()) {
					TRACE("retrieved root topology from %s but no change (ours is newer)",url.c_str());
					return;
				}
			}
			Utils::writeFile(rootTopologyPath.c_str(),body);
		}

		RR->topology->setSupernodes(Dictionary(rt.get("supernodes")));
	} catch ( ... ) {
		LOG("discarded invalid root topology update from %s (format invalid)",url.c_str());
		return;
	}
}

Node::ReasonForTermination Node::run()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);

	impl->started = true;
	impl->running = true;

	try {
#ifdef ZT_LOG_STDOUT
		RR->log = new Logger((const char *)0,(const char *)0,0);
#else
		RR->log = new Logger((RR->homePath + ZT_PATH_SEPARATOR_S + "node.log").c_str(),(const char *)0,131072);
#endif

		LOG("starting version %s",versionString());

		// Create non-crypto PRNG right away in case other code in init wants to use it
		RR->prng = new CMWC4096();

		// Read identity public and secret, generating if not present
		{
			bool gotId = false;
			std::string identitySecretPath(RR->homePath + ZT_PATH_SEPARATOR_S + "identity.secret");
			std::string identityPublicPath(RR->homePath + ZT_PATH_SEPARATOR_S + "identity.public");
			std::string idser;
			if (Utils::readFile(identitySecretPath.c_str(),idser))
				gotId = RR->identity.fromString(idser);
			if ((gotId)&&(!RR->identity.locallyValidate()))
				gotId = false;
			if (gotId) {
				// Make sure identity.public matches identity.secret
				idser = std::string();
				Utils::readFile(identityPublicPath.c_str(),idser);
				std::string pubid(RR->identity.toString(false));
				if (idser != pubid) {
					if (!Utils::writeFile(identityPublicPath.c_str(),pubid))
						return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write identity.public (home path not writable?)");
				}
			} else {
				LOG("no identity found or identity invalid, generating one... this might take a few seconds...");
				RR->identity.generate();
				LOG("generated new identity: %s",RR->identity.address().toString().c_str());
				idser = RR->identity.toString(true);
				if (!Utils::writeFile(identitySecretPath.c_str(),idser))
					return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write identity.secret (home path not writable?)");
				idser = RR->identity.toString(false);
				if (!Utils::writeFile(identityPublicPath.c_str(),idser))
					return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write identity.public (home path not writable?)");
			}
			Utils::lockDownFile(identitySecretPath.c_str(),false);
		}

		// Make sure networks.d exists (used by NodeConfig to remember networks)
		{
			std::string networksDotD(RR->homePath + ZT_PATH_SEPARATOR_S + "networks.d");
#ifdef __WINDOWS__
			CreateDirectoryA(networksDotD.c_str(),NULL);
#else
			mkdir(networksDotD.c_str(),0700);
#endif
		}
		// Make sure iddb.d exists (used by Topology to remember identities)
		{
			std::string iddbDotD(RR->homePath + ZT_PATH_SEPARATOR_S + "iddb.d");
#ifdef __WINDOWS__
			CreateDirectoryA(iddbDotD.c_str(),NULL);
#else
			mkdir(iddbDotD.c_str(),0700);
#endif
		}

		RR->http = new HttpClient();
		RR->sw = new Switch(RR);
		RR->mc = new Multicaster(RR);
		RR->antiRec = new AntiRecursion();
		RR->topology = new Topology(RR);
		try {
			RR->nc = new NodeConfig(RR);
		} catch (std::exception &exc) {
			return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unable to initialize IPC socket: is ZeroTier One already running?");
		}
		RR->node = this;

#ifdef ZT_AUTO_UPDATE
		if (ZT_DEFAULTS.updateLatestNfoURL.length()) {
			RR->updater = new SoftwareUpdater(RR);
			RR->updater->cleanOldUpdates(); // clean out updates.d on startup
		} else {
			LOG("WARNING: unable to enable software updates: latest .nfo URL from ZT_DEFAULTS is empty (does this platform actually support software updates?)");
		}
#endif

		// Initialize root topology from defaults or root-toplogy file in home path on disk
		if (impl->overrideRootTopology.length() == 0) {
			std::string rootTopologyPath(RR->homePath + ZT_PATH_SEPARATOR_S + "root-topology");
			std::string rootTopology;
			if (!Utils::readFile(rootTopologyPath.c_str(),rootTopology))
				rootTopology = ZT_DEFAULTS.defaultRootTopology;
			try {
				Dictionary rt(rootTopology);

				if (Topology::authenticateRootTopology(rt)) {
					// Set supernodes if root topology signature is valid
					RR->topology->setSupernodes(Dictionary(rt.get("supernodes",""))); // set supernodes from root-topology

					// If root-topology contains noupdate=1, disable further updates and only use what was on disk
					impl->disableRootTopologyUpdates = (Utils::strToInt(rt.get("noupdate","0").c_str()) > 0);
				} else {
					// Revert to built-in defaults if root topology fails signature check
					LOG("%s failed signature check, using built-in defaults instead",rootTopologyPath.c_str());
					Utils::rm(rootTopologyPath.c_str());
					RR->topology->setSupernodes(Dictionary(Dictionary(ZT_DEFAULTS.defaultRootTopology).get("supernodes","")));
					impl->disableRootTopologyUpdates = false;
				}
			} catch ( ... ) {
				return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"invalid root-topology format");
			}
		} else {
			try {
				Dictionary rt(impl->overrideRootTopology);
				RR->topology->setSupernodes(Dictionary(rt.get("supernodes","")));
				impl->disableRootTopologyUpdates = true;
			} catch ( ... ) {
				return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"invalid root-topology format");
			}
		}

		// Delete peers.persist if it exists -- legacy file, just takes up space
		Utils::rm(std::string(RR->homePath + ZT_PATH_SEPARATOR_S + "peers.persist").c_str());
	} catch (std::bad_alloc &exc) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"memory allocation failure");
	} catch (std::runtime_error &exc) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,exc.what());
	} catch ( ... ) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unknown exception during initialization");
	}

	// Core I/O loop
	try {
		/* Shut down if this file exists but fails to open. This is used on Mac to
		 * shut down automatically on .app deletion by symlinking this to the
		 * Info.plist file inside the ZeroTier One application. This causes the
		 * service to die when the user throws away the app, allowing uninstallation
		 * in the natural Mac way. */
		std::string shutdownIfUnreadablePath(RR->homePath + ZT_PATH_SEPARATOR_S + "shutdownIfUnreadable");

		uint64_t lastNetworkAutoconfCheck = Utils::now() - 5000ULL; // check autoconf again after 5s for startup
		uint64_t lastPingCheck = 0;
		uint64_t lastClean = Utils::now(); // don't need to do this immediately
		uint64_t lastNetworkFingerprintCheck = 0;
		uint64_t lastMulticastCheck = 0;
		uint64_t lastSupernodePingCheck = 0;
		uint64_t lastBeacon = 0;
		uint64_t lastRootTopologyFetch = 0;
		uint64_t lastShutdownIfUnreadableCheck = 0;
		long lastDelayDelta = 0;

		uint64_t networkConfigurationFingerprint = 0;
		RR->timeOfLastResynchronize = Utils::now();

		// We are up and running
		RR->initialized = true;

		while (impl->reasonForTermination == NODE_RUNNING) {
			uint64_t now = Utils::now();
			bool resynchronize = false;

			/* This is how the service automatically shuts down when the OSX .app is
			 * thrown in the trash. It's not used on any other platform for now but
			 * could do similar things. It's disabled on Windows since it doesn't really
			 * work there. */
#ifdef __UNIX_LIKE__
			if ((now - lastShutdownIfUnreadableCheck) > 10000) {
				lastShutdownIfUnreadableCheck = now;
				if (Utils::fileExists(shutdownIfUnreadablePath.c_str(),false)) {
					int tmpfd = ::open(shutdownIfUnreadablePath.c_str(),O_RDONLY,0);
					if (tmpfd < 0) {
						return impl->terminateBecause(Node::NODE_NORMAL_TERMINATION,"shutdownIfUnreadable exists but is not readable");
					} else ::close(tmpfd);
				}
			}
#endif

			// If it looks like the computer slept and woke, resynchronize.
			if (lastDelayDelta >= ZT_SLEEP_WAKE_DETECTION_THRESHOLD) {
				resynchronize = true;
				LOG("probable suspend/resume detected, pausing a moment for things to settle...");
				Thread::sleep(ZT_SLEEP_WAKE_SETTLE_TIME);
			}

			// If our network environment looks like it changed, resynchronize.
			if ((resynchronize)||((now - lastNetworkFingerprintCheck) >= ZT_NETWORK_FINGERPRINT_CHECK_DELAY)) {
				lastNetworkFingerprintCheck = now;
				uint64_t fp = RR->routingTable->networkEnvironmentFingerprint(RR->nc->networkTapDeviceNames());
				if (fp != networkConfigurationFingerprint) {
					LOG("netconf fingerprint change: %.16llx != %.16llx, resyncing with network",networkConfigurationFingerprint,fp);
					networkConfigurationFingerprint = fp;
					resynchronize = true;
				}
			}

			// Supernodes do not resynchronize unless explicitly ordered via SIGHUP.
			if ((resynchronize)&&(RR->topology->amSupernode()))
				resynchronize = false;

			// Check for SIGHUP / force resync.
			if (impl->resynchronize) {
				impl->resynchronize = false;
				resynchronize = true;
				LOG("resynchronize forced by user, syncing with network");
			}

			if (resynchronize) {
				RR->tcpTunnelingEnabled = false; // turn off TCP tunneling master switch at first, will be reenabled on persistent UDP failure
				RR->timeOfLastResynchronize = now;
			}

			/* Supernodes are pinged separately and more aggressively. The
			 * ZT_STARTUP_AGGRO parameter sets a limit on how rapidly they are
			 * tried, while PingSupernodesThatNeedPing contains the logic for
			 * determining if they need PING. */
			if ((now - lastSupernodePingCheck) >= ZT_STARTUP_AGGRO) {
				lastSupernodePingCheck = now;

				uint64_t lastReceiveFromAnySupernode = 0; // function object result paramter
				RR->topology->eachSupernodePeer(Topology::FindMostRecentDirectReceiveTimestamp(lastReceiveFromAnySupernode));

				// Turn on TCP tunneling master switch if we haven't heard anything since before
				// the last resynchronize and we've been trying long enough.
				uint64_t tlr = RR->timeOfLastResynchronize;
				if ((lastReceiveFromAnySupernode < tlr)&&((now - tlr) >= ZT_TCP_TUNNEL_FAILOVER_TIMEOUT)) {
					TRACE("network still unreachable after %u ms, TCP TUNNELING ENABLED",(unsigned int)ZT_TCP_TUNNEL_FAILOVER_TIMEOUT);
					RR->tcpTunnelingEnabled = true;
				}

				RR->topology->eachSupernodePeer(Topology::PingSupernodesThatNeedPing(RR,now));
			}

			if (resynchronize) {
				/* Send NOP to all peers on resynchronize, directly to supernodes and
				 * indirectly to regular nodes (to trigger RENDEZVOUS). Also clear
				 * learned paths since they're likely no longer valid, and close
				 * TCP sockets since they're also likely invalid. */
				RR->sm->closeTcpSockets();
				RR->topology->eachPeer(Topology::ResetActivePeers(RR,now));
			} else {
				/* Periodically check for changes in our local multicast subscriptions
				 * and broadcast those changes to directly connected peers. */
				if ((now - lastMulticastCheck) >= ZT_MULTICAST_LOCAL_POLL_PERIOD) {
					lastMulticastCheck = now;
					try {
						std::vector< SharedPtr<Network> > networks(RR->nc->networks());
						for(std::vector< SharedPtr<Network> >::const_iterator nw(networks.begin());nw!=networks.end();++nw)
							(*nw)->rescanMulticastGroups();
					} catch (std::exception &exc) {
						LOG("unexpected exception announcing multicast groups: %s",exc.what());
					} catch ( ... ) {
						LOG("unexpected exception announcing multicast groups: (unknown)");
					}
				}

				/* Periodically ping all our non-stale direct peers unless we're a supernode.
				 * Supernodes only ping each other (which is done above). */
				if ((!RR->topology->amSupernode())&&((now - lastPingCheck) >= ZT_PING_CHECK_DELAY)) {
					lastPingCheck = now;
					try {
						RR->topology->eachPeer(Topology::PingPeersThatNeedPing(RR,now));
					} catch (std::exception &exc) {
						LOG("unexpected exception running ping check cycle: %s",exc.what());
					} catch ( ... ) {
						LOG("unexpected exception running ping check cycle: (unkonwn)");
					}
				}
			}

			// Update network configurations when needed.
			try {
				if ((resynchronize)||((now - lastNetworkAutoconfCheck) >= ZT_NETWORK_AUTOCONF_CHECK_DELAY)) {
					lastNetworkAutoconfCheck = now;
					std::vector< SharedPtr<Network> > nets(RR->nc->networks());
					for(std::vector< SharedPtr<Network> >::iterator n(nets.begin());n!=nets.end();++n) {
						if ((now - (*n)->lastConfigUpdate()) >= ZT_NETWORK_AUTOCONF_DELAY)
							(*n)->requestConfiguration();
					}
				}
			} catch ( ... ) {
				LOG("unexpected exception updating network configurations (non-fatal, will retry)");
			}

			// Do periodic tasks in submodules.
			if ((now - lastClean) >= ZT_DB_CLEAN_PERIOD) {
				lastClean = now;
				try {
					RR->topology->clean(now);
				} catch ( ... ) {
					LOG("unexpected exception in Topology::clean() (non-fatal)");
				}
				try {
					RR->mc->clean(now);
				} catch ( ... ) {
					LOG("unexpected exception in Multicaster::clean() (non-fatal)");
				}
				try {
					RR->nc->clean();
				} catch ( ... ) {
					LOG("unexpected exception in NodeConfig::clean() (non-fatal)");
				}
				try {
					if (RR->updater)
						RR->updater->checkIfMaxIntervalExceeded(now);
				} catch ( ... ) {
					LOG("unexpected exception in SoftwareUpdater::checkIfMaxIntervalExceeded() (non-fatal)");
				}
			}

			// Send beacons to physical local LANs
			try {
				if ((resynchronize)||((now - lastBeacon) >= ZT_BEACON_INTERVAL)) {
					lastBeacon = now;
					char bcn[ZT_PROTO_BEACON_LENGTH];
					void *bcnptr = bcn;
					*((uint32_t *)(bcnptr)) = RR->prng->next32();
					bcnptr = bcn + 4;
					*((uint32_t *)(bcnptr)) = RR->prng->next32();
					RR->identity.address().copyTo(bcn + ZT_PROTO_BEACON_IDX_ADDRESS,ZT_ADDRESS_LENGTH);
					TRACE("sending LAN beacon to %s",ZT_DEFAULTS.v4Broadcast.toString().c_str());
					RR->antiRec->logOutgoingZT(bcn,ZT_PROTO_BEACON_LENGTH);
					RR->sm->send(ZT_DEFAULTS.v4Broadcast,false,false,bcn,ZT_PROTO_BEACON_LENGTH);
				}
			} catch ( ... ) {
				LOG("unexpected exception sending LAN beacon (non-fatal)");
			}

			// Check for updates to root topology (supernodes) periodically
			try {
				if ((now - lastRootTopologyFetch) >= ZT_UPDATE_ROOT_TOPOLOGY_CHECK_INTERVAL) {
					lastRootTopologyFetch = now;
					if (!impl->disableRootTopologyUpdates) {
						TRACE("fetching root topology from %s",ZT_DEFAULTS.rootTopologyUpdateURL.c_str());
						RR->http->GET(ZT_DEFAULTS.rootTopologyUpdateURL,HttpClient::NO_HEADERS,60,&_cbHandleGetRootTopology,RR);
					}
				}
			} catch ( ... ) {
				LOG("unexpected exception attempting to check for root topology updates (non-fatal)");
			}

			// Sleep for loop interval or until something interesting happens.
			try {
				unsigned long delay = std::min((unsigned long)ZT_MAX_SERVICE_LOOP_INTERVAL,RR->sw->doTimerTasks());
				uint64_t start = Utils::now();
				RR->sm->poll(delay,&_CBztTraffic,RR);
				lastDelayDelta = (long)(Utils::now() - start) - (long)delay; // used to detect sleep/wake
			} catch (std::exception &exc) {
				LOG("unexpected exception running Switch doTimerTasks: %s",exc.what());
			} catch ( ... ) {
				LOG("unexpected exception running Switch doTimerTasks: (unknown)");
			}
		}
	} catch ( ... ) {
		LOG("FATAL: unexpected exception in core loop: unknown exception");
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unexpected exception during outer main I/O loop");
	}

	return impl->terminate();
}

const char *Node::terminationMessage() const
	throw()
{
	if ((!((_NodeImpl *)_impl)->started)||(((_NodeImpl *)_impl)->running))
		return (const char *)0;
	return ((_NodeImpl *)_impl)->reasonForTerminationStr.c_str();
}

void Node::terminate(ReasonForTermination reason,const char *reasonText)
	throw()
{
	((_NodeImpl *)_impl)->reasonForTermination = reason;
	((_NodeImpl *)_impl)->reasonForTerminationStr = ((reasonText) ? reasonText : "");
	((_NodeImpl *)_impl)->renv.sm->whack();
}

void Node::resync()
	throw()
{
	((_NodeImpl *)_impl)->resynchronize = true;
	((_NodeImpl *)_impl)->renv.sm->whack();
}

bool Node::online()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);
	if ((!RR)||(!RR->initialized))
		return false;
	uint64_t now = Utils::now();
	uint64_t since = RR->timeOfLastResynchronize;
	std::vector< SharedPtr<Peer> > snp(RR->topology->supernodePeers());
	for(std::vector< SharedPtr<Peer> >::const_iterator sn(snp.begin());sn!=snp.end();++sn) {
		uint64_t lastRec = (*sn)->lastDirectReceive();
		if ((lastRec)&&(lastRec > since)&&((now - lastRec) < ZT_PEER_PATH_ACTIVITY_TIMEOUT))
			return true;
	}
	return false;
}

bool Node::started()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	return impl->started;
}

bool Node::running()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	return impl->running;
}

bool Node::initialized()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);
	return ((RR)&&(RR->initialized));
}

uint64_t Node::address()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);
	if ((!RR)||(!RR->initialized))
		return 0;
	return RR->identity.address().toInt();
}

void Node::join(uint64_t nwid)
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);
	if ((RR)&&(RR->initialized))
		RR->nc->join(nwid);
}

void Node::leave(uint64_t nwid)
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);
	if ((RR)&&(RR->initialized))
		RR->nc->leave(nwid);
}

struct GatherPeerStatistics
{
	uint64_t now;
	ZT1_Node_Status *status;
	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		++status->knownPeers;
		if (p->hasActiveDirectPath(now))
			++status->directlyConnectedPeers;
		if (p->alive(now))
			++status->alivePeers;
	}
};
void Node::status(ZT1_Node_Status *status)
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);

	memset(status,0,sizeof(ZT1_Node_Status));

	if ((!RR)||(!RR->initialized))
		return;

	Utils::scopy(status->publicIdentity,sizeof(status->publicIdentity),RR->identity.toString(false).c_str());
	RR->identity.address().toString(status->address,sizeof(status->address));
	status->rawAddress = RR->identity.address().toInt();

	status->knownPeers = 0;
	status->supernodes = RR->topology->numSupernodes();
	status->directlyConnectedPeers = 0;
	status->alivePeers = 0;
	GatherPeerStatistics gps;
	gps.now = Utils::now();
	gps.status = status;
	RR->topology->eachPeer<GatherPeerStatistics &>(gps);

	if (status->alivePeers > 0) {
		double dlsr = (double)status->directlyConnectedPeers / (double)status->alivePeers;
		if (dlsr > 1.0) dlsr = 1.0;
		if (dlsr < 0.0) dlsr = 0.0;
		status->directLinkSuccessRate = (float)dlsr;
	} else status->directLinkSuccessRate = 1.0f; // no connections to no active peers == 100% success at nothing

	status->online = online();
	status->running = impl->running;
	status->initialized = true;
}

struct CollectPeersAndPaths
{
	std::vector< std::pair< SharedPtr<Peer>,std::vector<Path> > > data;
	inline void operator()(Topology &t,const SharedPtr<Peer> &p) { this->data.push_back(std::pair< SharedPtr<Peer>,std::vector<Path> >(p,p->paths())); }
};
struct SortPeersAndPathsInAscendingAddressOrder
{
	inline bool operator()(const std::pair< SharedPtr<Peer>,std::vector<Path> > &a,const std::pair< SharedPtr<Peer>,std::vector<Path> > &b) const { return (a.first->address() < b.first->address()); }
};
ZT1_Node_PeerList *Node::listPeers()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);

	if ((!RR)||(!RR->initialized))
		return (ZT1_Node_PeerList *)0;

	CollectPeersAndPaths pp;
	RR->topology->eachPeer<CollectPeersAndPaths &>(pp);
	std::sort(pp.data.begin(),pp.data.end(),SortPeersAndPathsInAscendingAddressOrder());

	unsigned int returnBufSize = sizeof(ZT1_Node_PeerList);
	for(std::vector< std::pair< SharedPtr<Peer>,std::vector<Path> > >::iterator p(pp.data.begin());p!=pp.data.end();++p)
		returnBufSize += sizeof(ZT1_Node_Peer) + (sizeof(ZT1_Node_PhysicalPath) * (unsigned int)p->second.size());

	char *buf = (char *)::malloc(returnBufSize);
	if (!buf)
		return (ZT1_Node_PeerList *)0;
	memset(buf,0,returnBufSize);

	ZT1_Node_PeerList *pl = (ZT1_Node_PeerList *)buf;
	buf += sizeof(ZT1_Node_PeerList);

	pl->peers = (ZT1_Node_Peer *)buf;
	buf += (sizeof(ZT1_Node_Peer) * pp.data.size());
	pl->numPeers = 0;

	uint64_t now = Utils::now();
	for(std::vector< std::pair< SharedPtr<Peer>,std::vector<Path> > >::iterator p(pp.data.begin());p!=pp.data.end();++p) {
		ZT1_Node_Peer *prec = &(pl->peers[pl->numPeers++]);
		if (p->first->remoteVersionKnown())
			Utils::snprintf(prec->remoteVersion,sizeof(prec->remoteVersion),"%u.%u.%u",p->first->remoteVersionMajor(),p->first->remoteVersionMinor(),p->first->remoteVersionRevision());
		p->first->address().toString(prec->address,sizeof(prec->address));
		prec->rawAddress = p->first->address().toInt();
		prec->latency = p->first->latency();
		prec->role = RR->topology->isSupernode(p->first->address()) ? ZT1_Node_Peer_SUPERNODE : ZT1_Node_Peer_NODE;

		prec->paths = (ZT1_Node_PhysicalPath *)buf;
		buf += sizeof(ZT1_Node_PhysicalPath) * p->second.size();

		prec->numPaths = 0;
		for(std::vector<Path>::iterator pi(p->second.begin());pi!=p->second.end();++pi) {
			ZT1_Node_PhysicalPath *path = &(prec->paths[prec->numPaths++]);
			path->type = (ZT1_Node_PhysicalPathType)pi->type();
			if (pi->address().isV6()) {
				path->address.type = ZT1_Node_PhysicalAddress_TYPE_IPV6;
				memcpy(path->address.bits,pi->address().rawIpData(),16);
				// TODO: zoneIndex not supported yet, but should be once echo-location works w/V6
			} else {
				path->address.type = ZT1_Node_PhysicalAddress_TYPE_IPV4;
				memcpy(path->address.bits,pi->address().rawIpData(),4);
			}
			path->address.port = pi->address().port();
			Utils::scopy(path->address.ascii,sizeof(path->address.ascii),pi->address().toIpString().c_str());
			path->lastSend = (pi->lastSend() > 0) ? ((long)(now - pi->lastSend())) : (long)-1;
			path->lastReceive = (pi->lastReceived() > 0) ? ((long)(now - pi->lastReceived())) : (long)-1;
			path->lastPing = (pi->lastPing() > 0) ? ((long)(now - pi->lastPing())) : (long)-1;
			path->active = pi->active(now);
			path->fixed = pi->fixed();
		}
	}

	return pl;
}

// Fills out everything but ips[] and numIps, which must be done more manually
static void _fillNetworkQueryResultBuffer(const SharedPtr<Network> &network,const SharedPtr<NetworkConfig> &nconf,ZT1_Node_Network *nbuf)
{
	nbuf->nwid = network->id();
	Utils::snprintf(nbuf->nwidHex,sizeof(nbuf->nwidHex),"%.16llx",(unsigned long long)network->id());
	if (nconf) {
		Utils::scopy(nbuf->name,sizeof(nbuf->name),nconf->name().c_str());
		Utils::scopy(nbuf->description,sizeof(nbuf->description),nconf->description().c_str());
	}
	Utils::scopy(nbuf->device,sizeof(nbuf->device),network->tapDeviceName().c_str());
	Utils::scopy(nbuf->statusStr,sizeof(nbuf->statusStr),Network::statusString(network->status()));
	network->mac().toString(nbuf->macStr,sizeof(nbuf->macStr));
	network->mac().copyTo(nbuf->mac,sizeof(nbuf->mac));
	uint64_t lcu = network->lastConfigUpdate();
	if (lcu > 0)
		nbuf->configAge = (long)(Utils::now() - lcu);
	else nbuf->configAge = -1;
	nbuf->status = (ZT1_Node_NetworkStatus)network->status();
	nbuf->enabled = network->enabled();
	nbuf->isPrivate = (nconf) ? nconf->isPrivate() : true;
}

ZT1_Node_Network *Node::getNetworkStatus(uint64_t nwid)
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);

	if ((!RR)||(!RR->initialized))
		return (ZT1_Node_Network *)0;

	SharedPtr<Network> network(RR->nc->network(nwid));
	if (!network)
		return (ZT1_Node_Network *)0;
	SharedPtr<NetworkConfig> nconf(network->config2());
	std::set<InetAddress> ips(network->ips());

	char *buf = (char *)::malloc(sizeof(ZT1_Node_Network) + (sizeof(ZT1_Node_PhysicalAddress) * ips.size()));
	if (!buf)
		return (ZT1_Node_Network *)0;
	memset(buf,0,sizeof(ZT1_Node_Network) + (sizeof(ZT1_Node_PhysicalAddress) * ips.size()));

	ZT1_Node_Network *nbuf = (ZT1_Node_Network *)buf;
	buf += sizeof(ZT1_Node_Network);

	_fillNetworkQueryResultBuffer(network,nconf,nbuf);

	nbuf->ips = (ZT1_Node_PhysicalAddress *)buf;
	nbuf->numIps = 0;
	for(std::set<InetAddress>::iterator ip(ips.begin());ip!=ips.end();++ip) {
		ZT1_Node_PhysicalAddress *ipb = &(nbuf->ips[nbuf->numIps++]);
		if (ip->isV6()) {
			ipb->type = ZT1_Node_PhysicalAddress_TYPE_IPV6;
			memcpy(ipb->bits,ip->rawIpData(),16);
		} else {
			ipb->type = ZT1_Node_PhysicalAddress_TYPE_IPV4;
			memcpy(ipb->bits,ip->rawIpData(),4);
		}
		ipb->port = ip->port();
		Utils::scopy(ipb->ascii,sizeof(ipb->ascii),ip->toIpString().c_str());
	}

	return nbuf;
}

ZT1_Node_NetworkList *Node::listNetworks()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);

	if ((!RR)||(!RR->initialized))
		return (ZT1_Node_NetworkList *)0;

	std::vector< SharedPtr<Network> > networks(RR->nc->networks());
	std::vector< SharedPtr<NetworkConfig> > nconfs(networks.size());
	std::vector< std::set<InetAddress> > ipsv(networks.size());

	unsigned long returnBufSize = sizeof(ZT1_Node_NetworkList);
	for(unsigned long i=0;i<networks.size();++i) {
		nconfs[i] = networks[i]->config2(); // note: can return NULL
		ipsv[i] = networks[i]->ips();
		returnBufSize += sizeof(ZT1_Node_Network) + (sizeof(ZT1_Node_PhysicalAddress) * (unsigned int)ipsv[i].size());
	}

	char *buf = (char *)::malloc(returnBufSize);
	if (!buf)
		return (ZT1_Node_NetworkList *)0;
	memset(buf,0,returnBufSize);

	ZT1_Node_NetworkList *nl = (ZT1_Node_NetworkList *)buf;
	buf += sizeof(ZT1_Node_NetworkList);

	nl->networks = (ZT1_Node_Network *)buf;
	buf += sizeof(ZT1_Node_Network) * networks.size();

	for(unsigned long i=0;i<networks.size();++i) {
		ZT1_Node_Network *nbuf = &(nl->networks[nl->numNetworks++]);

		_fillNetworkQueryResultBuffer(networks[i],nconfs[i],nbuf);

		nbuf->ips = (ZT1_Node_PhysicalAddress *)buf;
		buf += sizeof(ZT1_Node_PhysicalAddress) * ipsv[i].size();

		nbuf->numIps = 0;
		for(std::set<InetAddress>::iterator ip(ipsv[i].begin());ip!=ipsv[i].end();++ip) {
			ZT1_Node_PhysicalAddress *ipb = &(nbuf->ips[nbuf->numIps++]);
			if (ip->isV6()) {
				ipb->type = ZT1_Node_PhysicalAddress_TYPE_IPV6;
				memcpy(ipb->bits,ip->rawIpData(),16);
			} else {
				ipb->type = ZT1_Node_PhysicalAddress_TYPE_IPV4;
				memcpy(ipb->bits,ip->rawIpData(),4);
			}
			ipb->port = ip->port();
			Utils::scopy(ipb->ascii,sizeof(ipb->ascii),ip->toIpString().c_str());
		}
	}

	return nl;
}

void Node::freeQueryResult(void *qr)
	throw()
{
	if (qr)
		::free(qr);
}

bool Node::updateCheck()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *RR = (RuntimeEnvironment *)&(impl->renv);
	if (RR->updater) {
		RR->updater->checkNow();
		return true;
	}
	return false;
}

class _VersionStringMaker
{
public:
	char vs[32];
	_VersionStringMaker()
	{
		Utils::snprintf(vs,sizeof(vs),"%d.%d.%d",(int)ZEROTIER_ONE_VERSION_MAJOR,(int)ZEROTIER_ONE_VERSION_MINOR,(int)ZEROTIER_ONE_VERSION_REVISION);
	}
	~_VersionStringMaker() {}
};
static const _VersionStringMaker __versionString;

const char *Node::versionString() throw() { return __versionString.vs; }

unsigned int Node::versionMajor() throw() { return ZEROTIER_ONE_VERSION_MAJOR; }
unsigned int Node::versionMinor() throw() { return ZEROTIER_ONE_VERSION_MINOR; }
unsigned int Node::versionRevision() throw() { return ZEROTIER_ONE_VERSION_REVISION; }

} // namespace ZeroTier
