/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_CREDENTIAL_HPP
#define ZT_CREDENTIAL_HPP

#include <string>
#include <memory>
#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "Constants.hpp"

namespace ZeroTier {

/**
 * Base class for credentials
 */
class Credential
{
public:
	/**
	 * Do not change type code IDs -- these are used in Revocation objects and elsewhere
	 */
	enum Type
	{
		CREDENTIAL_TYPE_NULL = 0,
		CREDENTIAL_TYPE_COM = 1,        // CertificateOfMembership
		CREDENTIAL_TYPE_CAPABILITY = 2,
		CREDENTIAL_TYPE_TAG = 3,
		CREDENTIAL_TYPE_COO = 4,        // CertificateOfOwnership
		CREDENTIAL_TYPE_REVOCATION = 6
	};
};

} // namespace ZeroTier

#endif
