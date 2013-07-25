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

#ifndef __WINDOWS__
#include <dlfcn.h>
#endif

#include "Utils.hpp"
#include "RuntimeEnvironment.hpp"
#include "RPC.hpp"
#include "Switch.hpp"
#include "Topology.hpp"

namespace ZeroTier {

RPC::LocalService::LocalService(const char *dllPath)
	throw(std::invalid_argument) :
	_handle((void *)0),
	_init((void *)0),
	_do((void *)0),
	_free((void *)0),
	_destroy((void *)0)
{
	_handle = dlopen(dllPath,RTLD_LAZY|RTLD_LOCAL);
	if (!_handle)
		throw std::invalid_argument("Unable to load DLL: dlopen() failed");

	_init = dlsym(_handle,"ZeroTierPluginInit");
	if (!_init) {
		dlclose(_handle);
		throw std::invalid_argument("Unable to resolve symbol ZeroTierPluginInit in DLL");
	}
	_do = dlsym(_handle,"ZeroTierPluginDo");
	if (!_do) {
		dlclose(_handle);
		throw std::invalid_argument("Unable to resolve symbol ZeroTierPluginDo in DLL");
	}
	_free = dlsym(_handle,"ZeroTierPluginFree");
	if (!_free) {
		dlclose(_handle);
		throw std::invalid_argument("Unable to resolve symbol ZeroTierPluginFree in DLL");
	}
	_destroy = dlsym(_handle,"ZeroTierPluginDestroy");
	if (!_destroy) {
		dlclose(_handle);
		throw std::invalid_argument("Unable to resolve symbol ZeroTierPluginDestroy in DLL");
	}

	if (((int (*)())_init)() < 0) {
		dlclose(_handle);
		throw std::invalid_argument("ZeroTierPluginInit() returned error");
	}
}

RPC::LocalService::~LocalService()
{
	if (_handle) {
		if (_destroy)
			((void (*)())_destroy)();
		dlclose(_handle);
	}
}

std::pair< int,std::vector<std::string> > RPC::LocalService::operator()(const std::vector<std::string> &args)
{
	unsigned int alengths[4096];
	const void *argptrs[4096];
	const unsigned int *rlengths = (const unsigned int *)0;
	const void **resultptrs = (const void **)0;
	std::vector<std::string> results;

	if (args.size() > 4096)
		throw std::runtime_error("args[] too long");

	for(unsigned int i=0;i<args.size();++i) {
		alengths[i] = args[i].length();
		argptrs[i] = (const void *)args[i].data();
	}

	int rcount = ((int (*)(unsigned int,const unsigned int *,const void **,const unsigned int **,const void ***))_do)((unsigned int)args.size(),alengths,argptrs,&rlengths,&resultptrs);

	for(int i=0;i<rcount;++i)
		results.push_back(std::string((const char *)resultptrs[i],rlengths[i]));

	((void (*)(int,const unsigned int *,const void **))_free)(rcount,rlengths,resultptrs);

	return std::pair< int,std::vector<std::string> >(rcount,results);
}

RPC::RPC(const RuntimeEnvironment *renv) :
	_r(renv)
{
}

RPC::~RPC()
{
	for(std::map<uint64_t,RemoteCallOutstanding>::iterator co(_remoteCallsOutstanding.begin());co!=_remoteCallsOutstanding.end();++co) {
		if (co->second.handler)
			co->second.handler(co->second.arg,co->first,co->second.peer,ZT_RPC_ERROR_CANCELLED,std::vector<std::string>());
	}

	for(std::map<std::string,LocalService *>::iterator s(_rpcServices.begin());s!=_rpcServices.end();++s)
		delete s->second;
}

std::pair< int,std::vector<std::string> > RPC::callLocal(const std::string &name,const std::vector<std::string> &args)
{
	Mutex::Lock _l(_rpcServices_m);
	std::map<std::string,LocalService *>::iterator s(_rpcServices.find(name));
	if (s == _rpcServices.end())
		return std::pair< int,std::vector<std::string> >(ZT_RPC_ERROR_NOT_FOUND,std::vector<std::string>());
	return ((*(s->second))(args));
}

uint64_t RPC::callRemote(
	const Address &peer,
	const std::string &name,
	const std::vector<std::string> &args,
	void (*handler)(void *,uint64_t,const Address &,int,const std::vector<std::string> &),
	void *arg)
	throw(std::invalid_argument,std::out_of_range)
{
	Packet outp(peer,_r->identity.address(),Packet::VERB_RPC);

	if (name.length() > 0xffff)
		throw std::invalid_argument("function name too long");
	outp.append((uint16_t)name.length());
	outp.append(name);
	for(std::vector<std::string>::const_iterator a(args.begin());a!=args.end();++a) {
		if (a->length() > 0xffff)
			throw std::invalid_argument("argument too long");
		outp.append((uint16_t)a->length());
		outp.append(*a);
	}
	outp.compress();

	uint64_t id = outp.packetId();

	{
		Mutex::Lock _l(_remoteCallsOutstanding_m);
		RemoteCallOutstanding &rc = _remoteCallsOutstanding[id];
		rc.callTime = Utils::now();
		rc.peer = peer;
		rc.handler = handler;
		rc.arg = arg;
	}

	_r->sw->send(outp,true);

	return id;
}

void RPC::clean()
{
	Mutex::Lock _l(_remoteCallsOutstanding_m);
	uint64_t now = Utils::now();
	for(std::map<uint64_t,RemoteCallOutstanding>::iterator co(_remoteCallsOutstanding.begin());co!=_remoteCallsOutstanding.end();) {
		if ((now - co->second.callTime) >= ZT_RPC_TIMEOUT) {
			if (co->second.handler)
				co->second.handler(co->second.arg,co->first,co->second.peer,ZT_RPC_ERROR_EXPIRED_NO_RESPONSE,std::vector<std::string>());
			_remoteCallsOutstanding.erase(co++);
		} else ++co;
	}
}

} // namespace ZeroTier
