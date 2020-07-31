/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
	String _path;
	String _networksPath;
	std::thread _onlineUpdateThread;
	std::map< uint64_t,std::map<uint64_t,std::map<int64_t,InetAddress> > > _online;
	std::mutex _online_l;
	bool _running;
};

} // namespace ZeroTier

#endif
