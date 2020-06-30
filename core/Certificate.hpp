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

#ifndef ZT_CERTIFICATE_HPP
#define ZT_CERTIFICATE_HPP

#include "Constants.hpp"
#include "SHA512.hpp"
#include "C25519.hpp"
#include "ECC384.hpp"
#include "SharedPtr.hpp"
#include "Identity.hpp"
#include "Locator.hpp"
#include "Dictionary.hpp"
#include "Utils.hpp"
#include "Blob.hpp"
#include "Containers.hpp"

namespace ZeroTier {

/**
 * Certificate describing and grouping a set of objects.
 *
 * This is a wrapper around the straight C ZT_IdentificationCertificate and
 * handles allocating memory for objects added via addXXX() and disposing of
 * them on delete. If pointers in the underlying C struct are set manually,
 * their memory is not freed on delete. Use the addXXX() methods to fill
 * out this structure in C++ code.
 *
 * The serialNo field is filled in automatically by sign() and decode(), so
 * it can be left undefined when building certificates. It contains a SHA384
 * hash of the certificate marshalled without the signature field.
 *
 * The hashCode() method and comparison operators compare the serial number
 * field, so these will not work correctly before sign() or decode() is
 * called.
 */
class Certificate : public ZT_Certificate
{
	friend class SharedPtr< Certificate >;
	friend class SharedPtr< const Certificate >;

public:
	ZT_INLINE Certificate() noexcept
	{ this->clear(); }

	ZT_INLINE Certificate(const ZT_Certificate &apiCert)
	{ *this = apiCert; }

	ZT_INLINE Certificate(const Certificate &cert)
	{ *this = cert; }

	/**
	 * Zero all fields and release all extra memory
	 */
	void clear();

	Certificate &operator=(const ZT_Certificate &apiCert);
	Certificate &operator=(const Certificate &cert);

	/**
	 * Add a subject node/identity without a locator
	 *
	 * @param id Identity
	 * @return Pointer to C struct
	 */
	ZT_Certificate_Identity *addSubjectIdentity(const Identity &id);

	/**
	 * Add a subject node/identity with a locator
	 *
	 * @param id Identity
	 * @param loc Locator signed by identity (signature is NOT checked here)
	 * @return Pointer to C struct
	 */
	ZT_Certificate_Identity *addSubjectIdentity(const Identity &id, const Locator &loc);

	/**
	 * Add a subject network
	 *
	 * @param id Network ID
	 * @param controller Network controller's full fingerprint
	 * @return Pointer to C struct
	 */
	ZT_Certificate_Network *addSubjectNetwork(const uint64_t id, const ZT_Fingerprint &controller);

	/**
	 * Add a subject certificate (by its serial number)
	 *
	 * @param serialNo 384-bit serial number
	 */
	void addSubjectCertificate(const uint8_t serialNo[ZT_SHA384_DIGEST_SIZE]);

	/**
	 * Add an update URL to the updateUrls list
	 *
	 * @param url Update URL
	 */
	void addSubjectUpdateUrl(const char *url);

	/**
	 * Set the extended attributes of this certificate
	 *
	 * @param x Extended attributes (set by issuer)
	 */
	ZT_INLINE void setExtendedAttributes(const Dictionary &x)
	{
		m_extendedAttributes.clear();
		x.encode(m_extendedAttributes);
		this->extendedAttributes = m_extendedAttributes.data();
		this->extendedAttributesSize = (unsigned int)m_extendedAttributes.size();
	}

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
	 * Verify self-contained signatures and validity of certificate structure
	 *
	 * This doesn't check the entire certificate chain, just the validity of
	 * the certificate's internal signature and fields.
	 *
	 * @return OK (0) or error code indicating why certificate failed verification.
	 */
	ZT_CertificateError verify() const;

	/**
	 * Set the unique ID of this certificate's subject
	 *
	 * This must be done after all other fields in the subject are set.
	 *
	 * @param uniqueId Unique ID
	 * @param uniqueIdPrivate Private key associated with unique ID to prove ownership of it
	 * @return True if successful
	 */
	bool setSubjectUniqueId(const uint8_t uniqueId[ZT_CERTIFICATE_UNIQUE_ID_SIZE_TYPE_NIST_P_384], const uint8_t uniqueIdPrivate[ZT_CERTIFICATE_UNIQUE_ID_PRIVATE_KEY_SIZE_TYPE_NIST_P_384]);

	/**
	 * Create a subject unique ID and corresponding private key required for use
	 *
	 * @param uniqueId Buffer to receive unique ID
	 * @param uniqueIdPrivate Buffer to receive private key
	 */
	static ZT_INLINE void createSubjectUniqueId(uint8_t uniqueId[ZT_CERTIFICATE_UNIQUE_ID_SIZE_TYPE_NIST_P_384], uint8_t uniqueIdPrivate[ZT_CERTIFICATE_UNIQUE_ID_PRIVATE_KEY_SIZE_TYPE_NIST_P_384])
	{
		uniqueId[0] = ZT_CERTIFICATE_UNIQUE_ID_PUBLIC_KEY_TYPE_NIST_P_384;
		ECC384GenerateKey(uniqueId + 1, uniqueIdPrivate);
	}

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)Utils::loadMachineEndian< uint32_t >(this->serialNo); }

	ZT_INLINE bool operator==(const ZT_Certificate &c) const noexcept
	{ return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) == 0; }
	ZT_INLINE bool operator!=(const ZT_Certificate &c) const noexcept
	{ return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) != 0; }
	ZT_INLINE bool operator<(const ZT_Certificate &c) const noexcept
	{ return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) < 0; }
	ZT_INLINE bool operator<=(const ZT_Certificate &c) const noexcept
	{ return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) <= 0; }
	ZT_INLINE bool operator>(const ZT_Certificate &c) const noexcept
	{ return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) > 0; }
	ZT_INLINE bool operator>=(const ZT_Certificate &c) const noexcept
	{ return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) >= 0; }

private:
	static void m_encodeSubject(const ZT_Certificate_Subject &s, Dictionary &d, bool omitUniqueIdProofSignature);

	// These hold any identity or locator objects that are owned by and should
	// be deleted with this certificate. Lists are used so the pointers never
	// change.
	List< Identity > m_identities;
	List< Locator > m_locators;
	List< String > m_strings;
	List< SHA384Hash > m_serials;

	// These are stored in a vector because the memory needs to be contiguous.
	Vector< ZT_Certificate_Identity > m_subjectIdentities;
	Vector< ZT_Certificate_Network > m_subjectNetworks;
	Vector< const uint8_t * > m_subjectCertificates;
	Vector< const char * > m_updateUrls;
	Vector< uint8_t > m_extendedAttributes;
	Vector< uint8_t > m_subjectUniqueId;
	Vector< uint8_t > m_subjectUniqueIdProofSignature;
	Vector< uint8_t > m_signature;

	std::atomic<int> __refCount;
};

} // namespace ZeroTier

#endif
