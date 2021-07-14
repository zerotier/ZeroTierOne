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

#ifndef ZT_CERTIFICATE_HPP
#define ZT_CERTIFICATE_HPP

#include "C25519.hpp"
#include "Constants.hpp"
#include "Containers.hpp"
#include "Dictionary.hpp"
#include "ECC384.hpp"
#include "Identity.hpp"
#include "Locator.hpp"
#include "SHA512.hpp"
#include "Utils.hpp"

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
class Certificate : public ZT_Certificate {
  public:
    Certificate() noexcept;
    explicit Certificate(const ZT_Certificate &apiCert);
    Certificate(const Certificate &cert);
    ~Certificate();

    Certificate &operator=(const ZT_Certificate &cert);

    ZT_INLINE Certificate &operator=(const Certificate &cert) noexcept
    {
        if (likely(&cert != this)) {
            const ZT_Certificate *const sup = &cert;
            *this                           = *sup;
        }
        return *this;
    }

    /**
     * @return Serial number in a H384 object
     */
    ZT_INLINE H384 getSerialNo() const noexcept { return H384(this->serialNo); }

    /**
     * @return True if this is a self-signed certificate
     */
    ZT_INLINE bool isSelfSigned() const noexcept
    {
        for (unsigned int i = 0; i < ZT_CERTIFICATE_HASH_SIZE; ++i) {
            if (this->issuer[i] != 0xff)
                return false;
        }
        return true;
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
     * Add an update URL to the updateUrls list
     *
     * @param url Update URL
     */
    void addSubjectUpdateUrl(const char *url);

    /**
     * Sign subject with unique ID private key and set.
     *
     * This is done when you createCSR but can also be done explicitly here. This
     * is mostly for testing purposes.
     *
     * @param uniqueIdPrivate Unique ID private key (includes public)
     * @param uniqueIdPrivateSize Size of private key
     * @return True on success
     */
    ZT_INLINE bool setSubjectUniqueId(const void *uniqueIdPrivate, unsigned int uniqueIdPrivateSize) { return m_setSubjectUniqueId(this->subject, uniqueIdPrivate, uniqueIdPrivateSize); }

    /**
     * Marshal this certificate in binary form
     *
     * The internal encoding used here is Dictionary to permit easy
     * extensibility.
     *
     * @param omitSignature If true omit the signature field (for signing and verification, default is false)
     * @return Marshaled certificate
     */
    Vector<uint8_t> encode(bool omitSignature = false) const;

    /**
     * Decode this certificate from marshaled bytes.
     *
     * @param data Marshalled certificate
     * @param len Length of marshalled certificate
     * @return True if input is valid and was unmarshalled (signature is NOT checked)
     */
    bool decode(const void *data, unsigned int len);

    /**
     * Sign this certificate.
     *
     * This sets serialNo, issuer, issuerPublicKey, and signature.
     *
     * @return True on success
     */
    bool sign(const uint8_t issuer[ZT_CERTIFICATE_HASH_SIZE], const void *issuerPrivateKey, unsigned int issuerPrivateKeySize);

    /**
     * Verify self-contained signatures and validity of certificate structure
     *
     * This cannot check the chain of trust back to a CA, only the internal validity
     * of this certificate.
     *
     * @param clock If non-negative, also do verifyTimeWindow()
     * @param checkSignatures If true, perform full signature check (which is more expensive than other checks)
     * @return OK (0) or error code indicating why certificate failed verification.
     */
    ZT_CertificateError verify(int64_t clock, bool checkSignatures) const;

    /**
     * Check this certificate's expiration status
     *
     * @param clock Current real world time in milliseconds since epoch
     * @return True if certificate is not expired or outside window
     */
    ZT_INLINE bool verifyTimeWindow(int64_t clock) const noexcept { return ((clock >= this->validity[0]) && (clock <= this->validity[1]) && (this->validity[0] <= this->validity[1])); }

    /**
     * Create a new certificate public/private key pair
     *
     * @param type Key pair type to create
     * @param publicKey Buffer to fill with public key
     * @param publicKeySize Result parameter: set to size of public key
     * @param privateKey Buffer to fill with private key
     * @param privateKeySize Result parameter: set to size of private key
     * @return True on success
     */
    static bool newKeyPair(const ZT_CertificatePublicKeyAlgorithm type, uint8_t publicKey[ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE], int *const publicKeySize, uint8_t privateKey[ZT_CERTIFICATE_MAX_PRIVATE_KEY_SIZE], int *const privateKeySize);

    /**
     * Create a CSR that encodes the subject of this certificate
     *
     * @param s Subject to encode
     * @param certificatePrivateKey Private key for certificate (includes public)
     * @param certificatePrivateKeySize Size of private
     * @param uniqueIdPrivate Unique ID private key for proof signature or NULL if none
     * @param uniqueIdPrivateSize Size of unique ID private key
     * @return Encoded subject (without any unique ID fields) or empty vector on error
     */
    static Vector<uint8_t> createCSR(const ZT_Certificate_Subject &s, const void *certificatePrivateKey, unsigned int certificatePrivateKeySize, const void *uniqueIdPrivate, unsigned int uniqueIdPrivateSize);

    ZT_INLINE unsigned long hashCode() const noexcept { return (unsigned long)Utils::loadMachineEndian<uint32_t>(this->serialNo); }

    ZT_INLINE bool operator==(const ZT_Certificate &c) const noexcept { return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) == 0; }

    ZT_INLINE bool operator!=(const ZT_Certificate &c) const noexcept { return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) != 0; }

    ZT_INLINE bool operator<(const ZT_Certificate &c) const noexcept { return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) < 0; }

    ZT_INLINE bool operator<=(const ZT_Certificate &c) const noexcept { return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) <= 0; }

    ZT_INLINE bool operator>(const ZT_Certificate &c) const noexcept { return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) > 0; }

    ZT_INLINE bool operator>=(const ZT_Certificate &c) const noexcept { return memcmp(this->serialNo, c.serialNo, ZT_SHA384_DIGEST_SIZE) >= 0; }

  private:
    void m_clear();
    static bool m_setSubjectUniqueId(ZT_Certificate_Subject &s, const void *uniqueIdPrivate, unsigned int uniqueIdPrivateSize);
    static void m_encodeSubject(const ZT_Certificate_Subject &s, Dictionary &d, bool omitUniqueIdProofSignature);

    // These hold any identity or locator objects that are owned by and should
    // be deleted with this certificate. Lists are used so the pointers never
    // change.
    ForwardList<Identity> m_identities;
    ForwardList<Locator> m_locators;
    ForwardList<String> m_strings;

    // These are stored in a vector because the memory needs to be contiguous.
    Vector<ZT_Certificate_Identity> m_subjectIdentities;
    Vector<ZT_Certificate_Network> m_subjectNetworks;
    Vector<const char *> m_updateUrls;
    Vector<uint8_t> m_extendedAttributes;
};

}   // namespace ZeroTier

#endif
