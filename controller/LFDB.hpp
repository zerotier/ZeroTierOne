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

#ifndef ZT_CONTROLLER_LFDB_HPP
#define ZT_CONTROLLER_LFDB_HPP

#include "DB.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <atomic>

namespace ZeroTier {

/**
 * DB implementation for controller that stores data in LF
 */
class LFDB : public DB
{
public:
	/**
	 * @param myId This controller's identity
	 * @param path Base path for ZeroTier node itself
	 * @param lfOwnerPrivate LF owner private in PEM format
	 * @param lfOwnerPublic LF owner public in @base62 format
	 * @param lfNodeHost LF node host
	 * @param lfNodePort LF node http (not https) port
	 * @param storeOnlineState If true, store online/offline state and IP info in LF (a lot of data, only for private networks!)
	 */
	LFDB(const Identity &myId,const char *path,const char *lfOwnerPrivate,const char *lfOwnerPublic,const char *lfNodeHost,int lfNodePort,bool storeOnlineState);
	virtual ~LFDB();

	virtual bool waitForReady();
	virtual bool isReady();
	virtual bool save(nlohmann::json &record,bool notifyListeners);
	virtual void eraseNetwork(const uint64_t networkId);
	virtual void eraseMember(const uint64_t networkId,const uint64_t memberId);
	virtual void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress);

protected:
	const Identity _myId;

	std::string _lfOwnerPrivate,_lfOwnerPublic;
	std::string _lfNodeHost;
	int _lfNodePort;

	struct _MemberState
	{
		_MemberState() :
			lastOnlineAddress(),
			lastOnlineTime(0),
			dirty(false),
			lastOnlineDirty(false) {}
		InetAddress lastOnlineAddress;
		int64_t lastOnlineTime;
		bool dirty;
		bool lastOnlineDirty;
	};
	struct _NetworkState
	{
		_NetworkState() :
			members(),
			dirty(false) {}
		std::unordered_map<uint64_t,_MemberState> members;
		bool dirty;
	};
	std::unordered_map<uint64_t,_NetworkState> _state;
	std::mutex _state_l;

	std::atomic_bool _running;
	std::atomic_bool _ready;
	std::thread _syncThread;
	bool _storeOnlineState;
};

} // namespace ZeroTier

#endif
