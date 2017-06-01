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

#ifndef ZT_DBM_HPP___
#define ZT_DBM_HPP___

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <stdexcept>

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/Utils.hpp"
#include "../node/Identity.hpp"
#include "../node/Peer.hpp"

#include "../ext/vsdm/vsdm.hpp"

// The Peer is the largest structure we persist here
#define ZT_DBM_MAX_VALUE_SIZE sizeof(Peer)

namespace ZeroTier {

class Node;
class DBM;

class DBM
{
public:
	ZT_PACKED_STRUCT(struct Value
	{
		Value(const uint64_t t,const uint16_t l,const void *d) :
			ts(t),
			l(l)
		{
			memcpy(data,d,l);
		}
		uint64_t ts;
		uint16_t len;
		uint8_t data[ZT_DBM_MAX_VALUE_SIZE];
	});

private:
	ZT_PACKED_STRUCT(struct _MapKey
	{
		_MapKey() : obj(0),type(0) {}
		_MapKey(const uint16_t t,const uint64_t o) : obj(o),type(t) {}
		uint64_t obj;
		uint16_t type;
		inline bool operator==(const _MapKey &k) const { return ((obj == k.obj)&&(type == k.type)); }
	});
	struct _MapHasher
	{
		inline std::size_t operator()(const _MapKey &k) const { return (std::size_t)((k.obj ^ (k.obj >> 32)) + (uint64_t)k.type); }
	};

	void onUpdate(uint64_t from,const _MapKey &k,const Value &v,uint64_t rev);
	void onDelete(uint64_t from,const _MapKey &k);

	class _vsdm_watcher
	{
	public:
		_vsdm_watcher(DBM *p) : _parent(p) {}
		inline void add(uint64_t from,const _MapKey &k,const Value &v,uint64_t rev) { _parent->onUpdate(from,k,v,rev); }
		inline void update(uint64_t from,const _MapKey &k,const Value &v,uint64_t rev) { _parent->onUpdate(from,k,v,rev); }
		inline void del(uint64_t from,const _MapKey &k) { _parent->onDelete(from,k); }
	private:
		DBM *_parent;
	};
	class _vsdm_serializer
	{
	public:
		static inline unsigned long objectSize(const _MapKey &k) { return 10; }
		static inline unsigned long objectSize(const Value &v) { return (10 + v.len); }
		static inline const char *objectData(const _MapKey &k) { return reinterpret_cast<const char *>(&k); }
		static inline const char *objectData(const Value &v) { return reinterpret_cast<const char *>(&v); }
		static inline bool objectDeserialize(const char *d,unsigned long l,_MapKey &k)
		{
			if (l == 10) {
				memcpy(&k,d,10);
				return true;
			}
			return false;
		}
		static inline bool objectDeserialize(const char *d,unsigned long l,Value &v)
		{
			if ((l >= 10)&&(l <= (10 + ZT_DBM_MAX_VALUE_SIZE))) {
				memcpy(&v,d,l);
				return true;
			}
			return false;
		}
	};
	class _vsdm_cryptor
	{
	public:
		_vsdm_cryptor(const Identity &secretIdentity);
		static inline unsigned long overhead() { return 24; }
		void encrypt(void *d,unsigned long l);
		bool decrypt(void *d,unsigned long l);
		uint8_t _key[32];
	};

	typedef vsdm< _MapKey,Value,16384,_vsdm_watcher,_vsdm_serializer,_vsdm_cryptor,_MapHasher > _Map;

	friend class _Map;

public:
	ZT_PACKED_STRUCT(struct ClusterPeerStatus
	{
		uint64_t startTime;
		uint64_t currentTime;
		uint64_t clusterPeersConnected;
		uint64_t ztPeersConnected;
		uint16_t platform;
		uint16_t arch;
	});

	DBM(const Identity &secretIdentity,uint64_t clusterMemberId,const std::string &basePath,Node *node);

	~DBM();

	void put(const ZT_StoredObjectType type,const uint64_t key,const void *data,unsigned int len);

	bool get(const ZT_StoredObjectType type,const uint64_t key,Value &value);

	void del(const ZT_StoredObjectType type,const uint64_t key);

	void clean();

private:
	bool DBM::_persistentPath(const ZT_StoredObjectType type,const uint64_t key,char *p,unsigned int maxlen);

	const std::string _basePath;
	Node *const _node;
	uint64_t _startTime;
	_Map _m;
};

} // namespace ZeroTier

#endif
