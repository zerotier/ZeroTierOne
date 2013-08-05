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

#ifndef _ZT_SYSENV_HPP
#define _ZT_SYSENV_HPP

#include <stdint.h>

#include "NonCopyable.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Local system environment monitoring utilities
 */
class SysEnv : NonCopyable
{
public:
	SysEnv(const RuntimeEnvironment *renv);
	~SysEnv();

	/**
	 * @return Fingerprint of currently running network environment
	 */
	uint64_t getNetworkConfigurationFingerprint()
		throw();

private:
	const RuntimeEnvironment *_r;
};

} // namespace ZeroTier

#endif
