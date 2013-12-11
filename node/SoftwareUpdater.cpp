/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#include "../version.h"

#include "SoftwareUpdater.hpp"
#include "Dictionary.hpp"
#include "C25519.hpp"
#include "Identity.hpp"
#include "Logger.hpp"
#include "RuntimeEnvironment.hpp"
#include "Thread.hpp"
#include "Node.hpp"

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
		LOG("unable to check for software updates, response code %d (%s)",code,body.c_str());
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}

	try {
		Dictionary nfo(body);
		const unsigned int vMajor = Utils::strToUInt(nfo.get("vMajor").c_str());
		const unsigned int vMinor = Utils::strToUInt(nfo.get("vMinor").c_str());
		const unsigned int vRevision = Utils::strToUInt(nfo.get("vRevision").c_str());
		const Address signedBy(nfo.get("signedBy"));
		const std::string signature(Utils::unhex(nfo.get("ed25519")));
		const std::string &url = nfo.get("url");

		if (signature.length() != ZT_C25519_SIGNATURE_LEN) {
			LOG("software update aborted: .nfo file invalid: bad Ed25519 signature");
			upd->_status = UPDATE_STATUS_IDLE;
			return;
		}
		if ((url.length() <= 7)||(url.substr(0,7) != "http://")) {
			LOG("software update aborted: .nfo file invalid: update URL must begin with http://");
			upd->_status = UPDATE_STATUS_IDLE;
			return;
		}
		if (packVersion(vMajor,vMinor,vRevision) <= upd->_myVersion) {
			LOG("software update aborted: .nfo file invalid: version on web site <= my version");
			upd->_status = UPDATE_STATUS_IDLE;
			return;
		}

		if (!ZT_DEFAULTS.updateAuthorities.count(signedBy)) {
			LOG("software update aborted: .nfo file specifies unknown signing authority");
			upd->_status = UPDATE_STATUS_IDLE;
			return;
		}

		upd->_status = UPDATE_STATUS_GETTING_FILE;
		upd->_signedBy = signedBy;
		upd->_signature = signature;

		HttpClient::GET(url,HttpClient::NO_HEADERS,ZT_UPDATE_HTTP_TIMEOUT,&_cbHandleGetLatestVersionBinary,arg);
	} catch ( ... ) {
		LOG("software update check failed: .nfo file invalid: fields missing or invalid dictionary format");
		upd->_status = UPDATE_STATUS_IDLE;
	}
}

void SoftwareUpdater::_cbHandleGetLatestVersionBinary(void *arg,int code,const std::string &url,bool onDisk,const std::string &body)
{
	SoftwareUpdater *upd = (SoftwareUpdater *)arg;
	const RuntimeEnvironment *_r = (const RuntimeEnvironment *)upd->_r;
	Mutex::Lock _l(upd->_lock);

	std::map< Address,Identity >::const_iterator updateAuthority = ZT_DEFAULTS.updateAuthorities.find(upd->_signedBy);
	if (updateAuthority == ZT_DEFAULTS.updateAuthorities.end()) { // sanity check, shouldn't happen
		LOG("software update aborted: .nfo file specifies unknown signing authority");
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}

	// The all-important authenticity check... :)
	if (!updateAuthority->second.verify(body.data(),body.length(),upd->_signature.data(),upd->_signature.length())) {
		LOG("software update aborted: update fetched from '%s' failed certificate check against signer %s",url.c_str(),updateAuthority->first.toString().c_str());
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}

#ifdef __UNIX_LIKE__
	size_t lastSlash = url.rfind('/');
	if (lastSlash == std::string::npos) { // sanity check, shouldn't happen
		LOG("software update aborted: invalid URL");
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}
	std::string updatesDir(_r->homePath + ZT_PATH_SEPARATOR_S + "updates.d");
	std::string updatePath(updatesDir + ZT_PATH_SEPARATOR_S + url.substr(lastSlash + 1));
	mkdir(updatesDir.c_str(),0755);

	int fd = ::open(updatePath.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0755);
	if (fd <= 0) {
		LOG("software update aborted: unable to open %s for writing",updatePath.c_str());
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}
	if ((long)::write(fd,body.data(),body.length()) != (long)body.length()) {
		LOG("software update aborted: unable to write to %s",updatePath.c_str());
		upd->_status = UPDATE_STATUS_IDLE;
		return;
	}
	::close(fd);
	::chmod(updatePath.c_str(),0755);

	upd->_status = UPDATE_STATUS_IDLE;

	_r->node->terminate(Node::NODE_RESTART_FOR_UPGRADE,updatePath.c_str());
#endif

#ifdef __WINDOWS__
	todo;
#endif
}

} // namespace ZeroTier
