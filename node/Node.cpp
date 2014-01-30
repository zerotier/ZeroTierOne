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
#endif

#ifdef __UNIX_LIKE__
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/file.h>
#endif

#include "Condition.hpp"
#include "Node.hpp"
#include "Topology.hpp"
#include "Demarc.hpp"
#include "Packet.hpp"
#include "Switch.hpp"
#include "Utils.hpp"
#include "EthernetTap.hpp"
#include "Logger.hpp"
#include "InetAddress.hpp"
#include "Salsa20.hpp"
#include "RuntimeEnvironment.hpp"
#include "NodeConfig.hpp"
#include "Defaults.hpp"
#include "SysEnv.hpp"
#include "Network.hpp"
#include "MulticastGroup.hpp"
#include "Mutex.hpp"
#include "Multicaster.hpp"
#include "CMWC4096.hpp"
#include "SHA512.hpp"
#include "Service.hpp"
#include "SoftwareUpdater.hpp"

#include "../version.h"

namespace ZeroTier {

struct _LocalClientImpl
{
	unsigned char key[32];
	UdpSocket *sock;
	void (*resultHandler)(void *,unsigned long,const char *);
	void *arg;
	unsigned int controlPort;
	InetAddress localDestAddr;
	Mutex inUseLock;
};

static void _CBlocalClientHandler(UdpSocket *sock,void *arg,const InetAddress &remoteAddr,const void *data,unsigned int len)
{
	_LocalClientImpl *impl = (_LocalClientImpl *)arg;
	if (!impl)
		return;
	if (!impl->resultHandler)
		return; // sanity check
	Mutex::Lock _l(impl->inUseLock);

	try {
		unsigned long convId = 0;
		std::vector<std::string> results;
		if (!NodeConfig::decodeControlMessagePacket(impl->key,data,len,convId,results))
			return;
		for(std::vector<std::string>::iterator r(results.begin());r!=results.end();++r)
			impl->resultHandler(impl->arg,convId,r->c_str());
	} catch ( ... ) {}
}

Node::LocalClient::LocalClient(const char *authToken,unsigned int controlPort,void (*resultHandler)(void *,unsigned long,const char *),void *arg)
	throw() :
	_impl((void *)0)
{
	_LocalClientImpl *impl = new _LocalClientImpl;

	UdpSocket *sock = (UdpSocket *)0;
	for(unsigned int i=0;i<5000;++i) {
		try {
			sock = new UdpSocket(true,32768 + (rand() % 20000),false,&_CBlocalClientHandler,impl);
			break;
		} catch ( ... ) {
			sock = (UdpSocket *)0;
		}
	}

	// If socket fails to bind, there's a big problem like missing IPv4 stack
	if (sock) {
		{
			unsigned int csk[64];
			SHA512::hash(csk,authToken,(unsigned int)strlen(authToken));
			memcpy(impl->key,csk,32);
		}

		impl->sock = sock;
		impl->resultHandler = resultHandler;
		impl->arg = arg;
		impl->controlPort = (controlPort) ? controlPort : (unsigned int)ZT_DEFAULT_CONTROL_UDP_PORT;
		impl->localDestAddr = InetAddress::LO4;
		impl->localDestAddr.setPort(impl->controlPort);
		_impl = impl;
	} else delete impl;
}

Node::LocalClient::~LocalClient()
{
	if (_impl) {
		((_LocalClientImpl *)_impl)->inUseLock.lock();
		delete ((_LocalClientImpl *)_impl)->sock;
		((_LocalClientImpl *)_impl)->inUseLock.unlock();
		delete ((_LocalClientImpl *)_impl);
	}
}

unsigned long Node::LocalClient::send(const char *command)
	throw()
{
	if (!_impl)
		return 0;
	_LocalClientImpl *impl = (_LocalClientImpl *)_impl;
	Mutex::Lock _l(impl->inUseLock);

	try {
		uint32_t convId = (uint32_t)rand();
		if (!convId)
			convId = 1;

		std::vector<std::string> tmp;
		tmp.push_back(std::string(command));
		std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > packets(NodeConfig::encodeControlMessage(impl->key,convId,tmp));

		for(std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> >::iterator p(packets.begin());p!=packets.end();++p)
			impl->sock->send(impl->localDestAddr,p->data(),p->size(),-1);

		return convId;
	} catch ( ... ) {
		return 0;
	}
}

std::vector<std::string> Node::LocalClient::splitLine(const char *line)
{
	return Utils::split(line," ","\\","\"");
}

std::string Node::LocalClient::authTokenDefaultUserPath()
{
#ifdef __WINDOWS__

	char buf[16384];
	if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_APPDATA,NULL,0,buf)))
		return (std::string(buf) + "\\ZeroTier\\One\\authtoken.secret");
	else return std::string();

