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

#include "DBM.hpp"

#include "../version.h"

#include "../node/Salsa20.hpp"
#include "../node/Poly1305.hpp"
#include "../node/SHA512.hpp"

#include "../osdep/OSUtils.hpp"

#define ZT_STORED_OBJECT_TYPE__CLUSTER_NODE_STATUS (ZT_STORED_OBJECT__MAX_TYPE_ID + 1)
#define ZT_STORED_OBJECT_TYPE__CLUSTER_DEFINITION (ZT_STORED_OBJECT__MAX_TYPE_ID + 2)

namespace ZeroTier {

// We generate the cluster ID from our address and version info since this is
// not at all designed to allow interoperation between versions (or endians)
// in the same cluster.
static inline uint64_t _mkClusterId(const Address &myAddress)
{
	uint64_t x = ZEROTIER_ONE_VERSION_MAJOR;
	x <<= 8;
	x += ZEROTIER_ONE_VERSION_MINOR;
	x <<= 8;
	x += ZEROTIER_ONE_VERSION_REVISION;
	x <<= 40;
	x ^= myAddress.toInt();
#if __BYTE_ORDER == __BIG_ENDIAN
	++x;
#endif;
	return x;
}

void DBM::onUpdate(uint64_t from,const _MapKey &k,const _MapValue &v,uint64_t rev)
{
	char p[4096];
	char tmp[ZT_DBM_MAX_VALUE_SIZE];
	if (_persistentPath((ZT_StoredObjectType)k.type,k.key,p,sizeof(p))) {
		// Reduce unnecessary disk writes
		FILE *f = fopen(p,"r");
		if (f) {
			long n = (long)fread(tmp,1,sizeof(tmp),f);
			fclose(f);
			if ((n == (long)v.len)&&(!memcmp(v.data,tmp,n)))
				return;
		}

		// Write to disk if file has changed or was not already present
		f = fopen(p,"w");
		if (f) {
			if (fwrite(data,len,1,f) != 1)
				fprintf(stderr,"WARNING: error writing to %s (I/O error)" ZT_EOL_S,p);
			fclose(f);
			if (type == ZT_STORED_OBJECT_IDENTITY_SECRET)
				OSUtils::lockDownFile(p,false);
		} else {
			fprintf(stderr,"WARNING: error writing to %s (cannot open)" ZT_EOL_S,p);
		}
	}
}

void DBM::onDelete(uint64_t from,const _MapKey &k)
{
	char p[4096];
	if (_persistentPath((ZT_StoredObjectType)k.type,k.key,p,sizeof(p)))
		OSUtils::rm(p);
}

DBM::_vsdm_cryptor::_vsdm_cryptor(const Identity &secretIdentity)
{
	uint8_t s512[64];
	SHA512::hash(h512,secretIdentity.privateKeyPair().priv.data,ZT_C25519_PRIVATE_KEY_LEN);
	memcpy(_key,s512,sizeof(_key));
}

void DBM::_vsdm_cryptor::encrypt(void *d,unsigned long l)
{
	if (l >= 24) { // sanity check
		uint8_t key[32];
		uint8_t authKey[32];
		uint8_t auth[16];

		uint8_t *const iv = reinterpret_cast<uint8_t *>(d) + (l - 16);
		Utils::getSecureRandom(iv,16);
		memcpy(key,_key,32);
		for(unsigned long i=0;i<8;++i)
			_key[i] ^= iv[i];

		Salsa20 s20(key,iv + 8);
		memset(authKey,0,32);
		s20.crypt12(authKey,authKey,32);
		s20.crypt12(d,d,l - 24);

		Poly1305::compute(auth,d,l - 24,authKey);
		memcpy(reinterpret_cast<uint8_t *>(d) + (l - 24),auth,8);
	}
}

bool DBM::_vsdm_cryptor::decrypt(void *d,unsigned long l)
{
	if (l >= 24) { // sanity check
		uint8_t key[32];
		uint8_t authKey[32];
		uint8_t auth[16];

		uint8_t *const iv = reinterpret_cast<uint8_t *>(d) + (l - 16);
		memcpy(key,_key,32);
		for(unsigned long i=0;i<8;++i)
			_key[i] ^= iv[i];

		Salsa20 s20(key,iv + 8);
		memset(authKey,0,32);
		s20.crypt12(authKey,authKey,32);

		Poly1305::compute(auth,d,l - 24,authKey);
		if (!Utils::secureEq(reinterpret_cast<uint8_t *>(d) + (l - 24),auth,8))
			return false;

		s20.crypt12(d,d,l - 24);

		return true;
	}
	return false;
}

DBM::DBM(const Identity &secretIdentity,uint64_t clusterMemberId,const std::string &basePath,Node *node) :
	_basePath(basePath),
	_node(node),
	_startTime(OSUtils::now()),
	_m(_mkClusterId(secretIdentity.address()),clusterMemberId,false,_vsdm_cryptor(secretIdentity),_vsdm_watcher(this))
{
}

DBM::~DBM()
{
}

void DBM::put(const ZT_StoredObjectType type,const uint64_t key,const void *data,unsigned int len)
{
	char p[4096];
	if (_m.put(_MapKey(key,(uint16_t)type),Value(OSUtils::now(),(uint16_t)len,data))) {
		if (_persistentPath(type,key,p,sizeof(p))) {
			FILE *f = fopen(p,"w");
			if (f) {
				if (fwrite(data,len,1,f) != 1)
					fprintf(stderr,"WARNING: error writing to %s (I/O error)" ZT_EOL_S,p);
				fclose(f);
				if (type == ZT_STORED_OBJECT_IDENTITY_SECRET)
					OSUtils::lockDownFile(p,false);
			} else {
				fprintf(stderr,"WARNING: error writing to %s (cannot open)" ZT_EOL_S,p);
			}
		}
	}
}

bool DBM::get(const ZT_StoredObjectType type,const uint64_t key,Value &value)
{
	char p[4096];
	if (_m.get(_MapKey(key,(uint16_t)type),value))
		return true;
	if (_persistentPath(type,key,p,sizeof(p))) {
		FILE *f = fopen(p,"r");
		if (f) {
			long n = (long)fread(value.data,1,sizeof(value.data),f);
			value.len = (n > 0) ? (uint16_t)n : (uint16_t)0;
			fclose(f);
			value.ts = OSUtils::getLastModified(p);
			_m.put(_MapKey(key,(uint16_t)type),value);
			return true;
		}
	}
	return false;
}

void DBM::del(const ZT_StoredObjectType type,const uint64_t key)
{
	char p[4096];
	_m.del(_MapKey(key,(uint16_t)type));
	if (_persistentPath(type,key,p,sizeof(p)))
		OSUtils::rm(p);
}

void DBM::clean()
{
}

bool DBM::_persistentPath(const ZT_StoredObjectType type,const uint64_t key,char *p,unsigned int maxlen)
{
	switch(type) {
		case ZT_STORED_OBJECT_IDENTITY_PUBLIC:
			Utils::snprintf(p,maxlen,"%s" ZT_PATH_SEPARATOR_S "identity.public",_basePath.c_str());
			return true;
		case ZT_STORED_OBJECT_IDENTITY_SECRET:
			Utils::snprintf(p,maxlen,"%s" ZT_PATH_SEPARATOR_S "identity.secret",_basePath.c_str());
			return true;
		case ZT_STORED_OBJECT_IDENTITY:
			Utils::snprintf(p,maxlen,"%s" ZT_PATH_SEPARATOR_S "iddb.d" ZT_PATH_SEPARATOR_S "%.10llx",_basePath.c_str(),key);
			return true;
		case ZT_STORED_OBJECT_NETWORK_CONFIG:
			Utils::snprintf(p,maxlen,"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.conf",_basePath.c_str(),key);
			return true;
		case ZT_STORED_OBJECT_PLANET:
			Utils::snprintf(p,maxlen,"%s" ZT_PATH_SEPARATOR_S "planet",_basePath.c_str());
			return true;
		case ZT_STORED_OBJECT_MOON:
			Utils::snprintf(p,maxlen,"%s" ZT_PATH_SEPARATOR_S "moons.d" ZT_PATH_SEPARATOR_S "%.16llx.moon",_basePath.c_str(),key);
			return true;
		case (ZT_StoredObjectType)ZT_STORED_OBJECT_TYPE__CLUSTER_DEFINITION:
			Utils::snprintf(p,maxlen,"%s" ZT_PATH_SEPARATOR_S "cluster",_basePath.c_str());
			return true;
		default:
			return false;
	}
}

} // namespace ZeroTier
