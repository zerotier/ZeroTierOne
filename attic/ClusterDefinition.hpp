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

#ifndef ZT_CLUSTERDEFINITION_HPP
#define ZT_CLUSTERDEFINITION_HPP

#ifdef ZT_ENABLE_CLUSTER

#include <vector>
#include <algorithm>

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../node/NonCopyable.hpp"
#include "../osdep/OSUtils.hpp"

#include "ClusterGeoIpService.hpp"

namespace ZeroTier {

/**
 * Parser for cluster definition file
 */
class ClusterDefinition : NonCopyable
{
public:
	struct MemberDefinition
	{
		MemberDefinition() : id(0),x(0),y(0),z(0) { name[0] = (char)0; }

		unsigned int id;
		int x,y,z;
		char name[256];
		InetAddress clusterEndpoint;
		std::vector<InetAddress> zeroTierEndpoints;
	};

	/**
	 * Load and initialize cluster definition and GeoIP data if any
	 *
	 * @param myAddress My ZeroTier address
	 * @param pathToClusterFile Path to cluster definition file
	 * @throws std::runtime_error Invalid cluster definition or unable to load data
	 */
	ClusterDefinition(uint64_t myAddress,const char *pathToClusterFile)
	{
		std::string cf;
		if (!OSUtils::readFile(pathToClusterFile,cf))
			return;

		char myAddressStr[64];
		Utils::ztsnprintf(myAddressStr,sizeof(myAddressStr),"%.10llx",myAddress);

		std::vector<std::string> lines(OSUtils::split(cf.c_str(),"\r\n","",""));
		for(std::vector<std::string>::iterator l(lines.begin());l!=lines.end();++l) {
			std::vector<std::string> fields(OSUtils::split(l->c_str()," \t","",""));
			if ((fields.size() < 5)||(fields[0][0] == '#')||(fields[0] != myAddressStr))
				continue;

			// <address> geo <CSV path> <ip start column> <ip end column> <latitutde column> <longitude column>
			if (fields[1] == "geo") {
				if ((fields.size() >= 7)&&(OSUtils::fileExists(fields[2].c_str()))) {
					int ipStartColumn = Utils::strToInt(fields[3].c_str());
					int ipEndColumn = Utils::strToInt(fields[4].c_str());
					int latitudeColumn = Utils::strToInt(fields[5].c_str());
					int longitudeColumn = Utils::strToInt(fields[6].c_str());
					if (_geo.load(fields[2].c_str(),ipStartColumn,ipEndColumn,latitudeColumn,longitudeColumn) <= 0)
						throw std::runtime_error(std::string("failed to load geo-ip data from ")+fields[2]);
				}
				continue;
			}

			// <address> <ID> <name> <backplane IP/port(s)> <ZT frontplane IP/port(s)> <x,y,z>
			int id = Utils::strToUInt(fields[1].c_str());
			if ((id < 0)||(id > ZT_CLUSTER_MAX_MEMBERS))
				throw std::runtime_error(std::string("invalid cluster member ID: ")+fields[1]);
			MemberDefinition &md = _md[id];

			md.id = (unsigned int)id;
			if (fields.size() >= 6) {
				std::vector<std::string> xyz(OSUtils::split(fields[5].c_str(),",","",""));
				md.x = (xyz.size() > 0) ? Utils::strToInt(xyz[0].c_str()) : 0;
				md.y = (xyz.size() > 1) ? Utils::strToInt(xyz[1].c_str()) : 0;
				md.z = (xyz.size() > 2) ? Utils::strToInt(xyz[2].c_str()) : 0;
			}
			Utils::scopy(md.name,sizeof(md.name),fields[2].c_str());
			md.clusterEndpoint.fromString(fields[3]);
			if (!md.clusterEndpoint)
				continue;
			std::vector<std::string> zips(OSUtils::split(fields[4].c_str(),",","",""));
			for(std::vector<std::string>::iterator zip(zips.begin());zip!=zips.end();++zip) {
				InetAddress i;
				i.fromString(*zip);
				if (i)
					md.zeroTierEndpoints.push_back(i);
			}

			_ids.push_back((unsigned int)id);
		}

		std::sort(_ids.begin(),_ids.end());
	}

	/**
	 * @return All member definitions in this cluster by ID (ID is array index)
	 */
	inline const MemberDefinition &operator[](unsigned int id) const throw() { return _md[id]; }

	/**
	 * @return Number of members in this cluster
	 */
	inline unsigned int size() const throw() { return (unsigned int)_ids.size(); }

	/**
	 * @return IDs of members in this cluster sorted by ID
	 */
	inline const std::vector<unsigned int> &ids() const throw() { return _ids; }

	/**
	 * @return GeoIP service for this cluster
	 */
	inline ClusterGeoIpService &geo() throw() { return _geo; }

	/**
	 * @return A vector (new copy) containing all cluster members
	 */
	inline std::vector<MemberDefinition> members() const
	{
		std::vector<MemberDefinition> m;
		for(std::vector<unsigned int>::const_iterator i(_ids.begin());i!=_ids.end();++i)
			m.push_back(_md[*i]);
		return m;
	}

private:
	MemberDefinition _md[ZT_CLUSTER_MAX_MEMBERS];
	std::vector<unsigned int> _ids;
	ClusterGeoIpService _geo;
};

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER

#endif
