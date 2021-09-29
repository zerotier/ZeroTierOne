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

#ifndef ZT_SOFTWAREUPDATER_HPP
#define ZT_SOFTWAREUPDATER_HPP

#include <stdint.h>
#include <stdio.h>

#include <vector>
#include <map>
#include <string>
#include <array>

#include "../include/ZeroTierOne.h"

#include "../node/Identity.hpp"
#include "../node/Packet.hpp"

#include "../ext/json/json.hpp"

/**
 * VERB_USER_MESSAGE type ID for software update messages
 */
#define ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE 100

/**
 * ZeroTier address of node that provides software updates
 */
#define ZT_SOFTWARE_UPDATE_SERVICE 0xb1d366e81fULL

/**
 * ZeroTier identity that must be used to sign software updates
 *
 * df24360f3e - update-signing-key-0010 generated Fri Jan 13th, 2017 at 4:05pm PST
 */
#define ZT_SOFTWARE_UPDATE_SIGNING_AUTHORITY "df24360f3e:0:06072642959c8dfb68312904d74d90197c8a7692697caa1b3fd769eca714f4370fab462fcee6ebcb5fffb63bc5af81f28a2514b2cd68daabb42f7352c06f21db"

/**
 * Chunk size for in-band downloads (can be changed, designed to always fit in one UDP packet easily)
 */
#define ZT_SOFTWARE_UPDATE_CHUNK_SIZE (ZT_PROTO_MAX_PACKET_LENGTH - 128)

/**
 * Sanity limit for the size of an update binary image
 */
#define ZT_SOFTWARE_UPDATE_MAX_SIZE (1024 * 1024 * 256)

/**
 * How often (ms) do we check?
 */
#define ZT_SOFTWARE_UPDATE_CHECK_PERIOD (60 * 10 * 1000)

/**
 * Default update channel
 */
#define ZT_SOFTWARE_UPDATE_DEFAULT_CHANNEL "release"

/**
 * Filename for latest update's binary image
 */
#define ZT_SOFTWARE_UPDATE_BIN_FILENAME "latest-update.exe"

#define ZT_SOFTWARE_UPDATE_JSON_VERSION_MAJOR "vMajor"
#define ZT_SOFTWARE_UPDATE_JSON_VERSION_MINOR "vMinor"
#define ZT_SOFTWARE_UPDATE_JSON_VERSION_REVISION "vRev"
#define ZT_SOFTWARE_UPDATE_JSON_VERSION_BUILD "vBuild"
#define ZT_SOFTWARE_UPDATE_JSON_PLATFORM "platform"
#define ZT_SOFTWARE_UPDATE_JSON_ARCHITECTURE "arch"
#define ZT_SOFTWARE_UPDATE_JSON_VENDOR "vendor"
#define ZT_SOFTWARE_UPDATE_JSON_CHANNEL "channel"
#define ZT_SOFTWARE_UPDATE_JSON_EXPECT_SIGNED_BY "expectedSigner"
#define ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNED_BY "signer"
#define ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIGNATURE "signature"
#define ZT_SOFTWARE_UPDATE_JSON_UPDATE_HASH "hash"
#define ZT_SOFTWARE_UPDATE_JSON_UPDATE_SIZE "size"
#define ZT_SOFTWARE_UPDATE_JSON_UPDATE_EXEC_ARGS "execArgs"
#define ZT_SOFTWARE_UPDATE_JSON_UPDATE_URL "url"

namespace ZeroTier {

class Node;

/**
 * This class handles retrieving and executing updates, or serving them
 */
class SoftwareUpdater
{
public:
	/**
	 * Each message begins with an 8-bit message verb
	 */
	enum MessageVerb
	{
		/**
		 * Payload: JSON containing current system platform, version, etc.
		 */
		VERB_GET_LATEST = 1,

		/**
		 * Payload: JSON describing latest update for this target. (No response is sent if there is none.)
		 */
		VERB_LATEST = 2,

		/**
		 * Payload:
		 *   <[16] first 128 bits of hash of data object>
		 *   <[4] 32-bit index of chunk to get>
		 */
		VERB_GET_DATA = 3,

		/**
		 * Payload:
		 *   <[16] first 128 bits of hash of data object>
		 *   <[4] 32-bit index of chunk>
		 *   <[...] chunk data>
		 */
		VERB_DATA = 4
	};

	SoftwareUpdater(Node &node,const std::string &homePath);
	~SoftwareUpdater();

	/**
	 * Set whether or not we will distribute updates
	 *
	 * @param distribute If true, scan update-dist.d now and distribute updates found there -- if false, clear and stop distributing
	 */
	void setUpdateDistribution(bool distribute);

	/**
	 * Handle a software update user message
	 *
	 * @param origin ZeroTier address of message origin
	 * @param data Message payload
	 * @param len Length of message
	 */
	void handleSoftwareUpdateUserMessage(uint64_t origin,const void *data,unsigned int len);

	/**
	 * Check for updates and do other update-related housekeeping
	 *
	 * It should be called about every 10 seconds.
	 *
	 * @return True if we've downloaded and verified an update
	 */
	bool check(const int64_t now);

	/**
	 * @return Meta-data for downloaded update or NULL if none
	 */
	inline const nlohmann::json &pending() const { return _latestMeta; }

	/**
	 * Apply any ready update now
	 *
	 * Depending on the platform this function may never return and may forcibly
	 * exit the process. It does nothing if no update is ready.
	 */
	void apply();

	/**
	 * Set software update channel
	 *
	 * @param channel 'release', 'beta', etc.
	 */
	inline void setChannel(const std::string &channel) { _channel = channel; }

private:
	Node &_node;
	uint64_t _lastCheckTime;
	std::string _homePath;
	std::string _channel;
	FILE *_distLog;

	// Offered software updates if we are an update host (we have update-dist.d and update hosting is enabled)
	struct _D
	{
		nlohmann::json meta;
		std::string bin;
	};
	std::map< std::array<uint8_t,16>,_D > _dist; // key is first 16 bytes of hash

	nlohmann::json _latestMeta;
	bool _latestValid;

	std::string _download;
	std::array<uint8_t,16> _downloadHashPrefix;
	unsigned long _downloadLength;
};

} // namespace ZeroTier

#endif
