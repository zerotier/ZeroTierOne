/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#ifndef ZT_DICTIONARY_HPP
#define ZT_DICTIONARY_HPP

#include <stdint.h>

#include <string>
#include <map>
#include <stdexcept>
 
#include "Constants.hpp"

// Three fields are added/updated by sign()
#define ZT_DICTIONARY_SIGNATURE "~!ed25519"
#define ZT_DICTIONARY_SIGNATURE_IDENTITY "~!sigid"
#define ZT_DICTIONARY_SIGNATURE_TIMESTAMP "~!sigts"

namespace ZeroTier {

class Identity;

/**
 * Simple key/value dictionary with string serialization
 *
 * The serialization format is a flat key=value with backslash escape.
 * It does not support comments or other syntactic complexities. It is
 * human-readable if the keys and values in the dictionary are also
 * human-readable. Otherwise it might contain unprintable characters.
 *
 * Keys beginning with "~!" are reserved for signature data fields.
 *
 * Note: the signature code depends on std::map<> being sorted, but no
 * other code does. So if the underlying data structure is ever swapped
 * out for an unsorted one, the signature code will have to be updated
 * to sort before composing the string to sign.
 */
class Dictionary : public std::map<std::string,std::string>
{
public:
	Dictionary() {}

	/**
	 * @param s String-serialized dictionary
	 * @param maxlen Maximum length of buffer
	 */
	Dictionary(const char *s,unsigned int maxlen) { fromString(s,maxlen); }

	/**
	 * @param s String-serialized dictionary
	 */
	Dictionary(const std::string &s) { fromString(s.c_str(),(unsigned int)s.length()); }

	/**
	 * Get a key, throwing an exception if it is not present
	 *
	 * @param key Key to look up
	 * @return Reference to value
	 * @throws std::invalid_argument Key not found
	 */
	inline const std::string &get(const std::string &key) const
		throw(std::invalid_argument)
	{
		const_iterator e(find(key));
		if (e == end())
			throw std::invalid_argument(std::string("missing required field: ")+key);
		return e->second;
	}

	/**
	 * Get a key, returning a default if not present
	 *
	 * @param key Key to look up
	 * @param dfl Default if not present
	 * @return Value or default
	 */
	inline const std::string &get(const std::string &key,const std::string &dfl) const
	{
		const_iterator e(find(key));
		if (e == end())
			return dfl;
		return e->second;
	}

	/**
	 * @param key Key to check
	 * @return True if dictionary contains key
	 */
	inline bool contains(const std::string &key) const { return (find(key) != end()); }

	/**
	 * @return String-serialized dictionary
	 */
	inline std::string toString() const
	{
		std::string s;
		for(const_iterator kv(begin());kv!=end();++kv) {
			_appendEsc(kv->first.data(),(unsigned int)kv->first.length(),s);
			s.push_back('=');
			_appendEsc(kv->second.data(),(unsigned int)kv->second.length(),s);
			s.append(ZT_EOL_S);
		}
		return s;
	}

	/**
	 * Clear and initialize from a string
	 *
	 * @param s String-serialized dictionary
	 * @param maxlen Maximum length of string buffer
	 */
	void fromString(const char *s,unsigned int maxlen);
	inline void fromString(const std::string &s) { fromString(s.c_str(),(unsigned int)s.length()); }

	/**
	 * @return True if this dictionary is cryptographically signed
	 */
	inline bool hasSignature() const { return (find(ZT_DICTIONARY_SIGNATURE) != end()); }

	/**
	 * @return Signing identity in string-serialized format or empty string if none
	 */
	inline std::string signingIdentity() const { return get(ZT_DICTIONARY_SIGNATURE_IDENTITY,std::string()); }

	/**
	 * @return Signature timestamp in milliseconds since epoch or 0 if none
	 */
	uint64_t signatureTimestamp() const;

	/**
	 * Remove any signature from this dictionary
	 */
	inline void removeSignature()
	{
		erase(ZT_DICTIONARY_SIGNATURE);
		erase(ZT_DICTIONARY_SIGNATURE_IDENTITY);
		erase(ZT_DICTIONARY_SIGNATURE_TIMESTAMP);
	}

	/**
	 * Add or update signature fields with a signature of all other keys and values
	 *
	 * @param with Identity to sign with (must have secret key)
	 * @return True on success
	 */
	bool sign(const Identity &id);

	/**
	 * Verify signature against an identity
	 *
	 * @param id Identity to verify against
	 * @return True if signature verification OK
	 */
	bool verify(const Identity &id) const;

private:
	void _mkSigBuf(std::string &buf) const;
	static void _appendEsc(const char *data,unsigned int len,std::string &to);
};

} // namespace ZeroTier

#endif
