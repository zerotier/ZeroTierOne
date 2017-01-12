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

#ifndef ZT_BUILD_ARCHITECTURE
#define ZT_BUILD_ARCHITECTURE 0
#endif
#ifndef ZT_BUILD_PLATFORM
#define ZT_BUILD_PLATFORM 0
#endif

namespace ZeroTier {

#if 0
#ifdef ZT_AUTO_UPDATE
#define ZT_AUTO_UPDATE_MAX_HTTP_RESPONSE_SIZE (1024 * 1024 * 64)
#define ZT_AUTO_UPDATE_CHECK_PERIOD 21600000
class BackgroundSoftwareUpdateChecker
{
public:
	bool isValidSigningIdentity(const Identity &id)
	{
		return (
			/* 0001 - 0004 : obsolete, used in old versions */
		  /* 0005 */   (id == Identity("ba57ea350e:0:9d4be6d7f86c5660d5ee1951a3d759aa6e12a84fc0c0b74639500f1dbc1a8c566622e7d1c531967ebceb1e9d1761342f88324a8ba520c93c35f92f35080fa23f"))
		  /* 0006 */ ||(id == Identity("5067b21b83:0:8af477730f5055c48135b84bed6720a35bca4c0e34be4060a4c636288b1ec22217eb22709d610c66ed464c643130c51411bbb0294eef12fbe8ecc1a1e2c63a7a"))
		  /* 0007 */ ||(id == Identity("4f5e97a8f1:0:57880d056d7baeb04bbc057d6f16e6cb41388570e87f01492fce882485f65a798648595610a3ad49885604e7fb1db2dd3c2c534b75e42c3c0b110ad07b4bb138"))
		  /* 0008 */ ||(id == Identity("580bbb8e15:0:ad5ef31155bebc6bc413991992387e083fed26d699997ef76e7c947781edd47d1997161fa56ba337b1a2b44b129fd7c7197ce5185382f06011bc88d1363b4ddd"))
		);
	}

