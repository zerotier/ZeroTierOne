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

#ifndef ZT_ETHERNETTAPFACTORY_HPP
#define ZT_ETHERNETTAPFACTORY_HPP

#include <stdint.h>

#include "MAC.hpp"
#include "NonCopyable.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

class EthernetTap;

/**
 * Ethernet tap factory
 *
 * This serves up tap implementations for a given platform. It should never be
 * deleted until the Node using it is shut down, since doing so may invalidate
 * any tap devices it manages.
 *
 * Using a factory pattern will faciliatate packaging ZeroTier as a library,
 * as well as moving toward a design that makes unit testing the entire app
 * quite a bit easier.
 */
class EthernetTapFactory : NonCopyable
{
public:
	EthernetTapFactory() {}
	virtual ~EthernetTapFactory() {}

	/**
	 * Create / open an Ethernet tap device
	 *
	 * On some platforms (Windows) this can be a time-consuming operation.
	 *
	 * Note that close() must be used. Do not just delete the tap instance,
	 * since this may leave orphaned resources or cause other problems.
	 *
	 * @param mac MAC address
	 * @param mtu Device MTU
	 * @param metric Interface metric (higher = lower priority, may not be supported on all OSes)
	 * @param nwid ZeroTier network ID
	 * @param desiredDevice Desired system device name or NULL for no preference
	 * @param friendlyName Friendly name of this interface or NULL for none (not used on all platforms)
	 * @param handler Function to call when packets are received
	 * @param arg First argument to provide to handler
	 * @return EthernetTap instance
	 * @throws std::runtime_error Unable to initialize tap device
	 */
	virtual EthernetTap *open(
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *desiredDevice,
		const char *friendlyName,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg) = 0;

	/**
	 * Close an ethernet tap device and delete/free the tap object
	 *
	 * @param tap Tap instance
	 * @param destroyPersistentDevices If true, destroy persistent device (on platforms where applicable)
	 */
	virtual void close(EthernetTap *tap,bool destroyPersistentDevices) = 0;
};

} // namespace ZeroTier

#endif
