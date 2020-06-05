/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CREDENTIAL_HPP
#define ZT_CREDENTIAL_HPP

#include "Constants.hpp"
#include "TriviallyCopyable.hpp"

#include <string>
#include <memory>
#include <stdexcept>

namespace ZeroTier {

class Capability;
class Revocation;
class Tag;
class CertificateOfMembership;
class CertificateOfOwnership;
class RuntimeEnvironment;

/**
 * Base class for credentials
 *
 * Note that all credentials are and must be trivially copyable.
 *
 * All credential verification methods are implemented in Credential.cpp as they share a lot
 * of common code and logic and grouping them makes auditing easier.
 */
class Credential : public TriviallyCopyable
{
public:
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
	VerifyResult _verify(const RuntimeEnvironment *RR,void *tPtr,const CertificateOfMembership &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *RR,void *tPtr,const Revocation &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *RR,void *tPtr,const Tag &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *RR,void *tPtr,const CertificateOfOwnership &credential) const;
	VerifyResult _verify(const RuntimeEnvironment *RR,void *tPtr,const Capability &credential) const;
};

} // namespace ZeroTier

#endif
