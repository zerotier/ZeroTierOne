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

#ifndef ZT_CONTROLLER_FILEDB_HPP
#define ZT_CONTROLLER_FILEDB_HPP

#include "DB.hpp"

namespace ZeroTier
{

class FileDB : public DB
{
public:
	FileDB(const char *path);
	virtual ~FileDB();

	virtual bool waitForReady();
	virtual bool isReady();
	virtual bool save(nlohmann::json &record,bool notifyListeners);
	virtual void eraseNetwork(const uint64_t networkId);
	virtual void eraseMember(const uint64_t networkId,const uint64_t memberId);
	virtual void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress);

protected:
	std::string _path;
	std::string _networksPath;
	std::string _tracePath;
	std::thread _onlineUpdateThread;
	std::map< uint64_t,std::map<uint64_t,std::map<int64_t,InetAddress> > > _online;
	std::mutex _online_l;
	bool _running;
};

} // namespace ZeroTier

#endif
