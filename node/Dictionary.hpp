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

#ifndef ZT_DICTIONARY_HPP
#define ZT_DICTIONARY_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "Buf.hpp"

#include <cstdint>

#define ZT_DICTIONARY_MAX_CAPACITY 65536

namespace ZeroTier {

/**
 * A small (in code and data) packed key=value store
 *
 * This stores data in the form of a compact blob that is sort of human
 * readable (depending on whether you put binary data in it) and is backward
 * compatible with older versions. Binary data is escaped such that the
 * serialized form of a Dictionary is always a valid null-terminated C string.
 *
 * Keys are restricted: no binary data, no CR/LF, and no equals (=). If a key
 * contains these characters it may not be retrievable. This is not checked.
 *
 * Lookup is via linear search and will be slow with a lot of keys. It's
 * designed for small things.
 *
 * There is code to test and fuzz this in selftest.cpp. Fuzzing a blob of
 * pointer tricks like this is important after any modifications.
 *
 * This is used for network configurations and for saving some things on disk
 * in the ZeroTier One service code.
 *
 * @tparam C Dictionary max capacity in bytes
 */
class Dictionary
{
public:
	ZT_ALWAYS_INLINE Dictionary() { _d[0] = 0; }
	explicit ZT_ALWAYS_INLINE Dictionary(const char *s) { this->load(s); }
	Dictionary(const char *s,unsigned int len);

	ZT_ALWAYS_INLINE operator bool() const { return (_d[0] != 0); }

	/**
	 * Load a dictionary from a C-string
	 *
	 * @param s Dictionary in string form
	 * @return False if 's' was longer than our capacity
	 */
	bool load(const char *s);

	/**
	 * Delete all entries
	 */
	ZT_ALWAYS_INLINE void clear() { memset(_d,0,sizeof(_d)); }

	/**
	 * Get an entry
	 *
	 * Note that to get binary values, dest[] should be at least one more than
	 * the maximum size of the value being retrieved. That's because even if
	 * the data is binary a terminating 0 is still appended to dest[] after it.
	 *
	 * If the key is not found, dest[0] is set to 0 to make dest[] an empty
	 * C string in that case. The dest[] array will *never* be unterminated
	 * after this call.
	 *
	 * Security note: if 'key' is ever directly based on anything that is not
	 * a hard-code or internally-generated name, it must be checked to ensure
	 * that the buffer is NULL-terminated since key[] does not take a secondary
	 * size parameter. In NetworkConfig all keys are hard-coded strings so this
	 * isn't a problem in the core.
	 *
	 * @param key Key to look up
	 * @param dest Destination buffer
	 * @param destlen Size of destination buffer
	 * @return -1 if not found, or actual number of bytes stored in dest[] minus trailing 0
	 */
	int get(const char *key,char *dest,unsigned int destlen) const;

	/**
	 * Get a boolean value
	 *
	 * @param key Key to look up
	 * @param dfl Default value if not found in dictionary
	 * @return Boolean value of key or 'dfl' if not found
	 */
	ZT_ALWAYS_INLINE bool getB(const char *key,bool dfl = false) const
	{
		char tmp[4];
		if (this->get(key,tmp,sizeof(tmp)) >= 0)
			return ((*tmp == '1')||(*tmp == 't')||(*tmp == 'T'));
		return dfl;
	}

	/**
	 * Get an unsigned int64 stored as hex in the dictionary
	 *
	 * @param key Key to look up
	 * @param dfl Default value or 0 if unspecified
	 * @return Decoded hex UInt value or 'dfl' if not found
	 */
	ZT_ALWAYS_INLINE uint64_t getUI(const char *key,uint64_t dfl = 0) const
	{
		char tmp[128];
		if (this->get(key,tmp,sizeof(tmp)) >= 1)
			return Utils::hexStrToU64(tmp);
		return dfl;
	}

	/**
	 * Get an unsigned int64 stored as hex in the dictionary
	 *
	 * @param key Key to look up
	 * @param dfl Default value or 0 if unspecified
	 * @return Decoded hex UInt value or 'dfl' if not found
	 */
	ZT_ALWAYS_INLINE int64_t getI(const char *key,int64_t dfl = 0) const
	{
		char tmp[128];
		if (this->get(key,tmp,sizeof(tmp)) >= 1)
			return Utils::hexStrTo64(tmp);
		return dfl;
	}

	/**
	 * Add a new key=value pair
	 *
	 * If the key is already present this will append another, but the first
	 * will always be returned by get(). This is not checked. If you want to
	 * ensure a key is not present use erase() first.
	 *
	 * Use the vlen parameter to add binary values. Nulls will be escaped.
	 *
	 * @param key Key -- nulls, CR/LF, and equals (=) are illegal characters
	 * @param value Value to set
	 * @param vlen Length of value in bytes or -1 to treat value[] as a C-string and look for terminating 0
	 * @return True if there was enough room to add this key=value pair
	 */
	bool add(const char *key,const char *value,int vlen = -1);

	/**
	 * Add a boolean as a '1' or a '0'
	 */
	bool add(const char *key,bool value);

	/**
	 * Add a 64-bit integer (unsigned) as a hex value
	 */
	bool add(const char *key,uint64_t value);

	/**
	 * Add a 64-bit integer (unsigned) as a hex value
	 */
	bool add(const char *key,int64_t value);

	/**
	 * Add a 64-bit integer (unsigned) as a hex value
	 */
	bool add(const char *key,const Address &a);

	/**
	 * @param key Key to check
	 * @return True if key is present
	 */
	ZT_ALWAYS_INLINE bool contains(const char *key) const
	{
		char tmp[2];
		return (this->get(key,tmp,2) >= 0);
	}

	/**
	 * @return Value of C template parameter
	 */
	ZT_ALWAYS_INLINE unsigned int capacity() const { return sizeof(_d); }

	ZT_ALWAYS_INLINE const char *data() const { return _d; }

private:
	char _d[ZT_DICTIONARY_MAX_CAPACITY];
};

} // namespace ZeroTier

#endif
