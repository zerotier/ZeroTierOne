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

#ifndef ZT_LINUXETHERNETTAPFACTORY_HPP
#define ZT_LINUXETHERNETTAPFACTORY_HPP

#include <vector>
#include <string>

#include "../node/EthernetTapFactory.hpp"
#include "../node/Mutex.hpp"

namespace ZeroTier {

class OSXEthernetTapFactory : public EthernetTapFactory
{
public:
	/**
	 * Create OSX ethernet tap factory
	 *
	 * If kext paths are specified, an attempt will be made to load the kext
	 * on launch if not present and unload it on shutdown.
	 *
	 * @param pathToTapKext Full path to the location of the tap kext
	 * @param tapKextName Name of tap kext as found within tap kext path (usually "tap.kext")
	 * @throws std::runtime_error Tap not available and unable to load kext
	 */
	OSXEthernetTapFactory(const char *pathToTapKext,const char *tapKextName);

	virtual ~OSXEthernetTapFactory();

	virtual EthernetTap *open(
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *desiredDevice,
		const char *friendlyName,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg);
	virtual void close(EthernetTap *tap,bool destroyPersistentDevices);

private:
	std::vector<EthernetTap *> _devices;
	Mutex _devices_m;
	std::string _pathToTapKext;
	std::string _tapKextName;
};

} // namespace ZeroTier

#endif
