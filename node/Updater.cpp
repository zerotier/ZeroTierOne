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
#include "Switch.hpp"
#include "SHA512.hpp"

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
		if ((u->first.length() >= 4)&&(!strcasecmp(u->first.substr(u->first.length() - 4).c_str(),".sig")))
			continue; // skip .sig companion files

		std::string fullPath(updatesPath + ZT_PATH_SEPARATOR_S + u->first);
		std::string sigPath(fullPath + ".sig");

		std::string buf;
		if (Utils::readFile(sigPath.c_str(),buf)) {
			Dictionary sig(buf);

			SharedUpdate shared;
			shared.fullPath = fullPath;
			shared.filename = u->first;

			std::string sha512(Utils::unhex(sig.get("sha512",std::string())));
			std::string signature(Utils::unhex(sig.get("sha512_ed25519",std::string())));
			Address signedBy(sig.get("signedBy",std::string()));
			if ((sha512.length() < sizeof(shared.sha512))||(signature.length() < shared.sig.size())||(!signedBy)) {
				TRACE("skipped shareable update due to missing fields in companion .sig: %s",fullPath.c_str());
				continue;
			}
			memcpy(shared.sha512,sha512.data(),sizeof(shared.sha512));
			memcpy(shared.sig.data,signature.data(),shared.sig.size());
			shared.signedBy = signedBy;

			int64_t fs = Utils::getFileSize(fullPath.c_str());
			if (fs <= 0) {
				TRACE("skipped shareable update due to unreadable, invalid, or 0-byte file: %s",fullPath.c_str());
				continue;
			}
			shared.size = (unsigned long)fs;

			LOG("sharing software update %s to other peers",shared.filename.c_str());
			_sharedUpdates.push_back(shared);
		} else {
			TRACE("skipped shareable update due to missing companion .sig: %s",fullPath.c_str());
			continue;
		}
	}
}

void Updater::getUpdateIfThisIsNewer(unsigned int vMajor,unsigned int vMinor,unsigned int revision)
{
	if (!compareVersions(vMajor,vMinor,revision,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION))
		return;

	std::string updateFilename(generateUpdateFilename(vMajor,vMinor,revision));
	if (!updateFilename.length()) {
		TRACE("an update to %u.%u.%u is available, but this platform or build doesn't support auto-update",vMajor,vMinor,revision);
		return;
	}

	std::vector< SharedPtr<Peer> > peers;
	_r->topology->eachPeer(Topology::CollectPeersWithActiveDirectPath(peers,Utils::now()));

	TRACE("new update available to %u.%u.%u, looking for %s from %u peers",vMajor,vMinor,revision,updateFilename.c_str(),(unsigned int)peers.size());

	for(std::vector< SharedPtr<Peer> >::iterator p(peers.begin());p!=peers.end();++p) {
		Packet outp((*p)->address(),_r->identity.address(),Packet::VERB_FILE_INFO_REQUEST);
		outp.append((unsigned char)0);
		outp.append((uint16_t)updateFilename.length());
		outp.append(updateFilename.data(),updateFilename.length());
		_r->sw->send(outp,true);
	}
}

void Updater::retryIfNeeded()
{
	Mutex::Lock _l(_lock);

	if (_download) {
		uint64_t elapsed = Utils::now() - _download->lastChunkReceivedAt;
		if ((elapsed >= ZT_UPDATER_PEER_TIMEOUT)||(!_download->currentlyReceivingFrom)) {
			if (_download->peersThatHave.empty()) {
				// Search for more sources if we have no more possibilities queued
				TRACE("all sources for %s timed out, searching for more...",_download->filename.c_str());
				_download->currentlyReceivingFrom.zero();

				std::vector< SharedPtr<Peer> > peers;
				_r->topology->eachPeer(Topology::CollectPeersWithActiveDirectPath(peers,Utils::now()));

				for(std::vector< SharedPtr<Peer> >::iterator p(peers.begin());p!=peers.end();++p) {
					Packet outp((*p)->address(),_r->identity.address(),Packet::VERB_FILE_INFO_REQUEST);
					outp.append((unsigned char)0);
					outp.append((uint16_t)_download->filename.length());
					outp.append(_download->filename.data(),_download->filename.length());
					_r->sw->send(outp,true);
				}
			} else {
				// If that peer isn't answering, try the next queued source
				_download->currentlyReceivingFrom = _download->peersThatHave.front();
				_download->peersThatHave.pop_front();
				_requestNextChunk();
			}
		} else if (elapsed >= ZT_UPDATER_RETRY_TIMEOUT) {
			// Re-request next chunk we don't have from current source
			_requestNextChunk();
		}
	}
}

