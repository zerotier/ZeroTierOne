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

#ifndef _ZT_UPDATER_HPP
#define _ZT_UPDATER_HPP

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <map>
#include <vector>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>

#include "Constants.hpp"
#include "Packet.hpp"
#include "Mutex.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Array.hpp"
#include "Dictionary.hpp"

// Chunk size-- this can be changed, picked to always fit in one packet each.
#define ZT_UPDATER_CHUNK_SIZE 1350

// Sanity check value for constraining max size since right now we buffer
// in RAM.
#define ZT_UPDATER_MAX_SUPPORTED_SIZE (1024 * 1024 * 16)

// Retry timeout in ms.
#define ZT_UPDATER_RETRY_TIMEOUT 30000

// After this long, look for a new set of peers that have the download shared.
#define ZT_UPDATER_REPOLL_TIMEOUT 60000

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Software update downloader and executer
 *
 * FYI: downloads occur via the protocol rather than out of band via http so
 * that ZeroTier One can be run in secure jailed environments where it is the
 * only protocol permitted over the "real" Internet. This is required for a
 * number of potentially popular use cases.
 *
 * The protocol is a simple chunk-pulling "trivial FTP" like thing that should
 * be suitable for core engine software updates. Software updates themselves
 * are platform-specific executables that ZeroTier One then exits and runs.
 *
 * Updaters are cached one-deep and can be replicated peer to peer in addition
 * to coming from supernodes. This makes it just a little bit BitTorrent-like
 * and helps things scale, and is also ready for further protocol
 * decentralization that may occur in the future.
 */
class Updater
{
public:
	Updater(const RuntimeEnvironment *renv);
	~Updater();

	/**
	 * Rescan home path for shareable updates
	 *
	 * This happens automatically on construction.
	 */
	void refreshShared();

	/**
	 * Attempt to find an update if this version is newer than ours
	 *
	 * This is called whenever a peer notifies us of its version. It does nothing
	 * if that version is not newer, otherwise it looks around for an update.
	 *
	 * @param vMajor Major version
	 * @param vMinor Minor version
	 * @param revision Revision
	 */
	void getUpdateIfThisIsNewer(unsigned int vMajor,unsigned int vMinor,unsigned int revision);

	/**
	 * Called periodically from main loop
	 */
	void retryIfNeeded();

	/**
	 * Called when a chunk is received
	 *
	 * @param sha512First16 First 16 bytes of SHA-512 hash
	 * @param at Position of chunk
	 * @param chunk Chunk data
	 * @param len Length of chunk
	 */
	void handleChunk(const void *sha512First16,unsigned long at,const void *chunk,unsigned long len);

	/**
	 * @return Canonical update filename for this platform or empty string if unsupported
	 */
	static std::string generateUpdateFilename(unsigned int vMajor,unsigned int vMinor,unsigned int revision);

	/**
	 * Parse an updater filename and extract version info
	 *
	 * @param filename Filename to parse
	 * @return True if info was extracted and value-result parameters set
	 */
	static bool parseUpdateFilename(const char *filename,unsigned int &vMajor,unsigned int &vMinor,unsigned int &revision);

private:
	struct _Download
	{
		_Download(const void *s512,const std::string &fn,unsigned long len,unsigned int vMajor,unsigned int vMinor,unsigned int rev)
		{
			data.resize(len);
			haveChunks.resize((len / ZT_UPDATER_CHUNK_SIZE) + 1,false);
			filename = fn;
			memcpy(sha512,s512,64);
			lastChunkSize = len % ZT_UPDATER_CHUNK_SIZE;
			versionMajor = vMajor;
			versionMinor = vMinor;
			revision = rev;
		}

		long nextChunk() const
		{
			std::vector<bool>::const_iterator ptr(std::find(haveChunks.begin(),haveChunks.end(),false));
			if (ptr != haveChunks.end())
				return std::distance(haveChunks.begin(),ptr);
			else return -1;
		}

		bool gotChunk(unsigned long at,const void *chunk,unsigned long len)
		{
			unsigned long whichChunk = at / ZT_UPDATER_CHUNK_SIZE;
			if (at != (ZT_UPDATER_CHUNK_SIZE * whichChunk))
				return false; // not at chunk boundary
			if (whichChunk >= haveChunks.size())
				return false; // overflow
			if ((whichChunk == (haveChunks.size() - 1))&&(len != lastChunkSize))
				return false; // last chunk, size wrong
			else if (len != ZT_UPDATER_CHUNK_SIZE)
				return false; // chunk size wrong
			for(unsigned long i=0;i<len;++i)
				data[at + i] = ((const char *)chunk)[i];
			haveChunks[whichChunk] = true;
			return true;
		}

		std::string data;
		std::vector<bool> haveChunks;
		std::vector<Address> peersThatHave;
		std::string filename;
		unsigned char sha512[64];
		Address currentlyReceivingFrom;
		uint64_t lastChunkReceivedAt;
		unsigned long lastChunkSize;
		unsigned int versionMajor,versionMinor,revision;
	};

	struct _Shared
	{
		std::string filename;
		unsigned char sha512[64];
		C25519::Signature sig;
		Address signedBy;
		unsigned long size;
	};

	const RuntimeEnvironment *_r;
	_Download *_download;
	std::map< Array<unsigned char,16>,_Shared > _sharedUpdates;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
