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
#include <unistd.h>

#include "OSXEthernetTapFactory.hpp"
#include "OSXEthernetTap.hpp"

namespace ZeroTier {

OSXEthernetTapFactory::OSXEthernetTapFactory(const char *pathToTapKext,const char *tapKextName)
	_pathToTapKext((pathToTapKext) ? pathToTapKext : ""),
	_tapKextName((tapKextName) ? tapKextName : "")
{
	struct stat stattmp;

	if ((_pathToTapKext.length())&&(_tapKextName.length())) {
		if (stat("/dev/zt0",&stattmp)) {
			long kextpid = (long)vfork();
			if (kextpid == 0) {
				::chdir(_pathToTapKext.c_str());
				::execl("/sbin/kextload","/sbin/kextload","-q","-repository",_pathToTapKext.c_str(),_tapKextName.c_str(),(const char *)0);
				::_exit(-1);
			} else if (kextpid > 0) {
				int exitcode = -1;
				::waitpid(kextpid,&exitcode,0);
			} else throw std::runtime_error("unable to create subprocess with fork()");
		}
	}

	if (stat("/dev/zt0",&stattmp)) {
		::usleep(500); // give tap device driver time to start up and try again
		if (stat("/dev/zt0",&stattmp))
			throw std::runtime_error("/dev/zt# tap devices do not exist and unable to load kernel extension");
	}
}

OSXEthernetTapFactory::~OSXEthernetTapFactory()
{
	Mutex::Lock _l(_devices_m);
	for(std::vector<EthernetTap *>::iterator d(_devices.begin());d!=_devices.end();++d)
		delete *d;

	if ((_pathToTapKext.length())&&(_tapKextName.length())) {
		// Attempt to unload kext. If anything else is using a /dev/zt# node, this
		// fails and the kext stays in the kernel.
		char tmp[16384];
		sprintf(tmp,"%s/%s",_pathToTapKext.c_str(),_tapKextName.c_str());
		long kextpid = (long)vfork();
		if (kextpid == 0) {
			::execl("/sbin/kextunload","/sbin/kextunload",tmp,(const char *)0);
			::_exit(-1);
		} else if (kextpid > 0) {
			int exitcode = -1;
			::waitpid(kextpid,&exitcode,0);
		}
	}
}

EthernetTap *OSXEthernetTapFactory::open(
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *desiredDevice,
	const char *friendlyName,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
{
	Mutex::Lock _l(_devices_m);
	EthernetTap *t = new OSXEthernetTap(mac,mtu,metric,nwid,desiredDevice,friendlyName,handler,arg);
	_devices.push_back(t);
	return t;
}

void OSXEthernetTapFactory::close(EthernetTap *tap,bool destroyPersistentDevices)
{
	{
		Mutex::Lock _l(_devices_m);
		for(std::vector<EthernetTap *>::iterator d(_devices.begin());d!=_devices.end();++d) {
			if (*d == tap) {
				_devices.erase(d);
				break;
			}
		}
	}
	delete tap;
}

} // namespace ZeroTier