void Updater::handleChunk(const Address &from,const void *sha512,unsigned int shalen,unsigned long at,const void *chunk,unsigned long len)
{
	Mutex::Lock _l(_lock);

	if (!_download) {
		TRACE("got chunk from %s while no download is in progress, ignored",from.toString().c_str());
		return;
	}

	if (memcmp(_download->sha512,sha512,(shalen > 64) ? 64 : shalen)) {
		TRACE("got chunk from %s for wrong download (SHA mismatch), ignored",from.toString().c_str());
		return;
	}

	unsigned long whichChunk = at / ZT_UPDATER_CHUNK_SIZE;

	if (
	     (at != (ZT_UPDATER_CHUNK_SIZE * whichChunk))||
	     (whichChunk >= _download->haveChunks.size())||
	     ((whichChunk == (_download->haveChunks.size() - 1))&&(len != _download->lastChunkSize))||
	     (len != ZT_UPDATER_CHUNK_SIZE)
	   ) {
		TRACE("got chunk from %s at invalid position or invalid size, ignored",from.toString().c_str());
		return;
	}

	for(unsigned long i=0;i<len;++i)
		_download->data[at + i] = ((const char *)chunk)[i];

	_download->haveChunks[whichChunk] = true;
	_download->lastChunkReceivedAt = Utils::now();

	_requestNextChunk();
}

void Updater::handlePeerHasFile(const Address &from,const char *filename,const void *sha512,unsigned long filesize,const Address &signedBy,const void *signature,unsigned int siglen)
{
	unsigned int vMajor = 0,vMinor = 0,revision = 0;
	if (!parseUpdateFilename(filename,vMajor,vMinor,revision)) {
		TRACE("rejected offer of %s from %s: could not extract version information from filename",filename,from.toString().c_str());
		return;
	}

	if (filesize > ZT_UPDATER_MAX_SUPPORTED_SIZE) {
		TRACE("rejected offer of %s from %s: file too large (%u > %u)",filename,from.toString().c_str(),(unsigned int)filesize,(unsigned int)ZT_UPDATER_MAX_SUPPORTED_SIZE);
		return;
	}

	if (!compareVersions(vMajor,vMinor,revision,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION)) {
		TRACE("rejected offer of %s from %s: version older than mine",filename,from.toString().c_str());
		return;
	}

	Mutex::Lock _l(_lock);

	if (_download) {
		if ((_download->filename == filename)&&(_download->data.length() == filesize)&&(!memcmp(sha512,_download->sha512,64))) {
			// Learn another source for the current download if this is the
			// same file.
			LOG("learned new source for %s: %s",filename,from.toString().c_str());
			if (!_download->currentlyReceivingFrom) {
				_download->currentlyReceivingFrom = from;
				_requestNextChunk();
			} else _download->peersThatHave.push_back(from);
			return;
		} else {
			// If there's a download, compare versions... only proceed if this
			// file being offered is newer.
			if (!compareVersions(vMajor,vMinor,revision,_download->versionMajor,_download->versionMinor,_download->revision)) {
				TRACE("rejected offer of %s from %s: already downloading newer version %s",filename,from.toString().c_str(),_download->filename.c_str());
				return;
			}
		}
	}

	// If we have no download OR if this was a different file, check its
	// validity via signature and then see if it's newer. If so start a new
	// download for it.
	{
		std::string nameAndSha(filename);
		nameAndSha.append((const char *)sha512,64);
		std::map< Address,Identity >::const_iterator uauth(ZT_DEFAULTS.updateAuthorities.find(signedBy));
		if (uauth == ZT_DEFAULTS.updateAuthorities.end()) {
			LOG("rejected offer of %s from %s: failed authentication: unknown signer %s",filename,from.toString().c_str(),signedBy.toString().c_str());
			return;
		}
		if (!uauth->second.verify(nameAndSha.data(),nameAndSha.length(),signature,siglen)) {
			LOG("rejected offer of %s from %s: failed authentication: signature from %s invalid",filename,from.toString().c_str(),signedBy.toString().c_str());
			return;
		}
	}

	// Replace existing download if any.
	delete _download;
	_download = (_Download *)0;

	// Create and initiate new download
	_download = new _Download;
	try {
		LOG("beginning download of software update %s from %s (%u bytes, signed by authorized identity %s)",filename,from.toString().c_str(),(unsigned int)filesize,signedBy.toString().c_str());

		_download->data.assign(filesize,(char)0);
		_download->haveChunks.resize((filesize / ZT_UPDATER_CHUNK_SIZE) + 1,false);
		_download->filename = filename;
		memcpy(_download->sha512,sha512,64);
		_download->currentlyReceivingFrom = from;
		_download->lastChunkReceivedAt = 0;
		_download->lastChunkSize = filesize % ZT_UPDATER_CHUNK_SIZE;
		_download->versionMajor = vMajor;
		_download->versionMinor = vMinor;
		_download->revision = revision;

		_requestNextChunk();
	} catch (std::exception &exc) {
		delete _download;
		_download = (_Download *)0;
		LOG("unable to begin download of %s from %s: %s",filename,from.toString().c_str(),exc.what());
	} catch ( ... ) {
		delete _download;
		_download = (_Download *)0;
		LOG("unable to begin download of %s from %s: unknown exception",filename,from.toString().c_str());
	}
}

