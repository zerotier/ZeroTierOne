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

#ifndef ZT_NETWORKCONFIGREQUESTMETADATA_HPP
#define ZT_NETWORKCONFIGREQUESTMETADATA_HPP

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Constants.hpp"
#include "NetworkConfig.hpp"
#include "Buffer.hpp"
#include "Packet.hpp"

#include "../version.h"

/**
 * Maximum length of the auth field (including terminating NULL, since it's a C-style string)
 *
 * Actual max length not including NULL is this minus one.
 */
#define ZT_NETWORK_CONFIG_REQUEST_METADATA_MAX_AUTH_LENGTH 2048

namespace ZeroTier {

/**
 * Network configuration request meta data
 */
class NetworkConfigRequestMetaData
{
public:
	/**
	 * Construct an empty meta-data object with zero/null values
	 */
	NetworkConfigRequestMetaData()
	{
		memset(this,0,sizeof(NetworkConfigRequestMetaData));
	}

	/**
	 * Initialize with defaults from this node's config and version
	 */
	inline void initWithDefaults()
	{
		memset(this,0,sizeof(NetworkConfigRequestMetaData));
		vendor = ZT_VENDOR_ZEROTIER;
		platform = ZT_PLATFORM_UNSPECIFIED;
		architecture = ZT_ARCHITECTURE_UNSPECIFIED;
		majorVersion = ZEROTIER_ONE_VERSION_MAJOR;
		minorVersion = ZEROTIER_ONE_VERSION_MINOR;
		revision = ZEROTIER_ONE_VERSION_REVISION;
		protocolVersion = ZT_PROTO_VERSION;
	}

	/**
	 * Zero/null everything
	 */
	inline void clear()
	{
		memset(this,0,sizeof(NetworkConfigRequestMetaData));
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		/* Unlike network config we always send the old fields. Newer network
		 * controllers will detect the presence of the new serialized data by
		 * detecting extra data after the terminating NULL. But always sending
		 * these maintains backward compatibility with old controllers. This
		 * appends a terminating NULL which seperates the old legacy meta-data
		 * from the new packed binary format that we send after. */
		b.appendCString("majv=" ZEROTIER_ONE_VERSION_MAJOR_S_HEX "\nminv=" ZEROTIER_ONE_VERSION_MINOR_S_HEX "\nrevv=" ZEROTIER_ONE_VERSION_REVISION_S_HEX "\n");

		b.append((uint16_t)1); // serialization version

		b.append((uint64_t)buildId);
		b.append((uint64_t)flags);
		b.append((uint16_t)vendor);
		b.append((uint16_t)platform);
		b.append((uint16_t)architecture);
		b.append((uint16_t)majorVersion);
		b.append((uint16_t)minorVersion);
		b.append((uint16_t)revision);
		b.append((uint16_t)protocolVersion);

		const unsigned int tl = strlen(auth);
		b.append((uint16_t)tl);
		b.append((const void *)auth,tl);

		b.append((uint16_t)0); // extended bytes, currently 0 since unused
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		memset(this,0,sizeof(NetworkConfigRequestMetaData));

		unsigned int p = startAt;

		// Seek past old style meta-data
		while (b[p]) ++p;
		++p;

		if (b.template at<uint16_t>(p) != 1)
			throw std::invalid_argument("unrecognized version");
		p += 2;

		buildId = b.template at<uint64_t>(p); p += 8;
		flags = b.template at<uint64_t>(p); p += 8;
		vendor = (ZT_Vendor)b.template at<uint16_t>(p); p += 2;
		platform = (ZT_Platform)b.template at<uint16_t>(p); p += 2;
		architecture = (ZT_Architecture)b.template at<uint16_t>(p); p += 2;
		majorVersion = b.template at<uint16_t>(p); p += 2;
		minorVersion = b.template at<uint16_t>(p); p += 2;
		revision = b.template at<uint16_t>(p); p += 2;
		protocolVersion = b.template at<uint16_t>(p); p += 2;

		const unsigned int tl = b.template at<uint16_t>(p); p += 2;
		memcpy(auth,b.field(p,tl),std::max(tl,(unsigned int)(ZT_NETWORK_CONFIG_REQUEST_METADATA_MAX_AUTH_LENGTH - 1)));
		p += tl;

		p += b.template at<uint16_t>(p) + 2;

		return (p - startAt);
	}

	/**
	 * Authentication data (e.g. bearer=<token>) as a C-style string (always null terminated)
	 */
	char auth[ZT_NETWORK_CONFIG_REQUEST_METADATA_MAX_AUTH_LENGTH];

	/**
	 * Build ID (currently unused, must be 0)
	 */
	uint64_t buildId;

	/**
	 * Flags (currently unused, must be 0)
	 */
	uint64_t flags;

	/**
	 * ZeroTier vendor or 0 for unspecified
	 */
	ZT_Vendor vendor;

	/**
	 * ZeroTier platform or 0 for unspecified
	 */
	ZT_Platform platform;

	/**
	 * ZeroTier architecture or 0 for unspecified
	 */
	ZT_Architecture architecture;

	/**
	 * ZeroTier software major version
	 */
	unsigned int majorVersion;

	/**
	 * ZeroTier software minor version
	 */
	unsigned int minorVersion;

	/**
	 * ZeroTier software revision
	 */
	unsigned int revision;

	/**
	 * ZeroTier protocol version
	 */
	unsigned int protocolVersion;
};

} // namespace ZeroTier

#endif
