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

#ifndef ZT_TRUSTSTORE_HPP
#define ZT_TRUSTSTORE_HPP

#include "Constants.hpp"
#include "Context.hpp"
#include "Containers.hpp"
#include "Certificate.hpp"
#include "SHA512.hpp"
#include "SharedPtr.hpp"
#include "Identity.hpp"
#include "Fingerprint.hpp"
#include "Mutex.hpp"
#include "Peer.hpp"

namespace ZeroTier {

/**
 * Certificate store and chain validator.
 *
 * WARNING: SharedPtr<Entry> entries returned from a trust store are valid
 * only as long as the trust store exists. The trust store is a core object
 * that lives as long as a Node, so this isn't an issue in the core, but it
 * should be remembered when testing.
 *
 * This is because each Entry includes a reference to its parent's mutex and
 * is synchronized by this mutex so its fields are safe to access while the
 * parent trust store is being modified or synchronized.
 *
 * This also means entries can't be moved between TrustStore instances,
 * hence there are no methods for doing that. There's only one instance in a
 * node anyway.
 */
class TrustStore
{
public:
	/**
	 * An entry in the node certificate trust store
	 */
	class Entry
	{
		friend class SharedPtr< TrustStore::Entry >;
		friend class SharedPtr< const TrustStore::Entry >;
		friend class TrustStore;

	public:
		/**
		 * @return Reference to held certificate
		 */
		ZT_INLINE const Certificate &certificate() const noexcept
		{ return m_certificate; }

		/**
		 * Get the local trust for this certificate
		 *
		 * This value may be changed dynamically by calls to update().
		 *
		 * @return Local trust bit mask
		 */
		ZT_INLINE unsigned int localTrust() const noexcept
		{
			RWMutex::RLock l(m_lock);
			return m_localTrust;
		}

		/**
		 * Change the local trust of this entry
		 *
		 * @param lt New local trust bit mask
		 */
		ZT_INLINE void setLocalTrust(const unsigned int lt) noexcept
		{
			RWMutex::Lock l(m_lock);
			m_localTrust = lt;
		}

		/**
		 * Get the error code for this certificate
		 *
		 * @return Error or ZT_CERTIFICATE_ERROR_NONE if none
		 */
		ZT_INLINE ZT_CertificateError error() const noexcept
		{
			RWMutex::RLock l(m_lock);
			return m_error;
		}

	private:
		Entry &operator=(const Entry &) { return *this; }

		ZT_INLINE Entry(RWMutex &l, const Certificate &cert, const unsigned int lt) noexcept:
			__refCount(0),
			m_lock(l),
			m_certificate(cert),
			m_localTrust(lt),
			m_error(ZT_CERTIFICATE_ERROR_NONE),
			m_subjectDeprecated(false),
			m_onTrustPath(false)
		{}

		std::atomic< int > __refCount;

		RWMutex &m_lock;
		const Certificate m_certificate;
		unsigned int m_localTrust;
		ZT_CertificateError m_error;
		bool m_subjectDeprecated;
		bool m_onTrustPath;
	};

	TrustStore();
	~TrustStore();

	/**
	 * Get certificate by certificate serial number
	 *
	 * Note that the error code should be checked. The certificate may be
	 * rejected and may still be in the store unless the store has been
	 * purged.
	 *
	 * @param serial SHA384 hash of certificate
	 * @return Entry or empty/nil if not found
	 */
	SharedPtr< Entry > get(const H384 &serial) const;

	/**
	 * Get roots specified by root set certificates in the local store.
	 *
	 * If more than one certificate locally trusted as a root set specifies
	 * the root, it will be returned once (as per Map behavior) but the latest
	 * locator will be returned from among those available.
	 *
	 * @return Roots and the latest locator specified for each (if any)
	 */
	Map< Identity, SharedPtr< const Locator > > roots();

	/**
	 * @param includeRejectedCertificates If true, also include certificates with error codes
	 * @return All certificates in asecending sort order by serial
	 */
	Vector< SharedPtr< Entry > > all(bool includeRejectedCertificates) const;

	/**
	 * Add a certificate
	 *
	 * A copy is made so it's fine if the original is freed after this call. If
	 * the certificate already exists its local trust flags are updated.
	 *
	 * IMPORTANT: The caller MUST also call update() after calling add() one or
	 * more times to actually add and revalidate certificates and their signature
	 * chains.
	 *
	 * @param cert Certificate to add
	 */
	void add(const Certificate &cert, unsigned int localTrust);

	/**
	 * Queue a certificate to be deleted
	 *
	 * Actual delete does not happen until the next update().
	 *
	 * @param serial Serial of certificate to delete
	 */
	void erase(const H384 &serial);

	/**
	 * Validate all certificates and their certificate chains
	 *
	 * This also processes any certificates added with add() since the last call to update().
	 *
	 * @param clock Current time in milliseconds since epoch, or -1 to not check times on this pass
	 * @param purge If non-NULL, purge rejected certificates and return them in this vector (vector should be empty)
	 * @return True if there were changes
	 */
	bool update(int64_t clock, Vector< SharedPtr< Entry > > *purge);

	/**
	 * Create a compressed binary version of certificates and their local trust
	 *
	 * @return Binary compressed certificates and local trust info
	 */
	Vector< uint8_t > save() const;

	/**
	 * Decode a saved trust store
	 *
	 * Decoded certificates are added to the add queue, so update() must be
	 * called after this to actually apply them.
	 *
	 * @param data Data to decode
	 * @return Number of certificates or -1 if input is invalid
	 */
	int load(const Vector< uint8_t > &data);

private:
	Map< H384, SharedPtr< Entry > > m_bySerial; // all certificates
	Map< Blob< ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE >, SharedPtr< Entry > > m_bySubjectUniqueId; // non-rejected certificates only
	Map< Fingerprint, Vector< SharedPtr< Entry > > > m_bySubjectIdentity; // non-rejected certificates only
	ForwardList< SharedPtr< Entry > > m_addQueue;
	ForwardList< H384 > m_deleteQueue;
	RWMutex m_lock;
};

} // namespace ZeroTier

#endif
