/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_CLUSTERDEFINITION_HPP
#define ZT_CLUSTERDEFINITION_HPP

#ifdef ZT_ENABLE_CLUSTER

#include <vector>
#include <algorithm>

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"

namespace ZeroTier {

/**
 * Parser for cluster definition file
 */
class ClusterDefinition
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

	ClusterDefinition(uint64_t myAddress,const char *pathToClusterFile)
	{
		std::string cf;
		if (!OSUtils::readFile(pathToClusterFile,cf))
			return;

		char myAddressStr[64];
		Utils::snprintf(myAddressStr,sizeof(myAddressStr),"%.10llx",myAddress);

		std::vector<std::string> lines(Utils::split(cf.c_str(),"\r\n","",""));
		for(std::vector<std::string>::iterator l(lines.begin());l!=lines.end();++l) {
			std::vector<std::string> fields(Utils::split(l->c_str()," \t","",""));
			if ((fields.size() < 5)||(fields[0][0] == '#')||(fields[0] != myAddressStr))
				continue;

			int id = Utils::strToUInt(fields[1].c_str());
			if ((id < 0)||(id > ZT_CLUSTER_MAX_MEMBERS))
				continue;
			MemberDefinition &md = _md[id];

			md.id = (unsigned int)id;
			if (fields.size() >= 6) {
				std::vector<std::string> xyz(Utils::split(fields[5].c_str(),",","",""));
				md.x = (xyz.size() > 0) ? Utils::strToInt(xyz[0].c_str()) : 0;
				md.y = (xyz.size() > 1) ? Utils::strToInt(xyz[1].c_str()) : 0;
				md.z = (xyz.size() > 2) ? Utils::strToInt(xyz[2].c_str()) : 0;
			}
			Utils::scopy(md.name,sizeof(md.name),fields[2].c_str());
			md.clusterEndpoint.fromString(fields[3]);
			if (!md.clusterEndpoint)
				continue;
			std::vector<std::string> zips(Utils::split(fields[4].c_str(),",","",""));
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

	inline const MemberDefinition &operator[](unsigned int id) const throw() { return _md[id]; }
	inline unsigned int size() const throw() { return (unsigned int)_ids.size(); }
	inline const std::vector<unsigned int> &ids() const throw() { return _ids; }

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
};

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER

#endif
