/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_CLUSTERGEOIPSERVICE_HPP
#define ZT_CLUSTERGEOIPSERVICE_HPP

#ifdef ZT_ENABLE_CLUSTER

#include <vector>
#include <map>
#include <string>

#include "../node/Constants.hpp"
#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"
#include "../osdep/Thread.hpp"

namespace ZeroTier {

/**
 * Runs the Cluster GeoIP service in the background and resolves geoIP queries
 */
class ClusterGeoIpService
{
public:
	/**
	 * @param pathToExe Path to cluster geo-resolution service executable
	 */
	ClusterGeoIpService(const char *pathToExe);

	~ClusterGeoIpService();

	/**
	 * Attempt to locate an IP
	 *
	 * This returns true if x, y, and z are set. Otherwise it returns false
	 * and a geo-locate job is ordered in the background. This usually takes
	 * 500-1500ms to complete, after which time results will be available.
	 * If false is returned the supplied coordinate variables are unchanged.
	 *
	 * @param ip IPv4 or IPv6 address
	 * @param x Reference to variable to receive X
	 * @param y Reference to variable to receive Y
	 * @param z Reference to variable to receive Z
	 * @return True if coordinates were set
	 */
	bool locate(const InetAddress &ip,int &x,int &y,int &z);

	void threadMain()
		throw();

private:
	const std::string _pathToExe;
	int _sOutputFd;
	int _sInputFd;
	volatile long _sPid;
	volatile bool _run;
	Thread _thread;
	Mutex _sOutputLock;

	struct _CE { uint64_t ts; int x,y,z; };
	std::map< InetAddress,_CE > _cache;
	Mutex _cache_m;
};

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER

#endif