#else // not __WINDOWS__

	const char *home = getenv("HOME");
	if (home) {
#ifdef __APPLE__
		return (std::string(home) + "/Library/Application Support/ZeroTier/One/authtoken.secret");
#else
		return (std::string(home) + "/.zeroTierOneAuthToken");
#endif
	} else return std::string();

#endif // __WINDOWS__ or not __WINDOWS__
}

std::string Node::LocalClient::authTokenDefaultSystemPath()
{
	return (ZT_DEFAULTS.defaultHomePath + ZT_PATH_SEPARATOR_S"authtoken.secret");
}

struct _NodeImpl
{
	RuntimeEnvironment renv;
	unsigned int port;
	unsigned int controlPort;
	std::string reasonForTerminationStr;
	volatile Node::ReasonForTermination reasonForTermination;
	volatile bool started;
	volatile bool running;
	volatile bool resynchronize;

	inline Node::ReasonForTermination terminate()
	{
		RuntimeEnvironment *_r = &renv;
		LOG("terminating: %s",reasonForTerminationStr.c_str());

		renv.shutdownInProgress = true;
		Thread::sleep(500);

		running = false;

#ifndef __WINDOWS__
		delete renv.netconfService;
#endif
		delete renv.updater;
		delete renv.nc;
		delete renv.sysEnv;
		delete renv.topology;
		delete renv.demarc;
		delete renv.sw;
		delete renv.mc;
		delete renv.prng;
		delete renv.log;

		return reasonForTermination;
	}

	inline Node::ReasonForTermination terminateBecause(Node::ReasonForTermination r,const char *rstr)
	{
		reasonForTerminationStr = rstr;
		reasonForTermination = r;
		return terminate();
	}
};

