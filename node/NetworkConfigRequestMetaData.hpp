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

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF
#include <string>
#include "Dictionary.hpp"
#endif

namespace ZeroTier {

class NetworkConfigRequestMetaData
{
public:
	NetworkConfigRequestMetaData() :
		_vendor(0),
		_majorVersion(0),
		_minorVersion(0),
		_revision(0),
		_buildNo(0),
		_flags(0)
	{
	}

protected:
	unsigned int _vendor;
	unsigned int _majorVersion;
	unsigned int _minorVersion;
	unsigned int _revision;
	unsigned int _buildNo;
	unsigned int _flags;
	char _passcode[ZT_MAX_NETWORK_SHORT_NAME_LENGTH + 1];
};

} // namespace ZeroTier

#endif
