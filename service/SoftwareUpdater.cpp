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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "SoftwareUpdater.hpp"

#include "../version.h"

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../node/SHA512.hpp"
#include "../node/Buffer.hpp"
#include "../node/Node.hpp"
#include "../osdep/OSUtils.hpp"

namespace ZeroTier {

SoftwareUpdater::SoftwareUpdater(Node &node,const char *homePath,bool updateDistributor) :
	_node(node),
	_lastCheckTime(OSUtils::now()), // check in the future in case we just started, in which case we're probably offline
	_homePath(homePath)
{
	// Load all updates we are distributing if we are an update distributor and have an update-dist.d folder
	if (updateDistributor) {
		std::string udd(_homePath + ZT_PATH_SEPARATOR_S + "update-dist.d");
		std::vector<std::string> ud(OSUtils::listDirectory(udd.c_str()));
		for(std::vector<std::string>::iterator u(ud.begin());u!=ud.end();++u) {
			// Each update has a companion .json file describing it. Other files are ignored.
			if ((u->length() > 5)&&(u->substr(u->length() - 5,5) == ".json")) {
				std::string buf;
				if (OSUtils::readFile((udd + ZT_PATH_SEPARATOR_S + *u).c_str(),buf)) {
					try {
						_D d;
						d.meta = OSUtils::jsonParse(buf);
						std::string metaHash = OSUtils::jsonBinFromHex(d.meta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_HASH]);
						if ((metaHash.length() == ZT_SHA512_DIGEST_LEN)&&(OSUtils::readFile((udd + ZT_PATH_SEPARATOR_S + u->substr(0,u->length() - 5)).c_str(),d.bin))) {
							uint8_t sha512[ZT_SHA512_DIGEST_LEN];
							SHA512::hash(sha512,d.bin.data(),(unsigned int)d.bin.length());
							if (!memcmp(sha512,metaHash.data(),ZT_SHA512_DIGEST_LEN)) { // double check that hash in JSON is correct
								_dist[Array<uint8_t,16>(sha512)] = d;
							}
						}
					} catch ( ... ) {} // ignore bad meta JSON, etc.
				}
			}
		}
	}
}

SoftwareUpdater::~SoftwareUpdater()
{
}

