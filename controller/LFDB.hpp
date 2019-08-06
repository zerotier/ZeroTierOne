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