#ifndef __WINDOWS__
static void _netconfServiceMessageHandler(void *renv,Service &svc,const Dictionary &msg)
{
	if (!renv)
		return; // sanity check
	const RuntimeEnvironment *_r = (const RuntimeEnvironment *)renv;

	try {
		//TRACE("from netconf:\n%s",msg.toString().c_str());
		const std::string &type = msg.get("type");
		if (type == "ready") {
			LOG("received 'ready' from netconf.service, sending netconf-init with identity information...");
			Dictionary initMessage;
			initMessage["type"] = "netconf-init";
			initMessage["netconfId"] = _r->identity.toString(true);
			_r->netconfService->send(initMessage);
		} else if (type == "netconf-response") {
			uint64_t inRePacketId = strtoull(msg.get("requestId").c_str(),(char **)0,16);
			uint64_t nwid = strtoull(msg.get("nwid").c_str(),(char **)0,16);
			Address peerAddress(msg.get("peer").c_str());

			if (peerAddress) {
				if (msg.contains("error")) {
					Packet::ErrorCode errCode = Packet::ERROR_INVALID_REQUEST;
					const std::string &err = msg.get("error");
					if (err == "OBJ_NOT_FOUND")
						errCode = Packet::ERROR_OBJ_NOT_FOUND;
					else if (err == "ACCESS_DENIED")
						errCode = Packet::ERROR_NETWORK_ACCESS_DENIED_;

					Packet outp(peerAddress,_r->identity.address(),Packet::VERB_ERROR);
					outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
					outp.append(inRePacketId);
					outp.append((unsigned char)errCode);
					outp.append(nwid);
					_r->sw->send(outp,true);
				} else if (msg.contains("netconf")) {
					const std::string &netconf = msg.get("netconf");
					if (netconf.length() < 2048) { // sanity check
						Packet outp(peerAddress,_r->identity.address(),Packet::VERB_OK);
						outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
						outp.append(inRePacketId);
						outp.append(nwid);
						outp.append((uint16_t)netconf.length());
						outp.append(netconf.data(),netconf.length());
						outp.compress();
						_r->sw->send(outp,true);
					}
				}
			}
		} else if (type == "netconf-push") {
			if (msg.contains("to")) {
				Dictionary to(msg.get("to")); // key: peer address, value: comma-delimited network list
				for(Dictionary::iterator t(to.begin());t!=to.end();++t) {
					Address ztaddr(t->first);
					if (ztaddr) {
						Packet outp(ztaddr,_r->identity.address(),Packet::VERB_NETWORK_CONFIG_REFRESH);

						char *saveptr = (char *)0;
						// Note: this loop trashes t->second, which is quasi-legal C++ but
						// shouldn't break anything as long as we don't try to use 'to'
						// for anything interesting after doing this.
						for(char *p=Utils::stok(const_cast<char *>(t->second.c_str()),",",&saveptr);(p);p=Utils::stok((char *)0,",",&saveptr)) {
							uint64_t nwid = Utils::hexStrToU64(p);
							if (nwid) {
								if ((outp.size() + sizeof(uint64_t)) >= ZT_UDP_DEFAULT_PAYLOAD_MTU) {
									_r->sw->send(outp,true);
									outp.reset(ztaddr,_r->identity.address(),Packet::VERB_NETWORK_CONFIG_REFRESH);
								}
								outp.append(nwid);
							}
						}

						if (outp.payloadLength())
							_r->sw->send(outp,true);
					}
				}
			}
		}
	} catch (std::exception &exc) {
		LOG("unexpected exception parsing response from netconf service: %s",exc.what());
	} catch ( ... ) {
		LOG("unexpected exception parsing response from netconf service: unknown exception");
	}
}
#endif // !__WINDOWS__

Node::Node(const char *hp,unsigned int port,unsigned int controlPort)
	throw() :
	_impl(new _NodeImpl)
{
	_NodeImpl *impl = (_NodeImpl *)_impl;
	if ((hp)&&(strlen(hp) > 0))
		impl->renv.homePath = hp;
	else impl->renv.homePath = ZT_DEFAULTS.defaultHomePath;
	impl->port = (port) ? port : (unsigned int)ZT_DEFAULT_UDP_PORT;
	impl->controlPort = (controlPort) ? controlPort : (unsigned int)ZT_DEFAULT_CONTROL_UDP_PORT;
	impl->reasonForTermination = Node::NODE_RUNNING;
	impl->started = false;
	impl->running = false;
	impl->resynchronize = false;
}

