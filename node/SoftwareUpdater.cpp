/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdexcept>

#include "../version.h"

#include "SoftwareUpdater.hpp"
#include "Dictionary.hpp"
#include "C25519.hpp"
#include "Identity.hpp"
#include "Logger.hpp"
#include "RuntimeEnvironment.hpp"
#include "Thread.hpp"
#include "Node.hpp"
#include "Utils.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace ZeroTier {

SoftwareUpdater::SoftwareUpdater(const RuntimeEnvironment *renv) :
	_r(renv),
	_myVersion(packVersion(ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION)),
	_lastUpdateAttempt(0),
	_status(UPDATE_STATUS_IDLE),
	_die(false),
	_lock()
{
}

SoftwareUpdater::~SoftwareUpdater()
{
	_die = true;
	for(;;) {
		_lock.lock();
		bool ip = (_status != UPDATE_STATUS_IDLE);
		_lock.unlock();
		if (ip)
			Thread::sleep(500);
		else break;
	}
}

void SoftwareUpdater::cleanOldUpdates()
{
	std::string updatesDir(_r->homePath + ZT_PATH_SEPARATOR_S + "updates.d");
	std::map<std::string,bool> dl(Utils::listDirectory(updatesDir.c_str()));
	for(std::map<std::string,bool>::iterator i(dl.begin());i!=dl.end();++i) {
		if (!i->second)
			Utils::rm((updatesDir + ZT_PATH_SEPARATOR_S + i->first).c_str());
	}
}

const char *SoftwareUpdater::parseNfo(
	const char *nfoText,
	unsigned int &vMajor,
	unsigned int &vMinor,
	unsigned int &vRevision,
	Address &signedBy,
	std::string &signature,
	std::string &url)
{
	try {
		Dictionary nfo(nfoText);

		vMajor = Utils::strToUInt(nfo.get("vMajor").c_str());
		vMinor = Utils::strToUInt(nfo.get("vMinor").c_str());
		vRevision = Utils::strToUInt(nfo.get("vRevision").c_str());
		signedBy = nfo.get("signedBy");
		signature = Utils::unhex(nfo.get("ed25519"));
		url = nfo.get("url");

		if (signature.length() != ZT_C25519_SIGNATURE_LEN)
			return "bad ed25519 signature, invalid length";
		if ((url.length() <= 7)||(url.substr(0,7) != "http://"))
			return "invalid URL, must begin with http://";

		return (const char *)0;
	} catch ( ... ) {
		return "invalid NFO file format or one or more required fields missing";
	}
}

bool SoftwareUpdater::validateUpdate(
	const void *data,
	unsigned int len,
	const Address &signedBy,
	const std::string &signature)
{
	std::map< Address,Identity >::const_iterator updateAuthority = ZT_DEFAULTS.updateAuthorities.find(signedBy);
	if (updateAuthority == ZT_DEFAULTS.updateAuthorities.end())
		return false;
	return updateAuthority->second.verify(data,len,signature.data(),(unsigned int)signature.length());
}

void SoftwareUpdater::_cbHandleGetLatestVersionInfo(void *arg,int code,const std::string &url,bool onDisk,const std::string &body)
{
	SoftwareUpdater *upd = (SoftwareUpdater *)arg;
	const RuntimeEnvironment *_r = (const RuntimeEnvironment *)upd->_r;
	Mutex::Lock _l(upd->_lock);

	if ((upd->_die)||(upd->_status != UPDATE_STATUS_GETTING_NFO)) {
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}

	if (code != 200) {
		LOG("software update check failed: server responded with code %d",code);
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}

	try {
		unsigned int vMajor = 0,vMinor = 0,vRevision = 0;
		Address signedBy;
		std::string signature,url;

		const char *err = parseNfo(body.c_str(),vMajor,vMinor,vRevision,signedBy,signature,url);

		if (err) {
			LOG("software update check aborted: .nfo file parse error: %s",err);
			upd->_status = UPDATE_STATUS_IDLE;
			return;
		}

		if (!ZT_DEFAULTS.updateAuthorities.count(signedBy)) {
			LOG("software update check aborted: .nfo file specifies unknown signing authority");
			upd->_status = UPDATE_STATUS_IDLE;
			return;
		}

#ifndef ZT_ALWAYS_UPDATE /* for testing */
		if (packVersion(vMajor,vMinor,vRevision) <= upd->_myVersion) {
			LOG("software update check complete: version on update site is not newer than my version, no update necessary");
			upd->_status = UPDATE_STATUS_IDLE;
			return;
		}
#endif

		upd->_status = UPDATE_STATUS_GETTING_FILE;
		upd->_signedBy = signedBy;
		upd->_signature = signature;

		HttpClient::GET(url,HttpClient::NO_HEADERS,ZT_UPDATE_HTTP_TIMEOUT,&_cbHandleGetLatestVersionBinary,arg);
	} catch ( ... ) {
		LOG("software update check failed: .nfo file invalid or missing field(s)");
		upd->_status = UPDATE_STATUS_IDLE;
	}
}

void SoftwareUpdater::_cbHandleGetLatestVersionBinary(void *arg,int code,const std::string &url,bool onDisk,const std::string &body)
{
	SoftwareUpdater *upd = (SoftwareUpdater *)arg;
	const RuntimeEnvironment *_r = (const RuntimeEnvironment *)upd->_r;
	Mutex::Lock _l(upd->_lock);

	if (!validateUpdate(body.data(),(unsigned int)body.length(),upd->_signedBy,upd->_signature)) {
		LOG("software update failed: update fetched from '%s' failed signature check (image size: %u)",url.c_str(),(unsigned int)body.length());
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}

	size_t lastSlash = url.rfind('/');
	if (lastSlash == std::string::npos) { // sanity check, shouldn't happen
		LOG("software update failed: invalid URL");
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}
	std::string updatesDir(_r->homePath + ZT_PATH_SEPARATOR_S + "updates.d");
	std::string updatePath(updatesDir + ZT_PATH_SEPARATOR_S + url.substr(lastSlash + 1));
#ifdef __WINDOWS__
	CreateDirectoryA(updatesDir.c_str(),NULL);
#else
	mkdir(updatesDir.c_str(),0755);
#endif

	FILE *upf = fopen(updatePath.c_str(),"wb");
	if (!upf) {
		LOG("software update failed: unable to open %s for writing",updatePath.c_str());
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}
	if (fwrite(body.data(),body.length(),1,upf) != 1) {
		LOG("software update failed: unable to write to %s",updatePath.c_str());
		upd->_status = UPDATE_STATUS_IDLE;
		fclose(upf);
		Utils::rm(updatePath);
		return;
	}
	fclose(upf);

#ifdef __UNIX_LIKE__
	::chmod(updatePath.c_str(),0755);
#endif

	// We exit with this reason code and the path as the text. It is the
	// caller's responsibility (main.c) to pick this up and do the right
	// thing.
	upd->_status = UPDATE_STATUS_IDLE;
	_r->node->terminate(Node::NODE_RESTART_FOR_UPGRADE,updatePath.c_str());
}

} // namespace ZeroTier
