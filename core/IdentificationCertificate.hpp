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

#ifndef ZT_IDENTIFICATIONCERTIFICATE_HPP
#define ZT_IDENTIFICATIONCERTIFICATE_HPP

#include "Constants.hpp"
#include "SHA512.hpp"
#include "C25519.hpp"
#include "ECC384.hpp"
#include "SharedPtr.hpp"
#include "Identity.hpp"
#include "Locator.hpp"
#include "Dictionary.hpp"
#include "Utils.hpp"
#include "Containers.hpp"

namespace ZeroTier {

/**
 * Certificate identifying the real world owner of an identity or network.
 *
 * This is a wrapper around the straight C ZT_IdentificationCertificate and
 * handles allocating memory for objects and disposing of it on GC. If filling
 * out a ZT_IdentificationCertificate structure, identities and other objects
 * should be attached via the addXXX() methods rather than by directly setting
 * the pointers in the C structure.
 *
 * If identities and similar objects are NOT added via the addXXX() methods,
 * this will not take care of de-allocating them when destroyed.
 *
 * The serialNo field is filled in automatically by sign() and decode(), so
 * it can be left undefined when building certificates.
 */
class IdentificationCertificate : public ZT_IdentificationCertificate
{
public:
	ZT_INLINE IdentificationCertificate() noexcept
	{ Utils::zero< sizeof(ZT_IdentificationCertificate) >((ZT_IdentificationCertificate *)this); }

	ZT_INLINE IdentificationCertificate(const ZT_IdentificationCertificate &apiCert)
	{ Utils::copy< sizeof(ZT_IdentificationCertificate) >((ZT_IdentificationCertificate *)this, &apiCert); }

	ZT_INLINE IdentificationCertificate(const IdentificationCertificate &cert)
	{ *this = cert; }

	IdentificationCertificate &operator=(const ZT_IdentificationCertificate &apiCert);

	IdentificationCertificate &operator=(const IdentificationCertificate &cert);

	/**
	 * Add a subject node/identity without a locator
	 *
	 * @param id Identity
	 * @return Pointer to C struct
	 */
	ZT_IdentificationCertificate_Node *addSubjectNode(const Identity &id);

	/**
	 * Add a subject node/identity with a locator
	 *
	 * @param id Identity
	 * @param loc Locator signed by identity (signature is NOT checked here)
	 * @return Pointer to C struct
	 */
	ZT_IdentificationCertificate_Node *addSubjectNode(const Identity &id, const Locator &loc);

	/**
	 * Add a subject network
	 *
	 * @param id Network ID
	 * @param controller Network controller's full fingerprint
	 * @return Pointer to C struct
	 */
	ZT_IdentificationCertificate_Network *addSubjectNetwork(const uint64_t id, const ZT_Fingerprint &controller);

	/**
	 * Marshal this certificate in binary form
	 *
	 * The internal encoding used here is Dictionary to permit easy
	 * extensibility.
	 *
	 * @param omitSignature If true omit the signature field (for signing and verification, default is false)
	 * @return Marshaled certificate
	 */
	Vector< uint8_t > encode(bool omitSignature = false) const;

	/**
	 * Decode this certificate from marshaled bytes.
	 *
	 * @param data Marshalled certificate
	 * @return True if input is valid and was unmarshalled (signature is NOT checked)
	 */
	bool decode(const Vector< uint8_t > &data);

	/**
	 * Sign this certificate (and also fill in serialNo).
	 *
	 * @param issuer Issuer identity (must have secret key)
	 * @return True on success
	 */
	bool sign(const Identity &issuer);

	/**
	 * Verify certificate signature against the issuer contained therein
	 *
	 * @return True if certificate is signed and signature is valid
	 */
	bool verify() const;

private:
	// These hold any identity or locator objects that are owned by and should
	// be deleted with this certificate. Lists are used so the pointers never
	// change.
	List< Identity > m_identities;
	List< Locator > m_locators;

	// These are stored in a vector because the memory needs to be contiguous.
	Vector< ZT_IdentificationCertificate_Node > m_nodes;
	Vector< ZT_IdentificationCertificate_Network > m_networks;
};

} // namespace ZeroTier

#endif
