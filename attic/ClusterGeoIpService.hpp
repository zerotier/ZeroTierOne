/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_CLUSTERGEOIPSERVICE_HPP
#define ZT_CLUSTERGEOIPSERVICE_HPP

#ifdef ZT_ENABLE_CLUSTER

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vector>
#include <string>
#include <algorithm>

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/NonCopyable.hpp"
#include "../node/InetAddress.hpp"

namespace ZeroTier {

/**
 * Loads a GeoIP CSV into memory for fast lookup, reloading as needed
 *
 * This was designed around the CSV from https://db-ip.com but can be used
 * with any similar GeoIP CSV database that is presented in the form of an
 * IP range and lat/long coordinates.
 *
 * It loads the whole database into memory, which can be kind of large. If
 * the CSV file changes, the changes are loaded automatically.
 */
class ClusterGeoIpService : NonCopyable
{
public:
	ClusterGeoIpService();
	~ClusterGeoIpService();

	/**
	 * Load or reload CSV file
	 *
	 * CSV column indexes start at zero. CSVs can be quoted with single or
	 * double quotes. Whitespace before or after commas is ignored. Backslash
	 * may be used for escaping whitespace as well.
	 *
	 * @param pathToCsv Path to (uncompressed) CSV file
	 * @param ipStartColumn Column with IP range start
	 * @param ipEndColumn Column with IP range end (inclusive)
	 * @param latitudeColumn Column with latitude
	 * @param longitudeColumn Column with longitude
	 * @return Number of valid records loaded or -1 on error (invalid file, not found, etc.)
	 */
	inline long load(const char *pathToCsv,int ipStartColumn,int ipEndColumn,int latitudeColumn,int longitudeColumn)
	{
		Mutex::Lock _l(_lock);
		return _load(pathToCsv,ipStartColumn,ipEndColumn,latitudeColumn,longitudeColumn);
	}

	/**
	 * Attempt to locate an IP
	 *
	 * This returns true if x, y, and z are set. If the return value is false
	 * the values of x, y, and z are undefined.
	 *
	 * @param ip IPv4 or IPv6 address
	 * @param x Reference to variable to receive X
	 * @param y Reference to variable to receive Y
	 * @param z Reference to variable to receive Z
	 * @return True if coordinates were set
	 */
	bool locate(const InetAddress &ip,int &x,int &y,int &z);

	/**
	 * @return True if IP database/service is available for queries (otherwise locate() will always be false)
	 */
	inline bool available() const
	{
		Mutex::Lock _l(_lock);
		return ((_v4db.size() + _v6db.size()) > 0);
	}

private:
	struct _V4E
	{
		uint32_t start;
		uint32_t end;
		float lat,lon;
		int16_t x,y,z;

		inline bool operator<(const _V4E &e) const { return (start < e.start); }
	};

	struct _V6E
	{
		uint8_t start[16];
		uint8_t end[16];
		float lat,lon;
		int16_t x,y,z;

		inline bool operator<(const _V6E &e) const { return (memcmp(start,e.start,16) < 0); }
	};

	static void _parseLine(const char *line,std::vector<_V4E> &v4db,std::vector<_V6E> &v6db,int ipStartColumn,int ipEndColumn,int latitudeColumn,int longitudeColumn);
	long _load(const char *pathToCsv,int ipStartColumn,int ipEndColumn,int latitudeColumn,int longitudeColumn);

	std::string _pathToCsv;
	int _ipStartColumn;
	int _ipEndColumn;
	int _latitudeColumn;
	int _longitudeColumn;

	uint64_t _lastFileCheckTime;
	uint64_t _csvModificationTime;
	int64_t _csvFileSize;

	std::vector<_V4E> _v4db;
	std::vector<_V6E> _v6db;

	Mutex _lock;
};

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER

#endif
