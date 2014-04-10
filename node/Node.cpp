/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include "Node.hpp"
#include "RuntimeEnvironment.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Defaults.hpp"
#include "Identity.hpp"
#include "Topology.hpp"
#include "SocketManager.hpp"
#include "Switch.hpp"
#include "EthernetTap.hpp"
#include "CMWC4096.hpp"
#include "NodeConfig.hpp"
#include "SysEnv.hpp"
#include "Network.hpp"
#include "MulticastGroup.hpp"
#include "Mutex.hpp"
#include "Multicaster.hpp"
#include "Service.hpp"
#include "SoftwareUpdater.hpp"
#include "Buffer.hpp"
#include "IpcConnection.hpp"

#include "../version.h"

namespace ZeroTier {

// ---------------------------------------------------------------------------

struct _NodeControlClientImpl
{
	void (*resultHandler)(void *,const char *);
	void *arg;
	IpcConnection *ipcc;
	std::string err;
};

static void _CBipcResultHandler(void *arg,IpcConnection *ipcc,IpcConnection::EventType event,const char *result)
{
	if ((event == IpcConnection::IPC_EVENT_COMMAND)&&(result)) {
		if (strcmp(result,"200 auth OK"))
			((_NodeControlClientImpl *)arg)->resultHandler(((_NodeControlClientImpl *)arg)->arg,result);
	}
}

Node::NodeControlClient::NodeControlClient(const char *hp,void (*resultHandler)(void *,const char *),void *arg,const char *authToken)
	throw() :
	_impl((void *)new _NodeControlClientImpl)
{
	_NodeControlClientImpl *impl = (_NodeControlClientImpl *)_impl;
	impl->ipcc = (IpcConnection *)0;

	if (!hp)
		hp = ZT_DEFAULTS.defaultHomePath.c_str();

	std::string at;
	if (authToken)
		at = authToken;
	else if (!Utils::readFile(authTokenDefaultSystemPath(),at)) {
		if (!Utils::readFile(authTokenDefaultUserPath(),at)) {
			impl->err = "no authentication token specified and authtoken.secret not readable";
			return;
		}
	}

	std::string myid;
	if (Utils::readFile((std::string(hp) + ZT_PATH_SEPARATOR_S + "identity.public").c_str(),myid)) {
		std::string myaddr(myid.substr(0,myid.find(':')));
		if (myaddr.length() != 10)
			impl->err = "invalid address extracted from identity.public";
		else {
			try {
				impl->resultHandler = resultHandler;
				impl->arg = arg;
				impl->ipcc = new IpcConnection((std::string(ZT_IPC_ENDPOINT_BASE) + myaddr).c_str(),&_CBipcResultHandler,_impl);
				impl->ipcc->printf("auth %s"ZT_EOL_S,at.c_str());
			} catch ( ... ) {
				impl->ipcc = (IpcConnection *)0;
				impl->err = "failure connecting to running ZeroTier One service";
			}
		}
	} else impl->err = "unable to read identity.public";
}

Node::NodeControlClient::~NodeControlClient()
{
	if (_impl) {
		delete ((_NodeControlClientImpl *)_impl)->ipcc;
		delete (_NodeControlClientImpl *)_impl;
	}
}

const char *Node::NodeControlClient::error() const
	throw()
{
	if (((_NodeControlClientImpl *)_impl)->err.length())
		return ((_NodeControlClientImpl *)_impl)->err.c_str();
	return (const char *)0;
}

void Node::NodeControlClient::send(const char *command)
	throw()
{
	try {
		if (((_NodeControlClientImpl *)_impl)->ipcc)
			((_NodeControlClientImpl *)_impl)->ipcc->printf("%s"ZT_EOL_S,command);
	} catch ( ... ) {}
}

std::vector<std::string> Node::NodeControlClient::splitLine(const char *line)
{
	return Utils::split(line," ","\\","\"");
}

const char *Node::NodeControlClient::authTokenDefaultUserPath()
{
	static std::string dlp;
	static Mutex dlp_m;

	Mutex::Lock _l(dlp_m);

#ifdef __WINDOWS__

	if (!dlp.length()) {
		char buf[16384];
		if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_APPDATA,NULL,0,buf)))
			dlp = (std::string(buf) + "\\ZeroTier\\One\\authtoken.secret");
	}

#else // not __WINDOWS__

	if (!dlp.length()) {
		const char *home = getenv("HOME");
		if (home) {
#ifdef __APPLE__
			dlp = (std::string(home) + "/Library/Application Support/ZeroTier/One/authtoken.secret");
#else
			dlp = (std::string(home) + "/.zeroTierOneAuthToken");
#endif
		}
	}

#endif // __WINDOWS__ or not __WINDOWS__

	return dlp.c_str();
}

const char *Node::NodeControlClient::authTokenDefaultSystemPath()
{
	static std::string dsp;
	static Mutex dsp_m;

	Mutex::Lock _l(dsp_m);

	if (!dsp.length())
		dsp = (ZT_DEFAULTS.defaultHomePath + ZT_PATH_SEPARATOR_S"authtoken.secret");

	return dsp.c_str();
}

