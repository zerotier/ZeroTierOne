/*
 * Copyright (c)2019 ZeroTier, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../node/Constants.hpp"
#include "../version.h"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#include <netioapi.h>
#include <iphlpapi.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ifaddrs.h>
#endif

#include "SoftwareUpdater.hpp"

#include "../node/Utils.hpp"
#include "../node/SHA512.hpp"
#include "../node/Buffer.hpp"
#include "../node/Node.hpp"

#include "../osdep/OSUtils.hpp"

namespace ZeroTier {

static int _compareVersion(unsigned int maj1,unsigned int min1,unsigned int rev1,unsigned int b1,unsigned int maj2,unsigned int min2,unsigned int rev2,unsigned int b2)
{
	if (maj1 > maj2) {
		return 1;
	} else if (maj1 < maj2) {
		return -1;
	} else {
		if (min1 > min2) {
			return 1;
		} else if (min1 < min2) {
			return -1;
		} else {
			if (rev1 > rev2) {
				return 1;
			} else if (rev1 < rev2) {
				return -1;
			} else {
				if (b1 > b2) {
					return 1;
				} else if (b1 < b2) {
					return -1;
				} else {
					return 0;
				}
			}
		}
	}
}

SoftwareUpdater::SoftwareUpdater(Node &node,const std::string &homePath) :
	_node(node),
	_lastCheckTime(0),
	_homePath(homePath),
	_channel(ZT_SOFTWARE_UPDATE_DEFAULT_CHANNEL),
	_distLog((FILE *)0),
	_latestValid(false),
	_downloadLength(0)
{
	OSUtils::rm((_homePath + ZT_PATH_SEPARATOR_S ZT_SOFTWARE_UPDATE_BIN_FILENAME).c_str());
}

SoftwareUpdater::~SoftwareUpdater()
{
	if (_distLog)
		fclose(_distLog);
}

void SoftwareUpdater::setUpdateDistribution(bool distribute)
{
	_dist.clear();
	if (distribute) {
		_distLog = fopen((_homePath + ZT_PATH_SEPARATOR_S "update-dist.log").c_str(),"a");

		const std::string udd(_homePath + ZT_PATH_SEPARATOR_S "update-dist.d");
		const std::vector<std::string> ud(OSUtils::listDirectory(udd.c_str()));
		for(std::vector<std::string>::const_iterator u(ud.begin());u!=ud.end();++u) {
			// Each update has a companion .json file describing it. Other files are ignored.
			if ((u->length() > 5)&&(u->substr(u->length() - 5,5) == ".json")) {

				std::string buf;
				if (OSUtils::readFile((udd + ZT_PATH_SEPARATOR_S + *u).c_str(),buf)) {
					try {
						_D d;
						d.meta = OSUtils::jsonParse(buf); // throws on invalid JSON

						// If update meta is called e.g. foo.exe.json, then foo.exe is the update itself
						const std::string binPath(udd + ZT_PATH_SEPARATOR_S + u->substr(0,u->length() - 5));
						const std::string metaHash(OSUtils::jsonBinFromHex(d.meta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_HASH]));
						if ((metaHash.length() == 64)&&(OSUtils::readFile(binPath.c_str(),d.bin))) {
							std::array<uint8_t,64> sha512;
							SHA512(sha512.data(),d.bin.data(),(unsigned int)d.bin.length());
							if (!memcmp(sha512.data(),metaHash.data(),64)) { // double check that hash in JSON is correct
								d.meta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIZE] = d.bin.length(); // override with correct value -- setting this in meta json is optional
								std::array<uint8_t,16> shakey;
								memcpy(shakey.data(),sha512.data(),16);
								_dist[shakey] = d;
								if (_distLog) {
									fprintf(_distLog,".......... INIT: DISTRIBUTING %s (%u bytes)" ZT_EOL_S,binPath.c_str(),(unsigned int)d.bin.length());
									fflush(_distLog);
								}
							}
						}
					} catch ( ... ) {} // ignore bad meta JSON, etc.
				}

			}
		}
	} else {
		if (_distLog) {
			fclose(_distLog);
			_distLog = (FILE *)0;
		}
	}
}

void SoftwareUpdater::handleSoftwareUpdateUserMessage(uint64_t origin,const void *data,unsigned int len)
{
	if (!len) return;
	const MessageVerb v = (MessageVerb)reinterpret_cast<const uint8_t *>(data)[0];
	try {
		switch(v) {

			case VERB_GET_LATEST:
			case VERB_LATEST: {
				nlohmann::json req = OSUtils::jsonParse(std::string(reinterpret_cast<const char *>(data) + 1,len - 1)); // throws on invalid JSON
				if (req.is_object()) {
					const unsigned int rvMaj = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR],0);
					const unsigned int rvMin = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR],0);
					const unsigned int rvRev = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION],0);
					const unsigned int rvBld = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VERSION_BUILD],0);
					const unsigned int rvPlatform = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_PLATFORM],0);
					const unsigned int rvArch = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_ARCHITECTURE],0);
					const unsigned int rvVendor = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VENDOR],0);
					const std::string rvChannel(OSUtils::jsonString(req[ZT_SOFTWARE_UPDATE_JSON_CHANNEL],""));

					if (v == VERB_GET_LATEST) {

						if (_dist.size() > 0) {
							const nlohmann::json *latest = (const nlohmann::json *)0;
							const std::string expectedSigner = OSUtils::jsonString(req[ZT_SOFTWARE_UPDATE_JSON_EXPECT_SIGNED_BY],"");
							unsigned int bestVMaj = rvMaj;
							unsigned int bestVMin = rvMin;
							unsigned int bestVRev = rvRev;
							unsigned int bestVBld = rvBld;
							for(std::map< std::array<uint8_t,16>,_D >::const_iterator d(_dist.begin());d!=_dist.end();++d) {
								// The arch field in update description .json files can be an array for e.g. multi-arch update files
								const nlohmann::json &dvArch2 = d->second.meta[ZT_SOFTWARE_UPDATE_JSON_ARCHITECTURE];
								std::vector<unsigned int> dvArch;
								if (dvArch2.is_array()) {
									for(unsigned long i=0;i<dvArch2.size();++i)
										dvArch.push_back((unsigned int)OSUtils::jsonInt(dvArch2[i],0));
								} else {
									dvArch.push_back((unsigned int)OSUtils::jsonInt(dvArch2,0));
								}

								if ((OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_PLATFORM],0) == rvPlatform)&&
								    (std::find(dvArch.begin(),dvArch.end(),rvArch) != dvArch.end())&&
								    (OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VENDOR],0) == rvVendor)&&
								    (OSUtils::jsonString(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_CHANNEL],"") == rvChannel)&&
								    (OSUtils::jsonString(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNED_BY],"") == expectedSigner)) {
									const unsigned int dvMaj = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR],0);
									const unsigned int dvMin = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR],0);
									const unsigned int dvRev = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION],0);
									const unsigned int dvBld = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_BUILD],0);
									if (_compareVersion(dvMaj,dvMin,dvRev,dvBld,bestVMaj,bestVMin,bestVRev,bestVBld) > 0) {
										latest = &(d->second.meta);
										bestVMaj = dvMaj;
										bestVMin = dvMin;
										bestVRev = dvRev;
										bestVBld = dvBld;
									}
								}
							}
							if (latest) {
								std::string lj;
								lj.push_back((char)VERB_LATEST);
								lj.append(OSUtils::jsonDump(*latest));
								_node.sendUserMessage((void *)0,origin,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,lj.data(),(unsigned int)lj.length());
								if (_distLog) {
									fprintf(_distLog,"%.10llx GET_LATEST %u.%u.%u_%u platform %u arch %u vendor %u channel %s -> LATEST %u.%u.%u_%u" ZT_EOL_S,(unsigned long long)origin,rvMaj,rvMin,rvRev,rvBld,rvPlatform,rvArch,rvVendor,rvChannel.c_str(),bestVMaj,bestVMin,bestVRev,bestVBld);
									fflush(_distLog);
								}
							}
						} // else no reply, since we have nothing to distribute

					} else { // VERB_LATEST

						if ((origin == ZT_SOFTWARE_UPDATE_SERVICE)&&
							  (_compareVersion(rvMaj,rvMin,rvRev,rvBld,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION,ZEROTIER_ONE_VERSION_BUILD) > 0)&&
							  (OSUtils::jsonString(req[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNED_BY],"") == ZT_SOFTWARE_UPDATE_SIGNING_AUTHORITY)) {
							const unsigned long len = (unsigned long)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIZE],0);
							const std::string hash = OSUtils::jsonBinFromHex(req[ZT_SOFTWARE_UPDATE_JSON_UPDATE_HASH]);
							if ((len <= ZT_SOFTWARE_UPDATE_MAX_SIZE)&&(hash.length() >= 16)) {
								if (_latestMeta != req) {
									_latestMeta = req;
									_latestValid = false;
									OSUtils::rm((_homePath + ZT_PATH_SEPARATOR_S ZT_SOFTWARE_UPDATE_BIN_FILENAME).c_str());
									_download = std::string();
									memcpy(_downloadHashPrefix.data(),hash.data(),16);
									_downloadLength = len;
								}

								if ((_downloadLength > 0)&&(_download.length() < _downloadLength)) {
									Buffer<128> gd;
									gd.append((uint8_t)VERB_GET_DATA);
									gd.append(_downloadHashPrefix.data(),16);
									gd.append((uint32_t)_download.length());
									_node.sendUserMessage((void *)0,ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
								}
							}
						}
					}

				}
			}	break;

			case VERB_GET_DATA:
				if ((len >= 21)&&(_dist.size() > 0)) {
					unsigned long idx = (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 17) << 24;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 18) << 16;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 19) << 8;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 20);
					std::array<uint8_t,16> shakey;
					memcpy(shakey.data(),reinterpret_cast<const uint8_t *>(data) + 1,16);
					std::map< std::array<uint8_t,16>,_D >::iterator d(_dist.find(shakey));
					if ((d != _dist.end())&&(idx < (unsigned long)d->second.bin.length())) {
						Buffer<ZT_SOFTWARE_UPDATE_CHUNK_SIZE + 128> buf;
						buf.append((uint8_t)VERB_DATA);
						buf.append(reinterpret_cast<const uint8_t *>(data) + 1,16);
						buf.append((uint32_t)idx);
						buf.append(d->second.bin.data() + idx,std::min((unsigned long)ZT_SOFTWARE_UPDATE_CHUNK_SIZE,(unsigned long)(d->second.bin.length() - idx)));
						_node.sendUserMessage((void *)0,origin,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,buf.data(),buf.size());
					}
				}
				break;

			case VERB_DATA:
				if ((len >= 21)&&(_downloadLength > 0)&&(!memcmp(_downloadHashPrefix.data(),reinterpret_cast<const uint8_t *>(data) + 1,16))) {
					unsigned long idx = (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 17) << 24;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 18) << 16;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 19) << 8;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 20);
					if (idx == (unsigned long)_download.length()) {
						_download.append(reinterpret_cast<const char *>(data) + 21,len - 21);
						if (_download.length() < _downloadLength) {
							Buffer<128> gd;
							gd.append((uint8_t)VERB_GET_DATA);
							gd.append(_downloadHashPrefix.data(),16);
							gd.append((uint32_t)_download.length());
							_node.sendUserMessage((void *)0,ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
						}
					}
				}
				break;

			default:
				if (_distLog) {
					fprintf(_distLog,"%.10llx WARNING: bad update message verb==%u length==%u (unrecognized verb)" ZT_EOL_S,(unsigned long long)origin,(unsigned int)v,len);
					fflush(_distLog);
				}
				break;
		}
	} catch ( ... ) {
		if (_distLog) {
			fprintf(_distLog,"%.10llx WARNING: bad update message verb==%u length==%u (unexpected exception, likely invalid JSON)" ZT_EOL_S,(unsigned long long)origin,(unsigned int)v,len);
			fflush(_distLog);
		}
	}
}

bool SoftwareUpdater::check(const int64_t now)
{
	if ((now - _lastCheckTime) >= ZT_SOFTWARE_UPDATE_CHECK_PERIOD) {
		_lastCheckTime = now;
		char tmp[512];
		const unsigned int len = OSUtils::ztsnprintf(tmp,sizeof(tmp),
			"%c{\"" ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_VERSION_BUILD "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_EXPECT_SIGNED_BY "\":\"%s\","
			"\"" ZT_SOFTWARE_UPDATE_JSON_PLATFORM "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_ARCHITECTURE "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_VENDOR "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_CHANNEL "\":\"%s\"}",
			(char)VERB_GET_LATEST,
			ZEROTIER_ONE_VERSION_MAJOR,
			ZEROTIER_ONE_VERSION_MINOR,
			ZEROTIER_ONE_VERSION_REVISION,
			ZEROTIER_ONE_VERSION_BUILD,
			ZT_SOFTWARE_UPDATE_SIGNING_AUTHORITY,
			ZT_BUILD_PLATFORM,
			ZT_BUILD_ARCHITECTURE,
			(int)ZT_VENDOR_ZEROTIER,
			_channel.c_str());
		_node.sendUserMessage((void *)0,ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,tmp,len);
	}

	if (_latestValid)
		return true;

	if (_downloadLength > 0) {
		if (_download.length() >= _downloadLength) {
			// This is the very important security validation part that makes sure
			// this software update doesn't have cooties.

			const std::string binPath(_homePath + ZT_PATH_SEPARATOR_S ZT_SOFTWARE_UPDATE_BIN_FILENAME);
			try {
				// (1) Check the hash itself to make sure the image is basically okay
				uint8_t sha512[64];
				SHA512(sha512,_download.data(),(unsigned int)_download.length());
				char hexbuf[(64 * 2) + 2];
				if (OSUtils::jsonString(_latestMeta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_HASH],"") == Utils::hex(sha512,64,hexbuf)) {
					// (2) Check signature by signing authority
					const std::string sig(OSUtils::jsonBinFromHex(_latestMeta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNATURE]));
					if (Identity(ZT_SOFTWARE_UPDATE_SIGNING_AUTHORITY).verify(_download.data(),(unsigned int)_download.length(),sig.data(),(unsigned int)sig.length())) {
						// (3) Try to save file, and if so we are good.
						OSUtils::rm(binPath.c_str());
						if (OSUtils::writeFile(binPath.c_str(),_download)) {
							OSUtils::lockDownFile(binPath.c_str(),false);
							_latestValid = true;
							_download = std::string();
							_downloadLength = 0;
							return true;
						}
					}
				}
			} catch ( ... ) {} // any exception equals verification failure

			// If we get here, checks failed.
			OSUtils::rm(binPath.c_str());
			_latestMeta = nlohmann::json();
			_latestValid = false;
			_download = std::string();
			_downloadLength = 0;
		} else {
			Buffer<128> gd;
			gd.append((uint8_t)VERB_GET_DATA);
			gd.append(_downloadHashPrefix.data(),16);
			gd.append((uint32_t)_download.length());
			_node.sendUserMessage((void *)0,ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
		}
	}

	return false;
}

void SoftwareUpdater::apply()
{
	std::string updatePath(_homePath + ZT_PATH_SEPARATOR_S ZT_SOFTWARE_UPDATE_BIN_FILENAME);
	if ((_latestMeta.is_object())&&(_latestValid)&&(OSUtils::fileExists(updatePath.c_str(),false))) {
#ifdef __WINDOWS__
		std::string cmdArgs(OSUtils::jsonString(_latestMeta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_EXEC_ARGS],""));
		if (cmdArgs.length() > 0) {
			updatePath.push_back(' ');
			updatePath.append(cmdArgs);
		}
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		memset(&si,0,sizeof(si));
		memset(&pi,0,sizeof(pi));
		CreateProcessA(NULL,const_cast<LPSTR>(updatePath.c_str()),NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP,NULL,NULL,&si,&pi);
		// Windows doesn't exit here -- updater will stop the service during update, etc. -- but we do want to stop multiple runs from happening
		_latestMeta = nlohmann::json();
		_latestValid = false;
#else
		char *argv[256];
		unsigned long ac = 0;
		argv[ac++] = const_cast<char *>(updatePath.c_str());
		const std::vector<std::string> argsSplit(OSUtils::split(OSUtils::jsonString(_latestMeta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_EXEC_ARGS],"").c_str()," ","\\","\""));
		for(std::vector<std::string>::const_iterator a(argsSplit.begin());a!=argsSplit.end();++a) {
			argv[ac] = const_cast<char *>(a->c_str());
			if (++ac == 255) break;
		}
		argv[ac] = (char *)0;
		chmod(updatePath.c_str(),0700);

		// Close all open file descriptors except stdout/stderr/etc.
		int minMyFd = STDIN_FILENO;
		if (STDOUT_FILENO > minMyFd) minMyFd = STDOUT_FILENO;
		if (STDERR_FILENO > minMyFd) minMyFd = STDERR_FILENO;
		++minMyFd;
#ifdef _SC_OPEN_MAX
		int maxMyFd = (int)sysconf(_SC_OPEN_MAX);
		if (maxMyFd <= minMyFd)
			maxMyFd = 65536;
#else
		int maxMyFd = 65536;
#endif
		while (minMyFd < maxMyFd)
			close(minMyFd++);

		execv(updatePath.c_str(),argv);
		fprintf(stderr,"FATAL: unable to execute software update binary at %s\n",updatePath.c_str());
		exit(1);
#endif
	}
}

} // namespace ZeroTier
