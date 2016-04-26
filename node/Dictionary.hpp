/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_DICTIONARY_HPP
#define ZT_DICTIONARY_HPP

#include "Constants.hpp"

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF

#include <stdint.h>

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

#include "Utils.hpp"

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
 * It's stored as a simple vector and can be linearly scanned or
 * binary searched. Dictionaries are only used for very small things
 * outside the core loop, so this is not a significant performance
 * issue and it reduces memory use and code footprint.
 */
class Dictionary : public std::vector< std::pair<std::string,std::string> >
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

	iterator find(const std::string &key);
	const_iterator find(const std::string &key) const;

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
	 * @param key Key to get
	 * @param dfl Default boolean result if key not found or empty (default: false)
	 * @return Boolean value of key
	 */
	bool getBoolean(const std::string &key,bool dfl = false) const;

	/**
	 * @param key Key to get
	 * @param dfl Default value if not present (default: 0)
	 * @return Value converted to unsigned 64-bit int or 0 if not found
	 */
	inline uint64_t getUInt(const std::string &key,uint64_t dfl = 0) const
	{
		const_iterator e(find(key));
		if (e == end())
			return dfl;
		return Utils::strToU64(e->second.c_str());
	}

	/**
	 * @param key Key to get
	 * @param dfl Default value if not present (default: 0)
	 * @return Value converted to unsigned 64-bit int or 0 if not found
	 */
	inline uint64_t getHexUInt(const std::string &key,uint64_t dfl = 0) const
	{
		const_iterator e(find(key));
		if (e == end())
			return dfl;
		return Utils::hexStrToU64(e->second.c_str());
	}

	/**
	 * @param key Key to get
	 * @param dfl Default value if not present (default: 0)
	 * @return Value converted to signed 64-bit int or 0 if not found
	 */
	inline int64_t getInt(const std::string &key,int64_t dfl = 0) const
	{
		const_iterator e(find(key));
		if (e == end())
			return dfl;
		return Utils::strTo64(e->second.c_str());
	}

	std::string &operator[](const std::string &key);

	/**
	 * @param key Key to set
	 * @param value String value
	 */
	inline void set(const std::string &key,const char *value)
	{
		(*this)[key] = value;
	}

	/**
	 * @param key Key to set
	 * @param value String value
	 */
	inline void set(const std::string &key,const std::string &value)
	{
		(*this)[key] = value;
	}

	/**
	 * @param key Key to set
	 * @param value Boolean value
	 */
	inline void set(const std::string &key,bool value)
	{
		(*this)[key] = ((value) ? "1" : "0");
	}

	/**
	 * @param key Key to set
	 * @param value Integer value
	 */
	inline void set(const std::string &key,uint64_t value)
	{
		char tmp[24];
		Utils::snprintf(tmp,sizeof(tmp),"%llu",(unsigned long long)value);
		(*this)[key] = tmp;
	}

	/**
	 * @param key Key to set
	 * @param value Integer value
	 */
	inline void set(const std::string &key,int64_t value)
	{
		char tmp[24];
		Utils::snprintf(tmp,sizeof(tmp),"%lld",(long long)value);
		(*this)[key] = tmp;
	}

	/**
	 * @param key Key to set
	 * @param value Integer value
	 */
	inline void setHex(const std::string &key,uint64_t value)
	{
		char tmp[24];
		Utils::snprintf(tmp,sizeof(tmp),"%llx",(unsigned long long)value);
		(*this)[key] = tmp;
	}

	/**
	 * @param key Key to check
	 * @return True if dictionary contains key
	 */
	inline bool contains(const std::string &key) const { return (find(key) != end()); }

	/**
	 * @return String-serialized dictionary
	 */
	std::string toString() const;

	/**
	 * Clear and initialize from a string
	 *
	 * @param s String-serialized dictionary
	 * @param maxlen Maximum length of string buffer
	 */
	void fromString(const char *s,unsigned int maxlen);
	inline void fromString(const std::string &s) { fromString(s.c_str(),(unsigned int)s.length()); }
	void updateFromString(const char *s,unsigned int maxlen);
	inline void update(const char *s,unsigned int maxlen) { updateFromString(s, maxlen); }
	inline void update(const std::string &s) { updateFromString(s.c_str(),(unsigned int)s.length()); }

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
	 * @param key Key to erase
	 */
	void eraseKey(const std::string &key);

	/**
	 * Remove any signature from this dictionary
	 */
	inline void removeSignature()
	{
		eraseKey(ZT_DICTIONARY_SIGNATURE);
		eraseKey(ZT_DICTIONARY_SIGNATURE_IDENTITY);
		eraseKey(ZT_DICTIONARY_SIGNATURE_TIMESTAMP);
	}

	/**
	 * Add or update signature fields with a signature of all other keys and values
	 *
	 * @param with Identity to sign with (must have secret key)
	 * @param now Current time
	 * @return True on success
	 */
	bool sign(const Identity &id,uint64_t now);

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

#endif // ZT_SUPPORT_OLD_STYLE_NETCONF

#endif