	void doUpdateCheck()
	{
		std::string url(OneService::autoUpdateUrl());
		if ((url.length() <= 7)||(url.substr(0,7) != "http://"))
			return;

		std::string httpHost;
		std::string httpPath;
		{
			std::size_t slashIdx = url.substr(7).find_first_of('/');
			if (slashIdx == std::string::npos) {
				httpHost = url.substr(7);
				httpPath = "/";
			} else {
				httpHost = url.substr(7,slashIdx);
				httpPath = url.substr(slashIdx + 7);
			}
		}
		if (httpHost.length() == 0)
			return;

		std::vector<InetAddress> ips(OSUtils::resolve(httpHost.c_str()));
		for(std::vector<InetAddress>::iterator ip(ips.begin());ip!=ips.end();++ip) {
			if (!ip->port())
				ip->setPort(80);
			std::string nfoPath = httpPath + "LATEST.nfo";
			std::map<std::string,std::string> requestHeaders,responseHeaders;
			std::string body;
			requestHeaders["Host"] = httpHost;
			unsigned int scode = Http::GET(ZT_AUTO_UPDATE_MAX_HTTP_RESPONSE_SIZE,60000,reinterpret_cast<const struct sockaddr *>(&(*ip)),nfoPath.c_str(),requestHeaders,responseHeaders,body);
			//fprintf(stderr,"UPDATE %s %s %u %lu\n",ip->toString().c_str(),nfoPath.c_str(),scode,body.length());
			if ((scode == 200)&&(body.length() > 0)) {
				/* NFO fields:
				 *
				 * file=<filename>
				 * signedBy=<signing identity>
				 * ed25519=<ed25519 ECC signature of archive in hex>
				 * vMajor=<major version>
				 * vMinor=<minor version>
				 * vRevision=<revision> */
				Dictionary<4096> nfo(body.c_str());
				char tmp[2048];

				if (nfo.get("vMajor",tmp,sizeof(tmp)) <= 0) return;
				const unsigned int vMajor = Utils::strToUInt(tmp);
				if (nfo.get("vMinor",tmp,sizeof(tmp)) <= 0) return;
				const unsigned int vMinor = Utils::strToUInt(tmp);
				if (nfo.get("vRevision",tmp,sizeof(tmp)) <= 0) return;
				const unsigned int vRevision = Utils::strToUInt(tmp);
				if (Utils::compareVersion(vMajor,vMinor,vRevision,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION) <= 0) {
					//fprintf(stderr,"UPDATE %u.%u.%u is not newer than our version\n",vMajor,vMinor,vRevision);
					return;
				}

				if (nfo.get("signedBy",tmp,sizeof(tmp)) <= 0) return;
				Identity signedBy;
				if ((!signedBy.fromString(tmp))||(!isValidSigningIdentity(signedBy))) {
					//fprintf(stderr,"UPDATE invalid signedBy or not authorized signing identity.\n");
					return;
				}

				if (nfo.get("file",tmp,sizeof(tmp)) <= 0) return;
				std::string filePath(tmp);
				if ((!filePath.length())||(filePath.find("..") != std::string::npos))
					return;
				filePath = httpPath + filePath;

				std::string fileData;
				if (Http::GET(ZT_AUTO_UPDATE_MAX_HTTP_RESPONSE_SIZE,60000,reinterpret_cast<const struct sockaddr *>(&(*ip)),filePath.c_str(),requestHeaders,responseHeaders,fileData) != 200) {
					//fprintf(stderr,"UPDATE GET %s failed\n",filePath.c_str());
					return;
				}

				if (nfo.get("ed25519",tmp,sizeof(tmp)) <= 0) return;
				std::string ed25519(Utils::unhex(tmp));
				if ((ed25519.length() == 0)||(!signedBy.verify(fileData.data(),(unsigned int)fileData.length(),ed25519.data(),(unsigned int)ed25519.length()))) {
					//fprintf(stderr,"UPDATE %s failed signature check!\n",filePath.c_str());
					return;
				}

				/* --------------------------------------------------------------- */
				/* We made it! Begin OS-specific installation code. */

#ifdef __APPLE__
				/* OSX version is in the form of a MacOSX .pkg file, so we will
				 * launch installer (normally in /usr/sbin) to install it. It will
				 * then turn around and shut down the service, update files, and
				 * relaunch. */
				{
					char bashp[128],pkgp[128];
					Utils::snprintf(bashp,sizeof(bashp),"/tmp/ZeroTierOne-update-%u.%u.%u.sh",vMajor,vMinor,vRevision);
					Utils::snprintf(pkgp,sizeof(pkgp),"/tmp/ZeroTierOne-update-%u.%u.%u.pkg",vMajor,vMinor,vRevision);
					FILE *pkg = fopen(pkgp,"w");
					if ((!pkg)||(fwrite(fileData.data(),fileData.length(),1,pkg) != 1)) {
						fclose(pkg);
						unlink(bashp);
						unlink(pkgp);
						fprintf(stderr,"UPDATE error writing %s\n",pkgp);
						return;
					}
					fclose(pkg);
					FILE *bash = fopen(bashp,"w");
					if (!bash) {
						fclose(pkg);
						unlink(bashp);
						unlink(pkgp);
						fprintf(stderr,"UPDATE error writing %s\n",bashp);
						return;
					}
					fprintf(bash,
						"#!/bin/bash\n"
						"export PATH=/bin:/usr/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin\n"
						"sleep 1\n"
						"installer -pkg \"%s\" -target /\n"
						"sleep 1\n"
						"rm -f \"%s\" \"%s\"\n"
						"exit 0\n",
						pkgp,
						pkgp,
						bashp);
					fclose(bash);
					long pid = (long)vfork();
					if (pid == 0) {
						setsid(); // detach from parent so that shell isn't killed when parent is killed
						signal(SIGHUP,SIG_IGN);
						signal(SIGTERM,SIG_IGN);
						signal(SIGQUIT,SIG_IGN);
						execl("/bin/bash","/bin/bash",bashp,(char *)0);
						exit(0);
					}
				}
#endif // __APPLE__

#ifdef __WINDOWS__
				/* Windows version comes in the form of .MSI package that
				 * takes care of everything. */
				{
					char tempp[512],batp[512],msip[512],cmdline[512];
					if (GetTempPathA(sizeof(tempp),tempp) <= 0)
						return;
					CreateDirectoryA(tempp,(LPSECURITY_ATTRIBUTES)0);
					Utils::snprintf(batp,sizeof(batp),"%s\\ZeroTierOne-update-%u.%u.%u.bat",tempp,vMajor,vMinor,vRevision);
					Utils::snprintf(msip,sizeof(msip),"%s\\ZeroTierOne-update-%u.%u.%u.msi",tempp,vMajor,vMinor,vRevision);
					FILE *msi = fopen(msip,"wb");
					if ((!msi)||(fwrite(fileData.data(),(size_t)fileData.length(),1,msi) != 1)) {
						fclose(msi);
						return;
					}
					fclose(msi);
					FILE *bat = fopen(batp,"wb");
					if (!bat)
						return;
					fprintf(bat,
						"TIMEOUT.EXE /T 1 /NOBREAK\r\n"
						"NET.EXE STOP \"ZeroTierOneService\"\r\n"
						"TIMEOUT.EXE /T 1 /NOBREAK\r\n"
						"MSIEXEC.EXE /i \"%s\" /qn\r\n"
						"TIMEOUT.EXE /T 1 /NOBREAK\r\n"
						"NET.EXE START \"ZeroTierOneService\"\r\n"
						"DEL \"%s\"\r\n"
						"DEL \"%s\"\r\n",
						msip,
						msip,
						batp);
					fclose(bat);
					STARTUPINFOA si;
					PROCESS_INFORMATION pi;
					memset(&si,0,sizeof(si));
					memset(&pi,0,sizeof(pi));
					Utils::snprintf(cmdline,sizeof(cmdline),"CMD.EXE /c \"%s\"",batp);
					CreateProcessA(NULL,cmdline,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP,NULL,NULL,&si,&pi);
				}
#endif // __WINDOWS__

				/* --------------------------------------------------------------- */

				return;
			} // else try to fetch from next IP address
		}
	}

