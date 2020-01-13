/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CREDENTIAL_HPP
#define ZT_CREDENTIAL_HPP

#include <string>
#include <memory>
#include <stdexcept>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "Constants.hpp"

namespace ZeroTier {

class Capability;
class Revocation;
class Tag;
class CertificateOfMembership;
class CertificateOfOwnership;
class RuntimeEnvironment;

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

	/**
	 * Result of verify() operations
	 */
	enum VerifyResult
	{
		VERIFY_OK = 0,
		VERIFY_BAD_SIGNATURE = 1,
		VERIFY_NEED_IDENTITY = 2
	};

protected:
	VerifyResult _verify(const RuntimeEnvironment *const RR,void *tPtr,const CertificateOfMembership &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *const RR,void *tPtr,const Revocation &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *const RR,void *tPtr,const Tag &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *const RR,void *tPtr,const CertificateOfOwnership &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *const RR,void *tPtr,const Capability &credential) const;
};

} // namespace ZeroTier

#endif
