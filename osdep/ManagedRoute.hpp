/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_MANAGEDROUTE_HPP
#define ZT_MANAGEDROUTE_HPP

#include <stdlib.h>
#include <string.h>

#include "../node/InetAddress.hpp"
#include "../node/Utils.hpp"
#include "../node/SharedPtr.hpp"
#include "../node/AtomicCounter.hpp"

#include <stdexcept>
#include <vector>
#include <map>

namespace ZeroTier {

/**
 * A ZT-managed route that used C++ RAII semantics to automatically clean itself up on deallocate
 */
class ManagedRoute
{
	friend class SharedPtr<ManagedRoute>;

public:
	ManagedRoute(const InetAddress &target,const InetAddress &via,const InetAddress &src,const char *device)
	{
		_target = target;
		_via = via;
		_src = src;
		if (via.ss_family == AF_INET)
			_via.setPort(32);
		else if (via.ss_family == AF_INET6)
			_via.setPort(128);
		if (src.ss_family == AF_INET) {
			_src.setPort(32);
		} else if (src.ss_family == AF_INET6) {
			_src.setPort(128);
		}
		Utils::scopy(_device,sizeof(_device),device);
		_systemDevice[0] = (char)0;
	}

	~ManagedRoute()
	{
		this->remove();
	}

	/**
	 * Set or update currently set route
	 *
	 * This must be called periodically for routes that shadow others so that
	 * shadow routes can be updated. In some cases it has no effect
	 *
	 * @return True if route add/update was successful
	 */
	bool sync();

	/**
	 * Remove and clear this ManagedRoute
	 *
	 * This does nothing if this ManagedRoute is not set or has already been
	 * removed. If this is not explicitly called it is called automatically on
	 * destruct.
	 */
	void remove();

	inline const InetAddress &target() const { return _target; }
	inline const InetAddress &via() const { return _via; }
	inline const InetAddress &src() const { return _src; }
	inline const char *device() const { return _device; }

private:
	ManagedRoute(const ManagedRoute &) {}
	inline ManagedRoute &operator=(const ManagedRoute &) { return *this; }

	InetAddress _target;
	InetAddress _via;
	InetAddress _src;
	InetAddress _systemVia; // for route overrides
	std::map<InetAddress,bool> _applied; // routes currently applied
	char _device[128];
	char _systemDevice[128]; // for route overrides

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