Node::~Node()
{
	delete (_NodeImpl *)_impl;
}

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

		LOG("starting version %s",versionString());

		// Create non-crypto PRNG right away in case other code in init wants to use it
		_r->prng = new CMWC4096();

		bool gotId = false;
		std::string identitySecretPath(_r->homePath + ZT_PATH_SEPARATOR_S + "identity.secret");
		std::string identityPublicPath(_r->homePath + ZT_PATH_SEPARATOR_S + "identity.public");
		std::string idser;
		if (Utils::readFile(identitySecretPath.c_str(),idser))
			gotId = _r->identity.fromString(idser);
		if ((gotId)&&(!_r->identity.locallyValidate()))
			gotId = false;
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
			LOG("no identity found or identity invalid, generating one... this might take a few seconds...");
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

		// Make sure networks.d exists
		{
			std::string networksDotD(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d");
#ifdef __WINDOWS__
			CreateDirectoryA(networksDotD.c_str(),NULL);
#else
			mkdir(networksDotD.c_str(),0700);
#endif
		}

		// Load or generate config authentication secret
		std::string configAuthTokenPath(_r->homePath + ZT_PATH_SEPARATOR_S + "authtoken.secret");
		std::string configAuthToken;
		if (!Utils::readFile(configAuthTokenPath.c_str(),configAuthToken)) {
			configAuthToken = "";
			unsigned int sr = 0;
			for(unsigned int i=0;i<24;++i) {
				Utils::getSecureRandom(&sr,sizeof(sr));
				configAuthToken.push_back("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[sr % 62]);
			}
			if (!Utils::writeFile(configAuthTokenPath.c_str(),configAuthToken))
				return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"could not write authtoken.secret (home path not writable?)");
		}
		Utils::lockDownFile(configAuthTokenPath.c_str(),false);

		// Create the objects that make up runtime state.
		_r->mc = new Multicaster();
		_r->sw = new Switch(_r);
		_r->demarc = new Demarc(_r);
		_r->topology = new Topology(_r,Utils::fileExists((_r->homePath + ZT_PATH_SEPARATOR_S + "iddb.d").c_str()));
		_r->sysEnv = new SysEnv();
		try {
			_r->nc = new NodeConfig(_r,configAuthToken.c_str(),impl->controlPort);
		} catch (std::exception &exc) {
			char foo[1024];
			Utils::snprintf(foo,sizeof(foo),"unable to bind to local control port %u: is another instance of ZeroTier One already running?",impl->controlPort);
			return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,foo);
		}
		_r->node = this;
#ifdef ZT_AUTO_UPDATE
		if (ZT_DEFAULTS.updateLatestNfoURL.length()) {
			_r->updater = new SoftwareUpdater(_r);
		} else {
			LOG("WARNING: unable to enable software updates: latest .nfo URL from ZT_DEFAULTS is empty (does this platform actually support software updates?)");
		}
#endif

		// Bind local port for core I/O
		if (!_r->demarc->bindLocalUdp(impl->port)) {
			char foo[1024];
			Utils::snprintf(foo,sizeof(foo),"unable to bind to global I/O port %u: is another instance of ZeroTier One already running?",impl->controlPort);
			return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,foo);
		}

		// Set initial supernode list
		_r->topology->setSupernodes(ZT_DEFAULTS.supernodes);
	} catch (std::bad_alloc &exc) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"memory allocation failure");
	} catch (std::runtime_error &exc) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,exc.what());
	} catch ( ... ) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unknown exception during initialization");
	}

	// Start external service subprocesses, which is only used by special nodes
	// right now and isn't available on Windows.
#ifndef __WINDOWS__
	try {
		std::string netconfServicePath(_r->homePath + ZT_PATH_SEPARATOR_S + "services.d" + ZT_PATH_SEPARATOR_S + "netconf.service");
		if (Utils::fileExists(netconfServicePath.c_str())) {
			LOG("netconf.d/netconf.service appears to exist, starting...");
			_r->netconfService = new Service(_r,"netconf",netconfServicePath.c_str(),&_netconfServiceMessageHandler,_r);
			Dictionary initMessage;
			initMessage["type"] = "netconf-init";
			initMessage["netconfId"] = _r->identity.toString(true);
			_r->netconfService->send(initMessage);
		}
	} catch ( ... ) {
		LOG("unexpected exception attempting to start services");
	}