bool Updater::findSharedUpdate(const char *filename,SharedUpdate &update) const
{
	Mutex::Lock _l(_lock);
	for(std::list<SharedUpdate>::const_iterator u(_sharedUpdates.begin());u!=_sharedUpdates.end();++u) {
		if (u->filename == filename) {
			update = *u;
			return true;
		}
	}
	return false;
}

bool Updater::findSharedUpdate(const void *sha512,unsigned int shalen,SharedUpdate &update) const
{
	if (!shalen)
		return false;
	Mutex::Lock _l(_lock);
	for(std::list<SharedUpdate>::const_iterator u(_sharedUpdates.begin());u!=_sharedUpdates.end();++u) {
		if (!memcmp(u->sha512,sha512,(shalen > 64) ? 64 : shalen)) {
			update = *u;
			return true;
		}
	}
	return false;
}

bool Updater::getSharedChunk(const void *sha512,unsigned int shalen,unsigned long at,void *chunk,unsigned long chunklen) const
{
	if (!chunklen)
		return true;
	if (!shalen)
		return false;
	Mutex::Lock _l(_lock);
	for(std::list<SharedUpdate>::const_iterator u(_sharedUpdates.begin());u!=_sharedUpdates.end();++u) {
		if (!memcmp(u->sha512,sha512,(shalen > 64) ? 64 : shalen)) {
			FILE *f = fopen(u->fullPath.c_str(),"rb");
			if (!f)
				return false;
			if (!fseek(f,(long)at,SEEK_SET)) {
				fclose(f);
				return false;
			}
			if (fread(chunk,chunklen,1,f) != 1) {
				fclose(f);
				return false;
			}
			fclose(f);
			return true;
		}
	}
	return false;
}

std::string Updater::generateUpdateFilename(unsigned int vMajor,unsigned int vMinor,unsigned int revision)
{
	// Defining ZT_OFFICIAL_BUILD enables this cascade of macros, which will
	// make your build auto-update itself if it's for an officially supported
	// architecture. The signing identity for auto-updates is in Defaults.
#ifdef ZT_OFFICIAL_BUILD

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

#else
	return std::string();
#endif // ZT_OFFICIAL_BUILD
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

void Updater::_requestNextChunk()
{
	// assumes _lock is locked

	if (!_download)
		return;

	unsigned long whichChunk = 0;
	std::vector<bool>::iterator ptr(std::find(_download->haveChunks.begin(),_download->haveChunks.end(),false));
	if (ptr == _download->haveChunks.end()) {
		unsigned char digest[64];
		SHA512::hash(digest,_download->data.data(),_download->data.length());
		if (memcmp(digest,_download->sha512,64)) {
			LOG("retrying download of %s -- SHA-512 mismatch, file corrupt!",_download->filename.c_str());
			std::fill(_download->haveChunks.begin(),_download->haveChunks.end(),false);
			whichChunk = 0;
		} else {
			LOG("successfully downloaded and authenticated %s, launching update...",_download->filename.c_str());
			delete _download;
			_download = (_Download *)0;
			return;
		}
	} else {
		whichChunk = std::distance(_download->haveChunks.begin(),ptr);
	}

	TRACE("requesting chunk %u/%u of %s from %s",(unsigned int)whichChunk,(unsigned int)_download->haveChunks.size(),_download->filename.c_str()_download->currentlyReceivingFrom.toString().c_str());

	Packet outp(_download->currentlyReceivingFrom,_r->identity.address(),Packet::VERB_FILE_BLOCK_REQUEST);
	outp.append(_download->sha512,16);
	outp.append((uint32_t)(whichChunk * ZT_UPDATER_CHUNK_SIZE));
	if (whichChunk == (_download->haveChunks.size() - 1))
		outp.append((uint16_t)_download->lastChunkSize);
	else outp.append((uint16_t)ZT_UPDATER_CHUNK_SIZE);
	_r->sw->send(outp,true);
}

} // namespace ZeroTier

