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
#include "RuntimeEnvironment.hpp"
#include "Containers.hpp"
#include "Certificate.hpp"
#include "Blob.hpp"
#include "SHA512.hpp"
#include "SharedPtr.hpp"
#include "Identity.hpp"
#include "Fingerprint.hpp"
#include "Mutex.hpp"
#include "Peer.hpp"

namespace ZeroTier {

/**
 * Certificate store and chain validator
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

	private:
		ZT_INLINE Entry(const Certificate &cert, const unsigned int lt) noexcept:
			m_certificate(cert),
			m_localTrust(lt)
		{}

	public:
		ZT_INLINE const Certificate &certificate() const noexcept
		{ return m_certificate; }

		ZT_INLINE unsigned int localTrust() const noexcept
		{ return m_localTrust.load(std::memory_order_relaxed); }

	private:
		Certificate m_certificate;
		std::atomic< unsigned int > m_localTrust;
		std::atomic< int > __refCount;
	};

	TrustStore();

	~TrustStore();

	/**
	 * Get certificate by certificate serial number
	 *
	 * @param serial SHA384 hash of certificate
	 * @return Entry or empty/nil if not found
	 */
	SharedPtr< const Entry > get(const SHA384Hash &serial) const;

	/**
	 * Get current root peers based on root-enumerating certs in trust store
	 *
	 * Root peers are created or obtained via this node's Topology. This should
	 * never be called while relevant data structures in Topology are locked.
	 *
	 * Locators in root peers are also updated if the locator present in the
	 * certificate is valid and newer.
	 *
	 * @param tPtr Caller pointer
	 * @param RR Runtime environment
	 * @return All roots (sort order undefined)
	 */
	Vector< SharedPtr< Peer > > roots(void *tPtr, const RuntimeEnvironment *RR);

	/**
	 * @return All certificates in asecending sort order by serial
	 */
	Vector< SharedPtr< const Entry > > all() const;

	/**
	 * Add a certificate
	 *
	 * A copy is made so it's fine if the original is freed after this call.
	 *
	 * IMPORTANT: The caller MUST also call update() after calling add() one or
	 * more times to actually add and revalidate certificates and their signature
	 * chains.
	 *
	 * @param cert Certificate to add
	 */
	void add(const Certificate &cert, unsigned int localTrust);

	/**
	 * Validate all certificates and their certificate chains
	 *
	 * This also processes any certificates added with add() since the last call to update().
	 *
	 * @param clock Current time in milliseconds since epoch
	 * @param purge If non-NULL, purge rejected certificates and return them in this vector (vector should be empty)
	 */
	void update(int64_t clock, Vector< std::pair< SharedPtr<Entry>, ZT_CertificateError > > *purge);

	/**
	 * Get a copy of the current rejected certificate set.
	 *
	 * @return Rejected certificates
	 */
	Vector< std::pair< SharedPtr<Entry>, ZT_CertificateError > > rejects() const;

private:
	Map< SHA384Hash, SharedPtr< Entry > > m_bySerial;
	Map< Vector< uint8_t >, SharedPtr< Entry > > m_bySubjectUniqueId;
	Map< Fingerprint, Vector< SharedPtr< Entry > > > m_bySubjectIdentity;
	ForwardList< SharedPtr< Entry > > m_addQueue;
	Map< SharedPtr< Entry >, ZT_CertificateError > m_rejected;
	RWMutex m_lock;
};

} // namespace ZeroTier

#endif
