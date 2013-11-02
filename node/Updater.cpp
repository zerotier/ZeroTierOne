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

#include "Updater.hpp"
#include "RuntimeEnvironment.hpp"
#include "Logger.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"
#include "Topology.hpp"

#include "../version.h"

namespace ZeroTier {

Updater::Updater(const RuntimeEnvironment *renv) :
	_r(renv),
	_download((_Download *)0)
{
	refreshShared();
}

Updater::~Updater()
{
	Mutex::Lock _l(_lock);
	delete _download;
}

void Updater::refreshShared()
{
	std::string updatesPath(_r->homePath + ZT_PATH_SEPARATOR_S + "updates.d");
	std::map<std::string,bool> ud(Utils::listDirectory(updatesPath.c_str()));

	Mutex::Lock _l(_lock);
	_sharedUpdates.clear();
	for(std::map<std::string,bool>::iterator u(ud.begin());u!=ud.end();++u) {
		if (u->second)
			continue; // skip directories
		if ((u->first.length() >= 4)&&(!strcasecmp(u->first.substr(u->first.length() - 4).c_str(),".nfo")))
			continue; // skip .nfo companion files

		std::string fullPath(updatesPath + ZT_PATH_SEPARATOR_S + u->first);
		std::string nfoPath(fullPath + ".nfo");

		std::string buf;
		if (Utils::readFile(nfoPath.c_str(),buf)) {
			Dictionary nfo(buf);

			_Shared shared;
			shared.filename = fullPath;

			std::string sha512(Utils::unhex(nfo.get("sha512",std::string())));
			if (sha512.length() < sizeof(shared.sha512)) {
				TRACE("skipped shareable update due to missing fields in companion .nfo: %s",fullPath.c_str());
				continue;
			}
			memcpy(shared.sha512,sha512.data(),sizeof(shared.sha512));

			std::string sig(Utils::unhex(nfo.get("sha512sig_ed25519",std::string())));
			if (sig.length() < shared.sig.size()) {
				TRACE("skipped shareable update due to missing fields in companion .nfo: %s",fullPath.c_str());
				continue;
			}
			memcpy(shared.sig.data,sig.data(),shared.sig.size());

			// Check signature to guard against updates.d being used as a data
			// exfiltration mechanism. We will only share properly signed updates,
			// nothing else.
			Address signedBy(nfo.get("signedBy",std::string()));
			std::map< Address,Identity >::const_iterator authority(ZT_DEFAULTS.updateAuthorities.find(signedBy));
			if ((authority == ZT_DEFAULTS.updateAuthorities.end())||(!authority->second.verify(shared.sha512,64,shared.sig))) {
				TRACE("skipped shareable update: not signed by valid authority or signature invalid: %s",fullPath.c_str());
				continue;
			}
			shared.signedBy = signedBy;

			int64_t fs = Utils::getFileSize(fullPath.c_str());
			if (fs <= 0) {
				TRACE("skipped shareable update due to unreadable, invalid, or 0-byte file: %s",fullPath.c_str());
				continue;
			}
			shared.size = (unsigned long)fs;

			Array<unsigned char,16> first16Bytes;
			memcpy(first16Bytes.data,sha512.data(),16);
			_sharedUpdates[first16Bytes] = shared;
		} else {
			TRACE("skipped shareable update due to missing companion .nfo: %s",fullPath.c_str());
			continue;
		}
	}
}

void Updater::getUpdateIfThisIsNewer(unsigned int vMajor,unsigned int vMinor,unsigned int revision)
{
	if (vMajor < ZEROTIER_ONE_VERSION_MAJOR)
		return;
	else if (vMajor == ZEROTIER_ONE_VERSION_MAJOR) {
		if (vMinor < ZEROTIER_ONE_VERSION_MINOR)
			return;
		else if (vMinor == ZEROTIER_ONE_VERSION_MINOR) {
			if (revision <= ZEROTIER_ONE_VERSION_REVISION)
				return;
		}
	}

	std::string updateFilename(generateUpdateFilename());
	if (!updateFilename.length()) {
		TRACE("a new update to %u.%u.%u is available, but this platform doesn't support auto updates",vMajor,vMinor,revision);
		return;
	}

	std::vector< SharedPtr<Peer> > peers;
	_r->topology->eachPeer(Topology::CollectPeersWithActiveDirectPath(peers,Utils::now()));

	TRACE("new update available to %u.%u.%u, looking for %s from %u peers",vMajor,vMinor,revision,updateFilename.c_str(),(unsigned int)peers.size());

	if (!peers.size())
		return;

	for(std::vector< SharedPtr<Peer> >::iterator p(peers.begin());p!=peers.end();++p) {
		Packet outp(p->address(),_r->identity.address(),Packet::VERB_FILE_INFO_REQUEST);
		outp.append((unsigned char)0);
		outp.append((uint16_t)updateFilename.length());
		outp.append(updateFilename.data(),updateFilename.length());
		_r->sw->send(outp,true);
	}
}

void Updater::retryIfNeeded()
{
}

void Updater::handleChunk(const void *sha512First16,unsigned long at,const void *chunk,unsigned long len)
{
}

std::string Updater::generateUpdateFilename(unsigned int vMajor,unsigned int vMinor,unsigned int revision)
{
	// Not supported... yet? Get it first cause it might identify as Linux too.
#ifdef __ANDROID__
#define _updSupported 1
	return std::string();
#endif

	// Linux on x86 and possibly in the future ARM
#ifdef __LINUX__
#if defined(__i386) || defined(__i486) || defined(__i586) || defined(__i686) || defined(__amd64) || defined(__x86_64) || defined(i386)
#define _updSupported 1
	char buf[128];
	Utils::snprintf(buf,sizeof(buf),"zt1-%u_%u_%u-linux-%s-update",vMajor,vMinor,revision,(sizeof(void *) == 8) ? "x64" : "x86");
	return std::string(buf);
#endif
/*
#if defined(__arm__) || defined(__arm) || defined(__aarch64__)
#define _updSupported 1
	char buf[128];
	Utils::snprintf(buf,sizeof(buf),"zt1-%u_%u_%u-linux-%s-update",vMajor,vMinor,revision,(sizeof(void *) == 8) ? "arm64" : "arm32");
	return std::string(buf);
#endif
*/
#endif

	// Apple stuff... only Macs so far...
#ifdef __APPLE__
#define _updSupported 1
#if defined(__powerpc) || defined(__powerpc__) || defined(__ppc__) || defined(__ppc64) || defined(__ppc64__) || defined(__powerpc64__)
	return std::string();
#endif
#if defined(TARGET_IPHONE_SIMULATOR) || defined(TARGET_OS_IPHONE)
	return std::string();
#endif
	char buf[128];
	Utils::snprintf(buf,sizeof(buf),"zt1-%u_%u_%u-mac-x86combined-update",vMajor,vMinor,revision);
	return std::string(buf);
#endif

	// ???
#ifndef _updSupported
	return std::string();
#endif
}

bool Updater::parseUpdateFilename(const char *filename,unsigned int &vMajor,unsigned int &vMinor,unsigned int &revision)
{
	std::vector<std::string> byDash(Utils::split(filename,"-","",""));
	if (byDash.size() < 2)
		return false;
	std::vector<std::string> byUnderscore(Utils::split(byDash[1].c_str(),"_","",""));
	if (byUnderscore.size() < 3)
		return false;
	vMajor = Utils::strToUInt(byUnderscore[0].c_str());
	vMinor = Utils::strToUInt(byUnderscore[1].c_str());
	revision = Utils::strToUInt(byUnderscore[2].c_str());
	return true;
}

} // namespace ZeroTier