#endif

	// Core I/O loop
	try {
		/* Shut down if this file exists but fails to open. This is used on Mac to
		 * shut down automatically on .app deletion by symlinking this to the
		 * Info.plist file inside the ZeroTier One application. This causes the
		 * service to die when the user throws away the app, allowing uninstallation
		 * in the natural Mac way. */
		std::string shutdownIfUnreadablePath(_r->homePath + ZT_PATH_SEPARATOR_S + "shutdownIfUnreadable");

		// TODO: persist some of this stuff between restarts
		uint64_t lastNetworkAutoconfCheck = Utils::now() - 5000; // check autoconf again after 5s for startup
		uint64_t lastPingCheck = 0;
		uint64_t lastSupernodePing = 0;
		uint64_t lastClean = Utils::now(); // don't need to do this immediately
		uint64_t lastNetworkFingerprintCheck = 0;
		uint64_t lastMulticastCheck = 0;

		uint64_t networkConfigurationFingerprint = _r->sysEnv->getNetworkConfigurationFingerprint(_r->nc->networkTapDeviceNames());
		long lastDelayDelta = 0;

		while (impl->reasonForTermination == NODE_RUNNING) {
			if (Utils::fileExists(shutdownIfUnreadablePath.c_str(),false)) {
				FILE *tmpf = fopen(shutdownIfUnreadablePath.c_str(),"r");
				if (!tmpf)
					return impl->terminateBecause(Node::NODE_NORMAL_TERMINATION,"shutdownIfUnreadable exists but is not readable");
				fclose(tmpf);
			}

			uint64_t now = Utils::now();
			bool resynchronize = impl->resynchronize;
			if (resynchronize) {
				LOG("manual resynchronize ordered, resyncing with network");
			}
			impl->resynchronize = false;

			// If it looks like the computer slept and woke, resynchronize.
			if (lastDelayDelta >= ZT_SLEEP_WAKE_DETECTION_THRESHOLD) {
				resynchronize = true;
				LOG("probable suspend/resume detected, pausing a moment for things to settle...");
				Thread::sleep(ZT_SLEEP_WAKE_SETTLE_TIME);
			}

			// If our network environment looks like it changed, also set resynchronize flag.
			if ((resynchronize)||((now - lastNetworkFingerprintCheck) >= ZT_NETWORK_FINGERPRINT_CHECK_DELAY)) {
				lastNetworkFingerprintCheck = now;
				uint64_t fp = _r->sysEnv->getNetworkConfigurationFingerprint(_r->nc->networkTapDeviceNames());
				if (fp != networkConfigurationFingerprint) {
					LOG("netconf fingerprint change: %.16llx != %.16llx, resyncing with network",networkConfigurationFingerprint,fp);
					networkConfigurationFingerprint = fp;
					resynchronize = true;
				}
			}

			// Ping supernodes separately for two reasons: (1) supernodes only ping each
			// other, and (2) we still want to ping them first on resynchronize.
			if ((resynchronize)||((now - lastSupernodePing) >= ZT_PEER_DIRECT_PING_DELAY)) {
				lastSupernodePing = now;
				std::vector< SharedPtr<Peer> > sns(_r->topology->supernodePeers());
				for(std::vector< SharedPtr<Peer> >::const_iterator p(sns.begin());p!=sns.end();++p)
					_r->sw->sendHELLO((*p)->address());
			}

			if (resynchronize) {
				/* If resynchronizing, forget P2P links to all peers and then send
				 * something to formerly active ones. This will relay via a supernode
				 * which will trigger a new RENDEZVOUS and a new hole punch. This
				 * functor excludes supernodes, which are pinged separately above. */
				_r->topology->eachPeer(Topology::ResetActivePeers(_r,now));
			} else {
				// Periodically check for changes in our local multicast subscriptions
				// and broadcast those changes to directly connected peers.
				if ((now - lastMulticastCheck) >= ZT_MULTICAST_LOCAL_POLL_PERIOD) {
					lastMulticastCheck = now;
					try {
						std::map< SharedPtr<Network>,std::set<MulticastGroup> > toAnnounce;
						std::vector< SharedPtr<Network> > networks(_r->nc->networks());
						for(std::vector< SharedPtr<Network> >::const_iterator nw(networks.begin());nw!=networks.end();++nw) {
							if ((*nw)->updateMulticastGroups())
								toAnnounce.insert(std::pair< SharedPtr<Network>,std::set<MulticastGroup> >(*nw,(*nw)->multicastGroups()));
						}
						if (toAnnounce.size())
							_r->sw->announceMulticastGroups(toAnnounce);
					} catch (std::exception &exc) {
						LOG("unexpected exception announcing multicast groups: %s",exc.what());
					} catch ( ... ) {
						LOG("unexpected exception announcing multicast groups: (unknown)");
					}
				}

				// Periodically ping all our non-stale direct peers unless we're a supernode.
				// Supernodes only ping each other (which is done above).
				if (!_r->topology->amSupernode()) {
					if ((now - lastPingCheck) >= ZT_PING_CHECK_DELAY) {
						lastPingCheck = now;
						try {
							_r->topology->eachPeer(Topology::PingPeersThatNeedPing(_r,now));
							_r->topology->eachPeer(Topology::OpenPeersThatNeedFirewallOpener(_r,now));
						} catch (std::exception &exc) {
							LOG("unexpected exception running ping check cycle: %s",exc.what());
						} catch ( ... ) {
							LOG("unexpected exception running ping check cycle: (unkonwn)");
						}
					}
				}
			}

			// Periodically or on resynchronize update network configurations.
			if ((resynchronize)||((now - lastNetworkAutoconfCheck) >= ZT_NETWORK_AUTOCONF_CHECK_DELAY)) {
				lastNetworkAutoconfCheck = now;
				std::vector< SharedPtr<Network> > nets(_r->nc->networks());
				for(std::vector< SharedPtr<Network> >::iterator n(nets.begin());n!=nets.end();++n) {
					if ((now - (*n)->lastConfigUpdate()) >= ZT_NETWORK_AUTOCONF_DELAY)
						(*n)->requestConfiguration();
				}
			}

			// Do periodic cleanup, flushes of stuff to disk, software update
			// checks, etc.
			if ((now - lastClean) >= ZT_DB_CLEAN_PERIOD) {
				lastClean = now;
				_r->mc->clean();
				_r->topology->clean();
				_r->nc->clean();
				if (_r->updater)
					_r->updater->checkIfMaxIntervalExceeded(now);
			}

			// Sleep for loop interval or until something interesting happens.
			try {
				unsigned long delay = std::min((unsigned long)ZT_MIN_SERVICE_LOOP_INTERVAL,_r->sw->doTimerTasks());
				uint64_t start = Utils::now();
				_r->mainLoopWaitCondition.wait(delay);
				lastDelayDelta = (long)(Utils::now() - start) - (long)delay; // used to detect sleep/wake
			} catch (std::exception &exc) {
				LOG("unexpected exception running Switch doTimerTasks: %s",exc.what());
			} catch ( ... ) {
				LOG("unexpected exception running Switch doTimerTasks: (unknown)");
			}
		}
	} catch ( ... ) {
		return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unexpected exception during outer main I/O loop");
	}

	return impl->terminate();
}

const char *Node::reasonForTermination() const
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
	((_NodeImpl *)_impl)->renv.mainLoopWaitCondition.signal();
}

void Node::resync()
	throw()
{
	((_NodeImpl *)_impl)->resynchronize = true;
	((_NodeImpl *)_impl)->renv.mainLoopWaitCondition.signal();
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

extern "C" {

ZeroTier::Node *zeroTierCreateNode(const char *hp,unsigned int port,unsigned int controlPort)
{
	return new ZeroTier::Node(hp,port,controlPort);
}

void zeroTierDeleteNode(ZeroTier::Node *n)
{
	delete n;
}

ZeroTier::Node::LocalClient *zeroTierCreateLocalClient(const char *authToken,unsigned int controlPort,void (*resultHandler)(void *,unsigned long,const char *),void *arg)
{
	return new ZeroTier::Node::LocalClient(authToken,controlPort,resultHandler,arg);
}

void zeroTierDeleteLocalClient(ZeroTier::Node::LocalClient *lc)
{
	delete lc;
}

} // extern "C"
