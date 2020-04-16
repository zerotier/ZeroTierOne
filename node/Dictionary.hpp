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
#include "Containers.hpp"

namespace ZeroTier {

/**
 * A simple key-value store for short keys
 *
 * This data structure is used for network configurations, node meta-data,
 * and other open-definition protocol objects. It consists of a key-value
 * store with short (max: 8 characters) keys that map to strings, blobs,
 * or integers with the latter being by convention in hex format.
 *
 * If this seems a little odd, it is. It dates back to the very first alpha
 * versions of ZeroTier and if it were redesigned today we'd use some kind
 * of simple or standardized binary encoding. Nevertheless it is efficient
 * and it works so there is no need to change it and break backward
 * compatibility.
 */
class Dictionary
{
public:
	Dictionary();

	/**
	 * Get a reference to a value
	 *
	 * @param k Key to look up
	 * @return Reference to value
	 */
	std::vector<uint8_t> &operator[](const char *k);

	/**
	 * Get a const reference to a value
	 *
	 * @param k Key to look up
	 * @return Reference to value or to empty vector if not found
	 */
	const std::vector<uint8_t> &operator[](const char *k) const;

	/**
	 * Add a boolean as '1' or '0'
	 */
	void add(const char *k,bool v);

	/**
	 * Add an integer as a hexadecimal string value
	 */
	void add(const char *k,uint16_t v);

	/**
	 * Add an integer as a hexadecimal string value
	 */
	void add(const char *k,uint32_t v);

	/**
	 * Add an integer as a hexadecimal string value
	 */
	void add(const char *k,uint64_t v);

	ZT_INLINE void add(const char *k,int16_t v) { add(k,(uint16_t)v); }
	ZT_INLINE void add(const char *k,int32_t v) { add(k,(uint32_t)v); }
	ZT_INLINE void add(const char *k,int64_t v) { add(k,(uint64_t)v); }

	/**
	 * Add an address in 10-digit hex string format
	 */
	void add(const char *k,const Address &v);

	/**
	 * Add a C string as a value
	 */
	void add(const char *k,const char *v);

	/**
	 * Add a binary blob as a value
	 */
	void add(const char *k,const void *data,unsigned int len);

	/**
	 * Get a boolean
	 *
	 * @param k Key to look up
	 * @param dfl Default value (default: false)
	 * @return Value of key or default if not found
	 */
	bool getB(const char *k,bool dfl = false) const;

	/**
	 * Get an integer
	 *
	 * @param k Key to look up
	 * @param dfl Default value (default: 0)
	 * @return Value of key or default if not found
	 */
	uint64_t getUI(const char *k,uint64_t dfl = 0) const;

	/**
	 * Get a C string
	 *
	 * If the buffer is too small the string will be truncated, but the
	 * buffer will always end in a terminating null no matter what.
	 *
	 * @param k Key to look up
	 * @param v Buffer to hold string
	 * @param cap Maximum size of string (including terminating null)
	 */
	void getS(const char *k,char *v,unsigned int cap) const;

	/**
	 * Erase all entries in dictionary
	 */
	void clear();

	/**
	 * @return Number of entries
	 */
	ZT_INLINE unsigned int size() const noexcept { return m_entries.size(); }

	/**
	 * @return True if dictionary is not empty
	 */
	ZT_INLINE bool empty() const noexcept { return m_entries.empty(); }

	/**
	 * Encode to a string in the supplied vector
	 *
	 * This does not add a terminating zero. This must be pushed afterwords
	 * if the result is to be handled as a C string.
	 *
	 * @param out String encoded dictionary
	 */
	void encode(Vector<uint8_t> &out) const;

	/**
	 * Decode a string encoded dictionary
	 *
	 * This will decode up to 'len' but will also abort if it finds a
	 * null/zero as this could be a C string.
	 *
	 * @param data Data to decode
	 * @param len Length of data
	 * @return True if dictionary was formatted correctly and valid, false on error
	 */
	bool decode(const void *data,unsigned int len);

private:
	// This just packs up to 8 character bytes into a 64-bit word. There is no need
	// for this to be portable in terms of endian-ness. It's just for fast key lookup.
	static ZT_INLINE uint64_t s_toKey(const char *k)
	{
		uint64_t key = 0;
		for(int i=0;i<8;++i) {
			if ((reinterpret_cast<uint8_t *>(&key)[i] = *(k++)) == 0)
				break;
		}
		return key;
	}

	Map< uint64_t,Vector<uint8_t> > m_entries;
};

} // namespace ZeroTier

#endif