void SoftwareUpdater::handleSoftwareUpdateUserMessage(uint64_t origin,const void *data,unsigned int len)
{
	if (!len) return;
	try {
		const MessageVerb v = (MessageVerb)reinterpret_cast<const uint8_t *>(data)[0];
		switch(v) {
			case VERB_GET_LATEST:
			case VERB_LATEST: {
				nlohmann::json req = OSUtils::jsonParse(std::string(reinterpret_cast<const char *>(data) + 1,len - 1));
				if (req.is_object()) {
					const unsigned int rvMaj = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR],0);
					const unsigned int rvMin = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR],0);
					const unsigned int rvRev = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION],0);
					if (v == VERB_GET_LATEST) {

						if (_dist.size() > 0) {
							const nlohmann::json *latest = (const nlohmann::json *)0;
							const std::string rSigner = OSUtils::jsonString(req[ZT_SOFTWARE_UPDATE_JSON_EXPECT_SIGNED_BY],"");
							for(std::map< Array<uint8_t,16>,_D >::const_iterator d(_dist.begin());d!=_dist.end();++d) {
								if (OSUtils::jsonString(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNED_BY],"") == rSigner) {
									const unsigned int dvMaj = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR],0);
									const unsigned int dvMin = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR],0);
									const unsigned int dvRev = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION],0);
									if (Utils::compareVersion(dvMaj,dvMin,dvRev,rvMaj,rvMin,rvRev) > 0)
										latest = &(d->second.meta);
								}
							}
							if (latest) {
								std::string lj;
								lj.push_back((char)VERB_LATEST);
								lj.append(OSUtils::jsonDump(*latest));
								_node.sendUserMessage(origin,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,lj.data(),(unsigned int)lj.length());
							}
						} // else no reply, since we have nothing to distribute

					} else { // VERB_LATEST

						if ((origin == ZT_SOFTWARE_UPDATE_SERVICE)&&
							  (Utils::compareVersion(rvMaj,rvMin,rvRev,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION) > 0)&&
							  (OSUtils::jsonString(req[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNED_BY],"") == ZT_SOFTWARE_UPDATE_SIGNING_AUTHORITY)) {
							const unsigned long len = (unsigned long)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIZE],0);
							const std::string hash = OSUtils::jsonBinFromHex(req[ZT_SOFTWARE_UPDATE_JSON_UPDATE_HASH]);
							if ((len <= ZT_SOFTWARE_UPDATE_MAX_SIZE)&&(hash.length() >= 16)) {
								if (_latestMeta != req) {
									_latestMeta = req;
									_latestBin = "";
									memcpy(_latestBinHashPrefix.data,hash.data(),16);
									_latestBinLength = len;
									_latestBinValid = false;
								}

								Buffer<128> gd;
								gd.append((uint8_t)VERB_GET_DATA);
								gd.append(_latestBinHashPrefix.data,16);
								gd.append((uint32_t)_latestBin.length());
								_node.sendUserMessage(ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
							}
						}

					}
				}
			}	break;

			case VERB_GET_DATA:
				if ((len >= 21)&&(_dist.size() > 0)) {
					std::map< Array<uint8_t,16>,_D >::iterator d(_dist.find(Array<uint8_t,16>(reinterpret_cast<const uint8_t *>(data) + 1)));
					if (d != _dist.end()) {
						unsigned long idx = (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 17) << 24;
						idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 18) << 16;
						idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 19) << 8;
						idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 20);
						if (idx < d->second.bin.length()) {
							Buffer<ZT_SOFTWARE_UPDATE_CHUNK_SIZE + 128> buf;
							buf.append((uint8_t)VERB_DATA);
							buf.append(reinterpret_cast<const uint8_t *>(data) + 1,16);
							buf.append((uint32_t)idx);
							buf.append(d->second.bin.data() + idx,std::max((unsigned long)ZT_SOFTWARE_UPDATE_CHUNK_SIZE,(unsigned long)(d->second.bin.length() - idx)));
							_node.sendUserMessage(origin,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,buf.data(),buf.size());
						}
					}
				}
				break;

			case VERB_DATA:
				if ((len >= 21)&&(!memcmp(_latestBinHashPrefix.data,reinterpret_cast<const uint8_t *>(data) + 1,16))) {
					unsigned long idx = (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 17) << 24;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 18) << 16;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 19) << 8;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 20);
					if (idx == _latestBin.length())
						_latestBin.append(reinterpret_cast<const char *>(data) + 21,len - 21);

					if (_latestBin.length() < _latestBinLength) {
						Buffer<128> gd;
						gd.append((uint8_t)VERB_GET_DATA);
						gd.append(_latestBinHashPrefix.data,16);
						gd.append((uint32_t)_latestBin.length());
						_node.sendUserMessage(ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
					}
				}
				break;

			default:
				break;
		}
	} catch ( ... ) {
		// Discard bad messages
	}
}

nlohmann::json SoftwareUpdater::check()
{
	if (_latestBinLength > 0) {
		if (_latestBin.length() >= _latestBinLength) {
			if (_latestBinValid) {
				return _latestMeta;
			} else {
				// This is the important security verification part!

				try {
					// (1) Check the hash itself to make sure the image is basically okay
					uint8_t sha512[ZT_SHA512_DIGEST_LEN];
					SHA512::hash(sha512,_latestBin.data(),(unsigned int)_latestBin.length());
					if (Utils::hex(sha512,ZT_SHA512_DIGEST_LEN) == OSUtils::jsonString(_latestMeta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_HASH],"")) {
						// (2) Check signature by signing authority
						std::string sig(OSUtils::jsonBinFromHex(_latestMeta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNATURE]));
						if (Identity(ZT_SOFTWARE_UPDATE_SIGNING_AUTHORITY).verify(_latestBin.data(),(unsigned int)_latestBin.length(),sig.data(),(unsigned int)sig.length())) {
							// If we passed both of these, the update is good!
							_latestBinValid = true;
							return _latestMeta;
						}
					}
				} catch ( ... ) {} // any exception equals verification failure

				// If we get here, checks failed.
				_latestMeta = nlohmann::json();
				_latestBin = "";
				_latestBinLength = 0;
				_latestBinValid = false;
			}
		} else {
			Buffer<128> gd;
			gd.append((uint8_t)VERB_GET_DATA);
			gd.append(_latestBinHashPrefix.data,16);
			gd.append((uint32_t)_latestBin.length());
			_node.sendUserMessage(ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
		}
	}

	const uint64_t now = OSUtils::now();
	if ((now - _lastCheckTime) >= ZT_SOFTWARE_UPDATE_CHECK_PERIOD) {
	}

	return nlohmann::json();
}

void SoftwareUpdater::apply()
{
	if ((_latestBin.length() == _latestBinLength)&&(_latestBinLength > 0)&&(_latestBinValid)) {
	}
}

} // namespace ZeroTier
