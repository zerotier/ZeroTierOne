/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
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
	Certificate() noexcept;
	Certificate(const ZT_Certificate &apiCert);
	Certificate(const Certificate &cert);
	~Certificate();

	Certificate &operator=(const ZT_Certificate &cert);

	ZT_INLINE Certificate &operator=(const Certificate &cert) noexcept
	{
		if (likely(&cert != this)) {
			const ZT_Certificate *const sup = &cert;
			*this = *sup;
		}
		return *this;
	}

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
	ZT_Certificate_Network *addSubjectNetwork(uint64_t id, const ZT_Fingerprint &controller);

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
	void setExtendedAttributes(const Dictionary &x);

	/**
	 * Set the unique ID of this certificate's subject
	 *
	 * This must be done after all other fields in the subject are set.
	 *
	 * @param uniqueId Unique ID
	 * @param uniqueIdPrivate Private key associated with unique ID to prove ownership of it
	 * @return True if successful
	 */
	bool setSubjectUniqueId(const uint8_t uniqueId[ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE], const uint8_t uniqueIdPrivate[ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE]);

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
	 * @param len Length of marshalled certificate
	 * @return True if input is valid and was unmarshalled (signature is NOT checked)
	 */
	bool decode(const void *data, unsigned int len);

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
	 * Create a CSR that encodes the subject of this certificate
	 *
	 * @param s Subject to encode
	 * @param uniqueId Unique ID to sign subject with or NULL if none
	 * @param uniqueIdSize Size of unique ID or 0 if none
	 * @param uniqueIdPrivate Unique ID private key for proof signature or NULL if none
	 * @param uniqueIdPrivateSize Size of unique ID private key
	 * @return Encoded subject (without any unique ID fields) or empty vector on error
	 */
	static Vector< uint8_t > createCSR(const ZT_Certificate_Subject &s, const void *uniqueId, unsigned int uniqueIdSize, const void *uniqueIdPrivate, unsigned int uniqueIdPrivateSize);

	/**
	 * Create a subject unique ID and corresponding private key required for use
	 *
	 * @param uniqueId Buffer to receive unique ID
	 * @param uniqueIdPrivate Buffer to receive private key
	 */
	static ZT_INLINE void createSubjectUniqueId(uint8_t uniqueId[ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE], uint8_t uniqueIdPrivate[ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_PRIVATE_SIZE])
	{
		uniqueId[0] = ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384;
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
	void m_clear();

	static void m_encodeSubject(const ZT_Certificate_Subject &s, Dictionary &d, bool omitUniqueIdProofSignature);

	// These hold any identity or locator objects that are owned by and should
	// be deleted with this certificate. Lists are used so the pointers never
	// change.
	ForwardList< Identity > m_identities;
	ForwardList< Locator > m_locators;
	ForwardList< String > m_strings;
	ForwardList< SHA384Hash > m_serials;

	// These are stored in a vector because the memory needs to be contiguous.
	Vector< ZT_Certificate_Identity > m_subjectIdentities;
	Vector< ZT_Certificate_Network > m_subjectNetworks;
	Vector< const uint8_t * > m_subjectCertificates;
	Vector< const char * > m_updateUrls;
	Vector< uint8_t > m_extendedAttributes;
	Vector< uint8_t > m_subjectUniqueId;
	Vector< uint8_t > m_subjectUniqueIdProofSignature;
	Vector< uint8_t > m_signature;

	std::atomic< int > __refCount;
};

} // namespace ZeroTier

#endif