// ---------------------------------------------------------------------------

struct _NodeImpl
{
	RuntimeEnvironment renv;
	unsigned int udpPort,tcpPort;
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
		TRACE("shutdown: delete netconfService");
#endif
		delete renv.updater;
		TRACE("shutdown: delete updater");
		delete renv.nc;
		TRACE("shutdown: delete nc");
		delete renv.sysEnv;
		TRACE("shutdown: delete sysEnv");
		delete renv.topology;
		TRACE("shutdown: delete topology");
		delete renv.sm;
		TRACE("shutdown: delete sm");
		delete renv.sw;
		TRACE("shutdown: delete sw");
		delete renv.mc;
		TRACE("shutdown: delete mc");
		delete renv.prng;
		TRACE("shutdown: delete prng");
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

Node::Node(const char *hp,unsigned int udpPort,unsigned int tcpPort,bool resetIdentity)
	throw() :
	_impl(new _NodeImpl)
{
	_NodeImpl *impl = (_NodeImpl *)_impl;

	if ((hp)&&(hp[0]))
		impl->renv.homePath = hp;
	else impl->renv.homePath = ZT_DEFAULTS.defaultHomePath;

	if (resetIdentity) {
		// Forget identity and peer database, peer keys, etc.
		Utils::rm((impl->renv.homePath + ZT_PATH_SEPARATOR_S + "identity.public").c_str());
		Utils::rm((impl->renv.homePath + ZT_PATH_SEPARATOR_S + "identity.secret").c_str());
		Utils::rm((impl->renv.homePath + ZT_PATH_SEPARATOR_S + "peers.persist").c_str());

		// Truncate network config information in networks.d but leave the files since we
		// still want to remember any networks we have joined. This will force re-config.
		std::string networksDotD(impl->renv.homePath + ZT_PATH_SEPARATOR_S + "networks.d");
		std::map< std::string,bool > nwfiles(Utils::listDirectory(networksDotD.c_str()));
		for(std::map<std::string,bool>::iterator nwf(nwfiles.begin());nwf!=nwfiles.end();++nwf) {
			FILE *foo = fopen((networksDotD + ZT_PATH_SEPARATOR_S + nwf->first).c_str(),"w");
			if (foo)
				fclose(foo);
		}
	}

	impl->udpPort = udpPort & 0xffff;
	impl->tcpPort = tcpPort & 0xffff;
	impl->reasonForTermination = Node::NODE_RUNNING;
	impl->started = false;
	impl->running = false;
	impl->resynchronize = false;
}

Node::~Node()
{
	delete (_NodeImpl *)_impl;
}

static void _CBztTraffic(const SharedPtr<Socket> &fromSock,void *arg,const InetAddress &from,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &data)
{
	const RuntimeEnvironment *_r = (const RuntimeEnvironment *)arg;
	if ((_r->sw)&&(!_r->shutdownInProgress))
		_r->sw->onRemotePacket(fromSock,from,data);
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
		_r->sm = new SocketManager(impl->udpPort,impl->tcpPort,&_CBztTraffic,_r);
		_r->topology = new Topology(_r,Utils::fileExists((_r->homePath + ZT_PATH_SEPARATOR_S + "iddb.d").c_str()));
		_r->sysEnv = new SysEnv();
		try {
			_r->nc = new NodeConfig(_r,configAuthToken.c_str());
		} catch (std::exception &exc) {
			return impl->terminateBecause(Node::NODE_UNRECOVERABLE_ERROR,"unable to initialize IPC socket: is ZeroTier One already running?");
		}
		_r->node = this;
#ifdef ZT_AUTO_UPDATE
		if (ZT_DEFAULTS.updateLatestNfoURL.length()) {
			_r->updater = new SoftwareUpdater(_r);
			_r->updater->cleanOldUpdates(); // clean out updates.d on startup
		} else {
			LOG("WARNING: unable to enable software updates: latest .nfo URL from ZT_DEFAULTS is empty (does this platform actually support software updates?)");
		}
#endif

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

		uint64_t lastNetworkAutoconfCheck = Utils::now() - 5000ULL; // check autoconf again after 5s for startup
		uint64_t lastPingCheck = 0;
		uint64_t lastClean = Utils::now(); // don't need to do this immediately
		uint64_t lastNetworkFingerprintCheck = 0;
		uint64_t lastMulticastCheck = 0;
		uint64_t lastSupernodePingCheck = 0;
		long lastDelayDelta = 0;

		uint64_t networkConfigurationFingerprint = 0;
		_r->timeOfLastResynchronize = Utils::now();

		while (impl->reasonForTermination == NODE_RUNNING) {
			/* This is how the service automatically shuts down when the OSX .app is
			 * thrown in the trash. It's not used on any other platform for now but
			 * could do similar things. It's disabled on Windows since it doesn't really
			 * work there. */
#ifdef __UNIX_LIKE__
			if (Utils::fileExists(shutdownIfUnreadablePath.c_str(),false)) {
				FILE *tmpf = fopen(shutdownIfUnreadablePath.c_str(),"r");
				if (!tmpf)
					return impl->terminateBecause(Node::NODE_NORMAL_TERMINATION,"shutdownIfUnreadable exists but is not readable");
				fclose(tmpf);
			}
#endif

			uint64_t now = Utils::now();
			bool resynchronize = false;

			// If it looks like the computer slept and woke, resynchronize.
			if (lastDelayDelta >= ZT_SLEEP_WAKE_DETECTION_THRESHOLD) {
				resynchronize = true;
				LOG("probable suspend/resume detected, pausing a moment for things to settle...");
				Thread::sleep(ZT_SLEEP_WAKE_SETTLE_TIME);
			}

			// If our network environment looks like it changed, resynchronize.
			if ((resynchronize)||((now - lastNetworkFingerprintCheck) >= ZT_NETWORK_FINGERPRINT_CHECK_DELAY)) {
				lastNetworkFingerprintCheck = now;
				uint64_t fp = _r->sysEnv->getNetworkConfigurationFingerprint(_r->nc->networkTapDeviceNames());
				if (fp != networkConfigurationFingerprint) {
					LOG("netconf fingerprint change: %.16llx != %.16llx, resyncing with network",networkConfigurationFingerprint,fp);
					networkConfigurationFingerprint = fp;
					resynchronize = true;
				}
			}

			// Supernodes do not resynchronize unless explicitly ordered via SIGHUP.
			if ((resynchronize)&&(_r->topology->amSupernode()))
				resynchronize = false;

			// Check for SIGHUP / force resync.
			if (impl->resynchronize) {
				impl->resynchronize = false;
				resynchronize = true;
				LOG("resynchronize forced by user, syncing with network");
			}

			if (resynchronize) {
				_r->tcpTunnelingEnabled = false; // turn off TCP tunneling master switch at first
				_r->timeOfLastResynchronize = now;
			}

			/* Supernodes are pinged separately and more aggressively. The
			 * ZT_STARTUP_AGGRO parameter sets a limit on how rapidly they are
			 * tried, while PingSupernodesThatNeedPing contains the logic for
			 * determining if they need PING. */
			if ((now - lastSupernodePingCheck) >= ZT_STARTUP_AGGRO) {
				lastSupernodePingCheck = now;

				uint64_t lastReceiveFromAnySupernode = 0; // function object result paramter
				_r->topology->eachSupernodePeer(Topology::FindMostRecentDirectReceiveTimestamp(lastReceiveFromAnySupernode));

				// Turn on TCP tunneling master switch if we haven't heard anything since before
				// the last resynchronize and we've been trying long enough.
				uint64_t tlr = _r->timeOfLastResynchronize;
				if ((lastReceiveFromAnySupernode < tlr)&&((now - tlr) >= ZT_TCP_TUNNEL_FAILOVER_TIMEOUT)) {
					TRACE("network still unreachable after %u ms, TCP TUNNELING ENABLED",(unsigned int)ZT_TCP_TUNNEL_FAILOVER_TIMEOUT);
					_r->tcpTunnelingEnabled = true;
				}

				_r->topology->eachSupernodePeer(Topology::PingSupernodesThatNeedPing(_r,now));
			}

			if (resynchronize) {
				/* Send NOP to all peers on resynchronize, directly to supernodes and
				 * indirectly to regular nodes (to trigger RENDEZVOUS). Also clear
				 * learned paths since they're likely no longer valid, and close
				 * TCP sockets since they're also likely invalid. */
				_r->sm->closeTcpSockets();
				_r->topology->eachPeer(Topology::ResetActivePeers(_r,now));
			} else {
				/* Periodically check for changes in our local multicast subscriptions
				 * and broadcast those changes to directly connected peers. */
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

				/* Periodically ping all our non-stale direct peers unless we're a supernode.
				 * Supernodes only ping each other (which is done above). */
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

			// Update network configurations when needed.
			if ((resynchronize)||((now - lastNetworkAutoconfCheck) >= ZT_NETWORK_AUTOCONF_CHECK_DELAY)) {
				lastNetworkAutoconfCheck = now;
				std::vector< SharedPtr<Network> > nets(_r->nc->networks());
				for(std::vector< SharedPtr<Network> >::iterator n(nets.begin());n!=nets.end();++n) {
					if ((now - (*n)->lastConfigUpdate()) >= ZT_NETWORK_AUTOCONF_DELAY)
						(*n)->requestConfiguration();
				}
			}

			// Do periodic tasks in submodules.
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
				unsigned long delay = std::min((unsigned long)ZT_MAX_SERVICE_LOOP_INTERVAL,_r->sw->doTimerTasks());
				uint64_t start = Utils::now();
				_r->sm->poll(delay);
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
	((_NodeImpl *)_impl)->renv.sm->whack();
}

void Node::resync()
	throw()
{
	((_NodeImpl *)_impl)->resynchronize = true;
	((_NodeImpl *)_impl)->renv.sm->whack();
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
