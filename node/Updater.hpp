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
#include <list>

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
#define ZT_UPDATER_RETRY_TIMEOUT 15000

// After this long, look for a new peer to download from
#define ZT_UPDATER_PEER_TIMEOUT 65000

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Software update downloader and executer
 *
 * Downloads occur via the ZT1 protocol rather than out of band via http so
 * that ZeroTier One can be run in secure jailed environments where it is the
 * only protocol permitted over the "real" Internet. This is wanted for a
 * number of potentially popular use cases, like private LANs that connect
 * nodes in hostile environments or playing attack/defend on the future CTF
 * network.
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
	/**
	 * Contains information about a shared update available to other peers
	 */
	struct SharedUpdate
	{
		std::string fullPath;
		std::string filename;
		unsigned char sha512[64];
		C25519::Signature sig;
		Address signedBy;
		unsigned long size;
	};

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
	 *
	 * This retries downloads if they're stalled and performs other cleanup.
	 */
	void retryIfNeeded();

	/**
	 * Called when a chunk is received
	 *
	 * If the chunk is a final chunk and we now have an update, this may result
	 * in the commencement of the update process and the shutdown of ZT1.
	 *
	 * @param from Originating peer
	 * @param sha512 Up to 64 bytes of hash to match
	 * @param shalen Length of sha512[]
	 * @param at Position of chunk
	 * @param chunk Chunk data
	 * @param len Length of chunk
	 */
	void handleChunk(const Address &from,const void *sha512,unsigned int shalen,unsigned long at,const void *chunk,unsigned long len);

	/**
	 * Called when a reply to a search for an update is received
	 *
	 * This checks SHA-512 hash signature and version as parsed from filename
	 * before starting the transfer.
	 *
	 * @param from Node that sent reply saying it has the file
	 * @param filename Name of file (can be parsed for version info)
	 * @param sha512 64-byte SHA-512 hash of file's contents
	 * @param filesize Size of file in bytes
	 * @param signedBy Address of signer of hash
	 * @param signature Signature (currently must be Ed25519)
	 * @param siglen Length of signature in bytes
	 */
	void handleAvailable(const Address &from,const char *filename,const void *sha512,unsigned long filesize,const Address &signedBy,const void *signature,unsigned int siglen);

	/**
	 * Get data about a shared update if found
	 *
	 * @param filename File name
	 * @param update Empty structure to be filled with update info
	 * @return True if found (if false, 'update' is unmodified)
	 */
	bool findSharedUpdate(const char *filename,SharedUpdate &update) const;

	/**
	 * Get data about a shared update if found
	 *
	 * @param sha512 Up to 64 bytes of hash to match
	 * @param shalen Length of sha512[]
	 * @param update Empty structure to be filled with update info
	 * @return True if found (if false, 'update' is unmodified)
	 */
	bool findSharedUpdate(const void *sha512,unsigned int shalen,SharedUpdate &update) const;

	/**
	 * Get a chunk of a shared update
	 *
	 * @param sha512 Up to 64 bytes of hash to match
	 * @param shalen Length of sha512[]
	 * @param at Position in file
	 * @param chunk Buffer to store data
	 * @param chunklen Number of bytes to get
	 * @return True if chunk[] was successfully filled, false if not found or other error
	 */
	bool getSharedChunk(const void *sha512,unsigned int shalen,unsigned long at,void *chunk,unsigned long chunklen) const;

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
	void _requestNextChunk();

	struct _Download
	{
		std::string data;
		std::vector<bool> haveChunks;
		std::list<Address> peersThatHave; // excluding current
		std::string filename;
		unsigned char sha512[64];
		Address currentlyReceivingFrom;
		uint64_t lastChunkReceivedAt;
		unsigned long lastChunkSize;
		unsigned int versionMajor,versionMinor,revision;
	};

	const RuntimeEnvironment *_r;
	_Download *_download;
	std::list<SharedUpdate> _sharedUpdates; // usually not more than 1 or 2 of these
	Mutex _lock;
};

} // namespace ZeroTier

#endif
