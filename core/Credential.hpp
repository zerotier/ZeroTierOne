/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CREDENTIAL_HPP
#define ZT_CREDENTIAL_HPP

#include "CallContext.hpp"
#include "Constants.hpp"
#include "TriviallyCopyable.hpp"

namespace ZeroTier {

class CapabilityCredential;
class RevocationCredential;
class TagCredential;
class MembershipCredential;
class OwnershipCredential;
class Context;

/**
 * Base class for credentials
 *
 * Note that all credentials are and must be trivially copyable.
 *
 * All credential verification methods are implemented in Credential.cpp as they share a lot
 * of common code and logic and grouping them makes auditing easier.
 */
class Credential : public TriviallyCopyable {
  public:
    /**
     * Result of verify() operations
     */
    enum VerifyResult { VERIFY_OK = 0, VERIFY_BAD_SIGNATURE = 1, VERIFY_NEED_IDENTITY = 2 };

  protected:
    static VerifyResult s_verify(const Context &ctx, const CallContext &cc, const MembershipCredential &credential);
    static VerifyResult s_verify(const Context &ctx, const CallContext &cc, const RevocationCredential &credential);
    static VerifyResult s_verify(const Context &ctx, const CallContext &cc, const TagCredential &credential);
    static VerifyResult s_verify(const Context &ctx, const CallContext &cc, const OwnershipCredential &credential);
    static VerifyResult s_verify(const Context &ctx, const CallContext &cc, const CapabilityCredential &credential);
};

}   // namespace ZeroTier

#endif
