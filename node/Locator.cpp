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

#include "Locator.hpp"
#include "Utils.hpp"

#include <string.h>
#include <stdlib.h>

#define ZT_LOCATOR_SIGNING_BUFFER_SIZE (64 + (18 * ZT_LOCATOR_MAX_PHYSICAL_ENDPOINTS) + (256 * ZT_LOCATOR_MAX_VIRTUAL_ENDPOINTS))

namespace ZeroTier {

void Locator::sign(const Identity &id,const Identity &organization,const int64_t timestamp)
{
	Buffer<ZT_LOCATOR_SIGNING_BUFFER_SIZE> *const sb = new Buffer<ZT_LOCATOR_SIGNING_BUFFER_SIZE>();
	_ts = timestamp;
	_id = id;
	_organization = organization;
	serialize(*sb,true);
	if (id)
		_signatureLength = id.sign(sb->data(),sb->size(),_signature,sizeof(_signature));
	if (organization)
		_orgSignatureLength = organization.sign(sb->data(),sb->size(),_orgSignature,sizeof(_orgSignature));
	delete sb;
}

bool Locator::verify() const
{
	Buffer<ZT_LOCATOR_SIGNING_BUFFER_SIZE> *const sb = new Buffer<ZT_LOCATOR_SIGNING_BUFFER_SIZE>();
	serialize(*sb,true);
	bool ok = _id.verify(sb->data(),sb->size(),_signature,_signatureLength);
	if ((ok)&&(_organization))
		ok &= _organization.verify(sb->data(),sb->size(),_orgSignature,_orgSignatureLength);
	delete sb;
	return ok;
}

void Locator::generateDNSRecords(char *buf,unsigned int buflen)
{
	Buffer<ZT_LOCATOR_SIGNING_BUFFER_SIZE> *const sb = new Buffer<ZT_LOCATOR_SIGNING_BUFFER_SIZE>();
	delete sb;
}

} // namespace ZeroTier
