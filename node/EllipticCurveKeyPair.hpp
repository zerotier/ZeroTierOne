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

#ifndef _ZT_ELLIPTICCURVEKEYPAIR_HPP
#define _ZT_ELLIPTICCURVEKEYPAIR_HPP

#include <string>
#include "EllipticCurveKey.hpp"

namespace ZeroTier {

/**
 * An elliptic curve key pair supporting generation and key agreement
 *
 * This is basically OpenSSL libcrypto glue.
 */
class EllipticCurveKeyPair
{
public:
	EllipticCurveKeyPair();
	EllipticCurveKeyPair(const EllipticCurveKeyPair &pair);
	EllipticCurveKeyPair(const EllipticCurveKey &pubk,const EllipticCurveKey &privk);
	~EllipticCurveKeyPair();

	const EllipticCurveKeyPair &operator=(const EllipticCurveKeyPair &pair);

	/**
	 * Fill this structure with a newly generated public/private key pair
	 *
	 * @return True if key generation is successful
	 */
	bool generate();

	/**
	 * Perform elliptic curve key agreement
	 *
	 * @param theirPublicKey Remote side's public key
	 * @param agreedUponKey Buffer to fill with agreed-upon symmetric key
	 * @param agreedUponKeyLength Number of bytes to generate
	 * @return True if key agreement is successful
	 */
	bool agree(const EllipticCurveKey &theirPublicKey,unsigned char *agreedUponKey,unsigned int agreedUponKeyLength) const;

	/**
	 * Sign a SHA256 hash
	 *
	 * @param sha256 Pointer to 256-bit / 32-byte SHA hash to sign
	 * @return ECDSA signature (r and s in binary format, each prefixed by an 8-bit size)
	 */
	std::string sign(const void *sha256) const;

	/**
	 * Sign something with this pair's private key, computing its hash first
	 *
	 * @param data Data to hash and sign
	 * @param len Length of data
	 * @return Signature bytes
	 */
	std::string sign(const void *data,unsigned int len) const;

	/**
	 * Verify a signature
	 *
	 * @param sha256 Pointer to 256-bit / 32-byte SHA hash to verify
	 * @param pk Public key to verify against
	 * @param sigbytes Signature bytes
	 * @param siglen Length of signature
	 */
	static bool verify(const void *sha256,const EllipticCurveKey &pk,const void *sigbytes,unsigned int siglen);

	/**
	 * Verify a signature
	 *
	 * @param data Data to verify
	 * @param len Length of data
	 * @param pk Public key to verify against
	 * @param sigbytes Signature bytes
	 * @param siglen Length of signature
	 */
	static bool verify(const void *data,unsigned int len,const EllipticCurveKey &pk,const void *sigbytes,unsigned int siglen);

	inline bool operator==(const EllipticCurveKeyPair &kp) const
		throw()
	{
		return ((_pub == kp._pub)&&(_priv == kp._priv));
	}
	inline bool operator!=(const EllipticCurveKeyPair &kp) const
		throw()
	{
		return ((_pub != kp._pub)||(_priv != kp._priv));
	}

	inline const EllipticCurveKey &pub() const throw() { return _pub; }
	inline const EllipticCurveKey &priv() const throw() { return _priv; }

private:
	bool initInternalKey();

	EllipticCurveKey _pub;
	EllipticCurveKey _priv;
	void *_internal_key;
};

} // namespace ZeroTier

#endif
