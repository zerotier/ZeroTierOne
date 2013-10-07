/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_CERTIFICATEOFMEMBERSHIP_HPP
#define _ZT_CERTIFICATEOFMEMBERSHIP_HPP

#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "Constants.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Identity.hpp"

namespace ZeroTier {

/**
 * Certificate of network membership
 *
 * The COM contains a sorted set of three-element tuples called qualifiers.
 * These contain an id, a value, and a maximum delta.
 *
 * The ID is arbitrary and should be assigned using a scheme that makes
 * every ID globally unique. ID 0 is reserved for the always-present
 * validity timestamp and range, and ID 1 is reserved for the always-present
 * network ID. IDs less than 65536 are reserved for future global
 * assignment.
 *
 * The value's meaning is ID-specific and isn't important here. What's
 * important is the value and the third member of the tuple: the maximum
 * delta. The maximum delta is the maximum difference permitted between
 * values for a given ID between certificates for the two certificates to
 * themselves agree.
 *
 * Network membership is checked by checking whether a peer's certificate
 * agrees with your own. The timestamp provides the fundamental criterion--
 * each member of a private network must constantly obtain new certificates
 * often enough to stay within the max delta for this qualifier. But other
 * criteria could be added in the future for very special behaviors, things
 * like latitude and longitude for instance.
 */
class CertificateOfMembership
{
public:
	/**
	 * Certificate type codes, used in serialization
	 *
	 * Only one so far, and only one hopefully there shall be for quite some
	 * time.
	 */
	enum Type
	{
		COM_UINT64_ED25519 = 1 // tuples of unsigned 64's signed with Ed25519
	};

	/**
	 * Reserved COM IDs
	 *
	 * IDs below 65536 should be considered reserved for future global
	 * assignment here.
	 */
	enum ReservedIds
	{
		COM_RESERVED_ID_TIMESTAMP = 0, // timestamp, max delta defines cert life
		COM_RESERVED_ID_NETWORK_ID = 1 // network ID, max delta always 0
	};

	CertificateOfMembership() { memset(_signature.data,0,_signature.size()); }
	CertificateOfMembership(const char *s) { fromString(s); }
	CertificateOfMembership(const std::string &s) { fromString(s.c_str()); }

	/**
	 * Add or update a qualifier in this certificate
	 *
	 * Any signature is invalidated and signedBy is set to null.
	 *
	 * @param id Qualifier ID
	 * @param value Qualifier value
	 * @param maxDelta Qualifier maximum allowed difference (absolute value of difference)
	 */
	void setQualifier(uint64_t id,uint64_t value,uint64_t maxDelta);

	/**
	 * @return String-serialized representation of this certificate
	 */
	std::string toString() const;

	/**
	 * Set this certificate equal to the hex-serialized string
	 *
	 * Invalid strings will result in invalid or undefined certificate
	 * contents. These will subsequently fail validation and comparison.
	 *
	 * @param s String to deserialize
	 */
	void fromString(const char *s);
	inline void fromString(const std::string &s) { fromString(s.c_str()); }

	/**
	 * Compare two certificates for parameter agreement
	 *
	 * This compares this certificate with the other and returns true if all
	 * paramters in this cert are present in the other and if they agree to
	 * within this cert's max delta value for each given parameter.
	 *
	 * Tuples present in other but not in this cert are ignored, but any
	 * tuples present in this cert but not in other result in 'false'.
	 *
	 * @param other Cert to compare with
	 * @return True if certs agree and 'other' may be communicated with
	 */
	bool agreesWith(const CertificateOfMembership &other) const
		throw();

	/**
	 * Sign this certificate
	 *
	 * @param with Identity to sign with, must include private key
	 * @return True if signature was successful
	 */
	bool sign(const Identity &with);

	/**
	 * Verify certificate against an identity
	 *
	 * @param id Identity to verify against
	 * @return True if certificate is signed by this identity and verification was successful
	 */
	bool verify(const Identity &id) const;

	/**
	 * @return True if signed
	 */
	inline bool isSigned() const throw() { return (_signedBy); }

	/**
	 * @return Address that signed this certificate or null address if none
	 */
	inline const Address &signedBy() const throw() { return _signedBy; }

private:
	struct _Qualifier
	{
		_Qualifier() throw() {}
		_Qualifier(uint64_t i,uint64_t v,uint64_t m) throw() :
			id(i),
			value(v),
			maxDelta(m) {}

		uint64_t id;
		uint64_t value;
		uint64_t maxDelta;

		inline bool operator==(const _Qualifier &q) const throw() { return (id == q.id); } // for unique
		inline bool operator<(const _Qualifier &q) const throw() { return (id < q.id); } // for sort
	};

	std::vector<_Qualifier> _qualifiers; // sorted by id and unique
	Address _signedBy;
	C25519::Signature _signature;
};

} // namespace ZeroTier

#endif
