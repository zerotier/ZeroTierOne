/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <list>
#include <vector>
#include <string>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/file.h>
#endif

#include <openssl/sha.h>

#include "Condition.hpp"
#include "Node.hpp"
#include "Topology.hpp"
#include "Demarc.hpp"
#include "Switch.hpp"
#include "Utils.hpp"
#include "EthernetTap.hpp"
#include "Logger.hpp"
#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Pack.hpp"
#include "RuntimeEnvironment.hpp"
#include "NodeConfig.hpp"
#include "Defaults.hpp"
#include "SysEnv.hpp"
#include "Network.hpp"
#include "MulticastGroup.hpp"
#include "Mutex.hpp"

#include "../version.h"

namespace ZeroTier {

struct _NodeImpl
{
	RuntimeEnvironment renv;
	std::string reasonForTerminationStr;
	Node::ReasonForTermination reasonForTermination;
	volatile bool started;
	volatile bool running;
	volatile bool updateStatusNow;
	volatile bool terminateNow;

	// Helper used to rapidly terminate from run()
	inline Node::ReasonForTermination terminateBecause(Node::ReasonForTermination r,const char *rstr)
	{
		RuntimeEnvironment *_r = &renv;
		LOG("terminating: %s",rstr);

		reasonForTerminationStr = rstr;
		reasonForTermination = r;
		running = false;
		return r;
	}
};

Node::Node(const char *hp,const char *urlPrefix,const char *configAuthorityIdentity)
	throw() :
	_impl(new _NodeImpl)
{
	_NodeImpl *impl = (_NodeImpl *)_impl;

	impl->renv.homePath = hp;
	impl->renv.autoconfUrlPrefix = urlPrefix;
	impl->renv.configAuthorityIdentityStr = configAuthorityIdentity;

	impl->reasonForTermination = Node::NODE_RUNNING;
	impl->started = false;
	impl->running = false;
	impl->updateStatusNow = false;
	impl->terminateNow = false;
}

Node::~Node()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;

	delete impl->renv.sysEnv;
	delete impl->renv.topology;
	delete impl->renv.sw;
	delete impl->renv.demarc;
	delete impl->renv.nc;
	delete impl->renv.log;

	delete impl;
}

/**
 * Execute node in current thread
 *
 * This does not return until the node shuts down. Shutdown may be caused
 * by an internally detected condition such as a new upgrade being
 * available or a fatal error, or it may be signaled externally using
 * the terminate() method.
 *
 * @return Reason for termination
 */
