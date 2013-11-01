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
}

void Updater::retryIfNeeded()
{
}

} // namespace ZeroTier