	void threadMain()
		throw()
	{
		try {
			this->doUpdateCheck();
		} catch ( ... ) {}
	}
};
static BackgroundSoftwareUpdateChecker backgroundSoftwareUpdateChecker;
#endif // ZT_AUTO_UPDATE
#endif

SoftwareUpdater::SoftwareUpdater(Node &node,const std::string &homePath) :
	_node(node),
	_lastCheckTime(0),
	_homePath(homePath),
	_latestBinLength(0),
	_latestBinValid(false)
{
}

SoftwareUpdater::~SoftwareUpdater()
{
}

void SoftwareUpdater::loadUpdatesToDistribute()
{
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
							printf("update-dist.d: %s\n",u->c_str());
						}
					}
				} catch ( ... ) {} // ignore bad meta JSON, etc.
			}
		}
	}
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
					const unsigned int rvPlatform = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_PLATFORM],0);
					const unsigned int rvArch = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_ARCHITECTURE],0);
					const unsigned int rvVendor = (unsigned int)OSUtils::jsonInt(req[ZT_SOFTWARE_UPDATE_JSON_VENDOR],0);
					const std::string rvChannel(OSUtils::jsonString(req[ZT_SOFTWARE_UPDATE_JSON_CHANNEL],""));
					if (v == VERB_GET_LATEST) {

						if (_dist.size() > 0) {
							const nlohmann::json *latest = (const nlohmann::json *)0;
							const std::string expectedSigner = OSUtils::jsonString(req[ZT_SOFTWARE_UPDATE_JSON_EXPECT_SIGNED_BY],"");
							for(std::map< Array<uint8_t,16>,_D >::const_iterator d(_dist.begin());d!=_dist.end();++d) {
								if ((OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_PLATFORM],0) == rvPlatform)&&
								    (OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_ARCHITECTURE],0) == rvArch)&&
								    (OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VENDOR],0) == rvVendor)&&
								    (OSUtils::jsonString(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_CHANNEL],"") == rvChannel)&&
								    (OSUtils::jsonString(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNED_BY],"") == expectedSigner)) {
									const unsigned int dvMaj = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR],0);
									const unsigned int dvMin = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR],0);
									const unsigned int dvRev = (unsigned int)OSUtils::jsonInt(d->second.meta[ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION],0);
									if (Utils::compareVersion(dvMaj,dvMin,dvRev,rvMaj,rvMin,rvRev) > 0) {
										latest = &(d->second.meta);
									}
								}
							}
							if (latest) {
								std::string lj;
								lj.push_back((char)VERB_LATEST);
								lj.append(OSUtils::jsonDump(*latest));
								_node.sendUserMessage(origin,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,lj.data(),(unsigned int)lj.length());
								printf(">> LATEST\n%s\n",OSUtils::jsonDump(*latest).c_str());
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
									printf("<< LATEST\n%s\n",OSUtils::jsonDump(req).c_str());
								}

								Buffer<128> gd;
								gd.append((uint8_t)VERB_GET_DATA);
								gd.append(_latestBinHashPrefix.data,16);
								gd.append((uint32_t)_latestBin.length());
								_node.sendUserMessage(ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
								printf(">> GET_DATA @%u\n",(unsigned int)_latestBin.length());
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
					printf("<< GET_DATA @%u from %.10llx for %s\n",(unsigned int)idx,origin,Utils::hex(reinterpret_cast<const uint8_t *>(data) + 1,16).c_str());
					std::map< Array<uint8_t,16>,_D >::iterator d(_dist.find(Array<uint8_t,16>(reinterpret_cast<const uint8_t *>(data) + 1)));
					if ((d != _dist.end())&&(idx < d->second.bin.length())) {
						Buffer<ZT_SOFTWARE_UPDATE_CHUNK_SIZE + 128> buf;
						buf.append((uint8_t)VERB_DATA);
						buf.append(reinterpret_cast<const uint8_t *>(data) + 1,16);
						buf.append((uint32_t)idx);
						buf.append(d->second.bin.data() + idx,std::min((unsigned long)ZT_SOFTWARE_UPDATE_CHUNK_SIZE,(unsigned long)(d->second.bin.length() - idx)));
						_node.sendUserMessage(origin,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,buf.data(),buf.size());
						printf(">> DATA @%u\n",(unsigned int)idx);
					}
				}
				break;

			case VERB_DATA:
				if ((len >= 21)&&(!memcmp(_latestBinHashPrefix.data,reinterpret_cast<const uint8_t *>(data) + 1,16))) {
					unsigned long idx = (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 17) << 24;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 18) << 16;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 19) << 8;
					idx |= (unsigned long)*(reinterpret_cast<const uint8_t *>(data) + 20);
					if (idx == _latestBin.length()) {
						_latestBin.append(reinterpret_cast<const char *>(data) + 21,len - 21);
					}
					printf("<< DATA @%u / %u bytes (we now have %u bytes)\n",(unsigned int)idx,(unsigned int)(len - 21),(unsigned int)_latestBin.length());

					if (_latestBin.length() < _latestBinLength) {
						Buffer<128> gd;
						gd.append((uint8_t)VERB_GET_DATA);
						gd.append(_latestBinHashPrefix.data,16);
						gd.append((uint32_t)_latestBin.length());
						_node.sendUserMessage(ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,gd.data(),gd.size());
						printf(">> GET_DATA @%u\n",(unsigned int)_latestBin.length());
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

nlohmann::json SoftwareUpdater::check(const uint64_t now)
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
							printf("VALID UPDATE\n%s\n",OSUtils::jsonDump(_latestMeta).c_str());
							return _latestMeta;
						}
					}
				} catch ( ... ) {} // any exception equals verification failure
				printf("INVALID UPDATE (!!!)\n%s\n",OSUtils::jsonDump(_latestMeta).c_str());

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
			printf(">> GET_DATA @%u\n",(unsigned int)_latestBin.length());
		}
	}

	if ((now - _lastCheckTime) >= ZT_SOFTWARE_UPDATE_CHECK_PERIOD) {
		_lastCheckTime = now;
		char tmp[512];
		const unsigned int len = Utils::snprintf(tmp,sizeof(tmp),
			"%c{\"" ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_EXPECT_SIGNED_BY "\":\"%s\","
			"\"" ZT_SOFTWARE_UPDATE_JSON_PLATFORM "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_ARCHITECTURE "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_VENDOR "\":%d,"
			"\"" ZT_SOFTWARE_UPDATE_JSON_CHANNEL "\":\"%s\"}",
			(char)VERB_GET_LATEST,
			ZEROTIER_ONE_VERSION_MAJOR,
			ZEROTIER_ONE_VERSION_MINOR,
			ZEROTIER_ONE_VERSION_REVISION,
			ZT_SOFTWARE_UPDATE_SIGNING_AUTHORITY,
			ZT_BUILD_PLATFORM,
			ZT_BUILD_ARCHITECTURE,
			(int)ZT_VENDOR_ZEROTIER,
			"release");
		_node.sendUserMessage(ZT_SOFTWARE_UPDATE_SERVICE,ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE,tmp,len);
		printf(">> GET_LATEST\n");
	}

	return nlohmann::json();
}

void SoftwareUpdater::apply()
{
	if ((_latestBin.length() == _latestBinLength)&&(_latestBinLength > 0)&&(_latestBinValid)) {
	}
}

} // namespace ZeroTier