Node::ReasonForTermination Node::run()
	throw()
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	RuntimeEnvironment *_r = (RuntimeEnvironment *)&(impl->renv);

	impl->started = true;
	impl->running = true;

	try {
#ifdef ZT_LOG_STDOUT
		_r->log = new Logger((const char *)0,(const char *)0,0);
#else
		_r->log = new Logger((_r->homePath + ZT_PATH_SEPARATOR_S + "node.log").c_str(),(const char *)0,131072);
#endif

		TRACE("initializing...");

		if (!_r->configAuthority.fromString(_r->configAuthorityIdentityStr))
			return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"configuration authority identity is not valid");

		bool gotId = false;
		std::string identitySecretPath(_r->homePath + ZT_PATH_SEPARATOR_S + "identity.secret");
		std::string identityPublicPath(_r->homePath + ZT_PATH_SEPARATOR_S + "identity.public");
		std::string idser;
		if (Utils::readFile(identitySecretPath.c_str(),idser))
			gotId = _r->identity.fromString(idser);
		if (gotId) {
			// Make sure identity.public matches identity.secret
			idser = std::string();
			Utils::readFile(identityPublicPath.c_str(),idser);
			std::string pubid(_r->identity.toString(false));
			if (idser != pubid) {
				if (!Utils::writeFile(identityPublicPath.c_str(),pubid))
					return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write identity.public (home path not writable?)");
			}
		} else {
			LOG("no identity found, generating one... this might take a few seconds...");
			_r->identity.generate();
			LOG("generated new identity: %s",_r->identity.address().toString().c_str());
			idser = _r->identity.toString(true);
			if (!Utils::writeFile(identitySecretPath.c_str(),idser))
				return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write identity.secret (home path not writable?)");
			idser = _r->identity.toString(false);
			if (!Utils::writeFile(identityPublicPath.c_str(),idser))
				return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write identity.public (home path not writable?)");
		}
		Utils::lockDownFile(identitySecretPath.c_str(),false);

		// Generate ownership verification secret, which can be presented to
		// a controlling web site (like ours) to prove ownership of a node and
		// permit its configuration to be centrally modified. When ZeroTier One
		// requests its config it sends a hash of this secret, and so the
		// config server can verify this hash to determine if the secret the
		// user presents is correct.
		std::string ovsPath(_r->homePath + ZT_PATH_SEPARATOR_S + "thisdeviceismine");
		if (((Utils::now() - Utils::getLastModified(ovsPath.c_str())) >= ZT_OVS_GENERATE_NEW_IF_OLDER_THAN)||(!Utils::readFile(ovsPath.c_str(),_r->ownershipVerificationSecret))) {
			_r->ownershipVerificationSecret = "";
			for(unsigned int i=0;i<24;++i)
				_r->ownershipVerificationSecret.push_back("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[Utils::randomInt<unsigned int>() % 62]);
			_r->ownershipVerificationSecret.append(ZT_EOL_S);
			if (!Utils::writeFile(ovsPath.c_str(),_r->ownershipVerificationSecret))
				return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write 'thisdeviceismine' (home path not writable?)");
		}
		Utils::lockDownFile(ovsPath.c_str(),false);
		_r->ownershipVerificationSecret = Utils::trim(_r->ownershipVerificationSecret); // trim off CR file is saved with
		unsigned char ovsDig[32];
		SHA256_CTX sha;
		SHA256_Init(&sha);
		SHA256_Update(&sha,_r->ownershipVerificationSecret.data(),_r->ownershipVerificationSecret.length());
		SHA256_Final(ovsDig,&sha);
		_r->ownershipVerificationSecretHash = Utils::base64Encode(ovsDig,32);

		// Create the core objects in RuntimeEnvironment: node config, demarcation
		// point, switch, network topology database, and system environment
		// watcher.
		_r->nc = new NodeConfig(_r,_r->autoconfUrlPrefix + _r->identity.address().toString());
		_r->demarc = new Demarc(_r);
		_r->sw = new Switch(_r);
		_r->topology = new Topology(_r,(_r->homePath + ZT_PATH_SEPARATOR_S + "peer.db").c_str());
		_r->sysEnv = new SysEnv(_r);

		// TODO: make configurable
		bool boundPort = false;
		for(unsigned int p=ZT_DEFAULT_UDP_PORT;p<(ZT_DEFAULT_UDP_PORT + 128);++p) {
			if (_r->demarc->bindLocalUdp(p)) {
				boundPort = true;
				break;
			}
		}
		if (!boundPort)
			return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not bind any local UDP ports");

		// TODO: bootstrap off network so we don't have to update code for
		// changes in supernodes.
		_r->topology->setSupernodes(ZT_DEFAULTS.supernodes);
	} catch (std::bad_alloc &exc) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"memory allocation failure");
	} catch (std::runtime_error &exc) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,exc.what());
	} catch ( ... ) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unknown exception during initialization");
	}

	try {
		std::string statusPath(_r->homePath + ZT_PATH_SEPARATOR_S + "status");

		uint64_t lastPingCheck = 0;
		uint64_t lastTopologyClean = Utils::now(); // don't need to do this immediately
		uint64_t lastNetworkFingerprintCheck = 0;
		uint64_t lastAutoconfigureCheck = 0;
		uint64_t networkConfigurationFingerprint = _r->sysEnv->getNetworkConfigurationFingerprint();
		uint64_t lastMulticastCheck = 0;
		uint64_t lastMulticastAnnounceAll = 0;
		uint64_t lastStatusUpdate = 0;
		long lastDelayDelta = 0;

		LOG("%s starting version %s",_r->identity.address().toString().c_str(),versionString());

		while (!impl->terminateNow) {
			uint64_t now = Utils::now();
			bool pingAll = false; // set to true to force a ping of *all* known direct links

			// Detect sleep/wake by looking for delay loop pauses that are longer
			// than we intended to pause.
			if (lastDelayDelta >= ZT_SLEEP_WAKE_DETECTION_THRESHOLD) {
				lastNetworkFingerprintCheck = 0; // force network environment check
				lastMulticastCheck = 0; // force multicast group check on taps
				pingAll = true;

				LOG("probable suspend/resume detected, pausing a moment for things to settle...");
				Thread::sleep(ZT_SLEEP_WAKE_SETTLE_TIME);
			}

			// Periodically check our network environment, sending pings out to all
			// our direct links if things look like we got a different address.
			if ((now - lastNetworkFingerprintCheck) >= ZT_NETWORK_FINGERPRINT_CHECK_DELAY) {
				lastNetworkFingerprintCheck = now;
				uint64_t fp = _r->sysEnv->getNetworkConfigurationFingerprint();
				if (fp != networkConfigurationFingerprint) {
					LOG("netconf fingerprint change: %.16llx != %.16llx, resyncing with network",networkConfigurationFingerprint,fp);
					networkConfigurationFingerprint = fp;
					pingAll = true;
					lastAutoconfigureCheck = 0; // check autoconf after network config change
					lastMulticastCheck = 0; // check multicast group membership after network config change
					_r->nc->whackAllTaps(); // call whack() on all tap devices
				}
			}

			if ((now - lastAutoconfigureCheck) >= ZT_AUTOCONFIGURE_CHECK_DELAY) {
				// It seems odd to only do this simple check every so often, but the purpose is to
				// delay between calls to refreshConfiguration() enough that the previous attempt
				// has time to either succeed or fail. Otherwise we'll block the whole loop, since
				// config update is guarded by a Mutex.
				lastAutoconfigureCheck = now;
				if ((now - _r->nc->lastAutoconfigure()) >= ZT_AUTOCONFIGURE_INTERVAL)
					_r->nc->refreshConfiguration(); // happens in background
			}

			// Periodically check for changes in our local multicast subscriptions and broadcast
			// those changes to peers.
			if ((now - lastMulticastCheck) >= ZT_MULTICAST_LOCAL_POLL_PERIOD) {
				lastMulticastCheck = now;
				bool announceAll = ((now - lastMulticastAnnounceAll) >= ZT_MULTICAST_LIKE_ANNOUNCE_ALL_PERIOD);
				try {
					std::map< SharedPtr<Network>,std::set<MulticastGroup> > toAnnounce;
					{
						std::vector< SharedPtr<Network> > networks(_r->nc->networks());
						for(std::vector< SharedPtr<Network> >::const_iterator nw(networks.begin());nw!=networks.end();++nw) {
							if (((*nw)->updateMulticastGroups())||(announceAll))
								toAnnounce.insert(std::pair< SharedPtr<Network>,std::set<MulticastGroup> >(*nw,(*nw)->multicastGroups()));
						}
					}

					if (toAnnounce.size()) {
						_r->sw->announceMulticastGroups(toAnnounce);

						// Only update lastMulticastAnnounceAll if we've announced something. This keeps
						// the announceAll condition true during startup when there are no multicast
						// groups until there is at least one. Technically this shouldn't be required as
						// updateMulticastGroups() should return true on any change, but why not?
						if (announceAll)
							lastMulticastAnnounceAll = now;
					}
				} catch (std::exception &exc) {
					LOG("unexpected exception announcing multicast groups: %s",exc.what());
				} catch ( ... ) {
					LOG("unexpected exception announcing multicast groups: (unknown)");
				}
			}

			if ((now - lastPingCheck) >= ZT_PING_CHECK_DELAY) {
				lastPingCheck = now;
				try {
					if (_r->topology->isSupernode(_r->identity.address())) {
						// The only difference in how supernodes behave is here: they only
						// actively ping each other and only passively listen for pings
						// from anyone else. They also don't send firewall openers, since
						// they're never firewalled.
						std::vector< SharedPtr<Peer> > sns(_r->topology->supernodePeers());
						for(std::vector< SharedPtr<Peer> >::const_iterator p(sns.begin());p!=sns.end();++p) {
							if ((now - (*p)->lastDirectSend()) > ZT_PEER_DIRECT_PING_DELAY)
								_r->sw->sendHELLO((*p)->address());
						}
					} else {
						std::vector< SharedPtr<Peer> > needPing,needFirewallOpener;

						if (pingAll) {
							_r->topology->eachPeer(Topology::CollectPeersWithActiveDirectPath(needPing));
						} else {
							_r->topology->eachPeer(Topology::CollectPeersThatNeedPing(needPing));
							_r->topology->eachPeer(Topology::CollectPeersThatNeedFirewallOpener(needFirewallOpener));
						}

						for(std::vector< SharedPtr<Peer> >::iterator p(needPing.begin());p!=needPing.end();++p) {
							try {
								_r->sw->sendHELLO((*p)->address());
							} catch (std::exception &exc) {
								LOG("unexpected exception sending HELLO to %s: %s",(*p)->address().toString().c_str());
							} catch ( ... ) {
								LOG("unexpected exception sending HELLO to %s: (unknown)",(*p)->address().toString().c_str());
							}
						}

						for(std::vector< SharedPtr<Peer> >::iterator p(needFirewallOpener.begin());p!=needFirewallOpener.end();++p) {
							try {
								(*p)->sendFirewallOpener(_r,now);
							} catch (std::exception &exc) {
								LOG("unexpected exception sending firewall opener to %s: %s",(*p)->address().toString().c_str(),exc.what());
							} catch ( ... ) {
								LOG("unexpected exception sending firewall opener to %s: (unknown)",(*p)->address().toString().c_str());
							}
						}
					}
				} catch (std::exception &exc) {
					LOG("unexpected exception running ping check cycle: %s",exc.what());
				} catch ( ... ) {
					LOG("unexpected exception running ping check cycle: (unkonwn)");
				}
			}

			if ((now - lastTopologyClean) >= ZT_TOPOLOGY_CLEAN_PERIOD) {
				lastTopologyClean = now;
				_r->topology->clean(); // happens in background
			}

			if (((now - lastStatusUpdate) >= ZT_STATUS_OUTPUT_PERIOD)||(impl->updateStatusNow)) {
				lastStatusUpdate = now;
				impl->updateStatusNow = false;
				FILE *statusf = ::fopen(statusPath.c_str(),"w");
				if (statusf) {
					try {
						_r->topology->eachPeer(Topology::DumpPeerStatistics(statusf));
					} catch ( ... ) {
						TRACE("unexpected exception updating status dump");
					}
					::fclose(statusf);
				}
			}

			try {
				unsigned long delay = std::min((unsigned long)ZT_MIN_SERVICE_LOOP_INTERVAL,_r->sw->doTimerTasks());
				uint64_t start = Utils::now();
				Thread::sleep(delay);
				lastDelayDelta = (long)(Utils::now() - start) - (long)delay;
			} catch (std::exception &exc) {
				LOG("unexpected exception running Switch doTimerTasks: %s",exc.what());
			} catch ( ... ) {
				LOG("unexpected exception running Switch doTimerTasks: (unknown)");
			}
		}
	} catch ( ... ) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unexpected exception during outer main I/O loop");
	}

	return impl->terminateBecause(Node::NODE_NORMAL_TERMINATION,"normal termination");
}

const char *Node::reasonForTermination() const
	throw()
{
	if ((!((_NodeImpl *)_impl)->started)||(((_NodeImpl *)_impl)->running))
		return (const char *)0;
	return ((_NodeImpl *)_impl)->reasonForTerminationStr.c_str();
}

void Node::terminate()
	throw()
{
	((_NodeImpl *)_impl)->terminateNow = true;
}

void Node::updateStatusNow()
	throw()
{
	((_NodeImpl *)_impl)->updateStatusNow = true;
}

class _VersionStringMaker
{
public:
	char vs[32];
	_VersionStringMaker()
	{
		sprintf(vs,"%d.%d.%d",(int)ZEROTIER_ONE_VERSION_MAJOR,(int)ZEROTIER_ONE_VERSION_MINOR,(int)ZEROTIER_ONE_VERSION_REVISION);
	}
	~_VersionStringMaker() {}
};
static const _VersionStringMaker __versionString;

const char *Node::versionString() throw() { return __versionString.vs; }

unsigned int Node::versionMajor() throw() { return ZEROTIER_ONE_VERSION_MAJOR; }
unsigned int Node::versionMinor() throw() { return ZEROTIER_ONE_VERSION_MINOR; }
unsigned int Node::versionRevision() throw() { return ZEROTIER_ONE_VERSION_REVISION; }

// Scanned for by loader and/or updater to determine a binary's version
const unsigned char EMBEDDED_VERSION_STAMP[20] = {
	0x6d,0xfe,0xff,0x01,0x90,0xfa,0x89,0x57,0x88,0xa1,0xaa,0xdc,0xdd,0xde,0xb0,0x33,
	ZEROTIER_ONE_VERSION_MAJOR,
	ZEROTIER_ONE_VERSION_MINOR,
	(unsigned char)(((unsigned int)ZEROTIER_ONE_VERSION_REVISION) & 0xff), /* little-endian */
	(unsigned char)((((unsigned int)ZEROTIER_ONE_VERSION_REVISION) >> 8) & 0xff)
};

} // namespace ZeroTier
