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

#ifndef ZT_DICTIONARY_HPP
#define ZT_DICTIONARY_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "Buf.hpp"
#include "Containers.hpp"

namespace ZeroTier {

class Identity;

/**
 * A simple key-value store for short keys
 *
 * This data structure is used for network configurations, node meta-data,
 * and other open-definition protocol objects.
 *
 * This is technically a binary encoding, but with delimiters chosen so that
 * it looks like a series of key=value lines of the keys and values are
 * human-readable strings.
 *
 * The fastest way to build a dictionary to send is to use the append
 * static functions, not to populate and then encode a Dictionary.
 */
class Dictionary
{
public:
	typedef SortedMap< String, Vector< uint8_t > >::const_iterator const_iterator;

	ZT_INLINE Dictionary()
	{}

	ZT_INLINE ~Dictionary()
	{}

	/*
	ZT_INLINE void dump() const
	{
		printf("\n--\n");
		for (const_iterator e(begin()); e != end(); ++e) {
			printf("%.8x  %s=", Utils::fnv1a32(e->second.data(), (unsigned int)e->second.size()), e->first.c_str());
			bool binary = false;
			for (Vector< uint8_t >::const_iterator c(e->second.begin()); c != e->second.end(); ++c) {
				if ((*c < 33) || (*c > 126)) {
					binary = true;
					break;
				}
			}
			if (binary) {
				for (Vector< uint8_t >::const_iterator c(e->second.begin()); c != e->second.end(); ++c)
					printf("%.2x", (unsigned int)*c);
			} else {
				Vector< uint8_t > s(e->second);
				s.push_back(0);
				printf("%s", s.data());
			}
			printf("\n");
		}
		printf("--\n");
	}
	*/

	/**
	 * Get a reference to a value
	 *
	 * @param k Key to look up
	 * @return Reference to value
	 */
	ZT_MAYBE_UNUSED Vector< uint8_t > &operator[](const char *k);

	/**
	 * Get a const reference to a value
	 *
	 * @param k Key to look up
	 * @return Reference to value or to empty vector if not found
	 */
	ZT_MAYBE_UNUSED const Vector< uint8_t > &operator[](const char *k) const;

	/**
	 * @return Start of key->value pairs
	 */
	ZT_MAYBE_UNUSED ZT_INLINE const_iterator begin() const noexcept
	{ return m_entries.begin(); }

	/**
	 * @return End of key->value pairs
	 */
	ZT_MAYBE_UNUSED ZT_INLINE const_iterator end() const noexcept
	{ return m_entries.end(); }

	/**
	 * Add an integer as a hexadecimal string value
	 *
	 * @param k Key to set
	 * @param v Integer to set, will be cast to uint64_t and stored as hex
	 */
	ZT_MAYBE_UNUSED ZT_INLINE void add(const char *const k, const uint64_t v)
	{
		char buf[24];
		add(k, Utils::hex((uint64_t)(v), buf));
	}

	/**
	 * Add an integer as a hexadecimal string value
	 *
	 * @param k Key to set
	 * @param v Integer to set, will be cast to uint64_t and stored as hex
	 */
	ZT_MAYBE_UNUSED ZT_INLINE void add(const char *const k, const int64_t v)
	{
		char buf[24];
		add(k, Utils::hex((uint64_t)(v), buf));
	}

	/**
	 * Add an address in 10-digit hex string format
	 */
	ZT_MAYBE_UNUSED void add(const char *k, const Address &v);

	/**
	 * Add a C string as a value
	 */
	ZT_MAYBE_UNUSED void add(const char *k, const char *v);

	/**
	 * Add a binary blob as a value
	 */
	ZT_MAYBE_UNUSED void add(const char *k, const void *data, unsigned int len);

	/**
	 * Get an integer
	 *
	 * @param k Key to look up
	 * @param dfl Default value (default: 0)
	 * @return Value of key or default if not found
	 */
	ZT_MAYBE_UNUSED uint64_t getUI(const char *k, uint64_t dfl = 0) const;

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
	ZT_MAYBE_UNUSED char *getS(const char *k, char *v, unsigned int cap) const;

	/**
	 * Get an object supporting the marshal/unmarshal interface pattern
	 * 
	 * @tparam T Object type (inferred)
	 * @param k Key to look up
	 * @param obj Object to unmarshal() into
	 * @return True if unmarshal was successful
	 */
	template< typename T >
	ZT_MAYBE_UNUSED ZT_INLINE bool getO(const char *k, T &obj) const
	{
		const Vector< uint8_t > &d = (*this)[k];
		if (d.empty())
			return false;
		return (obj.unmarshal(d.data(), (unsigned int)d.size()) > 0);
	}

	/**
	 * Add an object supporting the marshal/unmarshal interface pattern
	 *
	 * @tparam T Object type (inferred)
	 * @param k Key to add
	 * @param obj Object to marshal() into vector
	 * @return True if successful
	 */
	template< typename T >
	ZT_MAYBE_UNUSED ZT_INLINE bool addO(const char *k, T &obj)
	{
		Vector< uint8_t > &d = (*this)[k];
		d.resize(T::marshalSizeMax());
		const int l = obj.marshal(d.data());
		if (l > 0) {
			d.resize(l);
			return true;
		}
		d.clear();
		return false;
	}

	/**
	 * Erase all entries in dictionary
	 */
	ZT_MAYBE_UNUSED void clear();

	/**
	 * @return Number of entries
	 */
	ZT_MAYBE_UNUSED ZT_INLINE unsigned int size() const noexcept
	{ return (unsigned int)m_entries.size(); }

	/**
	 * @return True if dictionary is not empty
	 */
	ZT_MAYBE_UNUSED ZT_INLINE bool empty() const noexcept
	{ return m_entries.empty(); }

	/**
	 * Encode to a string in the supplied vector
	 *
	 * @param out String encoded dictionary
	 */
	ZT_MAYBE_UNUSED void encode(Vector< uint8_t > &out) const;

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
	ZT_MAYBE_UNUSED bool decode(const void *data, unsigned int len);

	/**
	 * Append a key=value pair to a buffer (vector or FCV)
	 * 
	 * @param out Buffer
	 * @param k Key (must be <= 8 characters)
	 * @param v Value
	 */
	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const bool v)
	{
		s_appendKey(out, k);
		out.push_back((uint8_t)(v ? '1' : '0'));
		out.push_back((uint8_t)'\n');
	}

	/**
	 * Append a key=value pair to a buffer (vector or FCV)
	 * 
	 * @param out Buffer
	 * @param k Key (must be <= 8 characters)
	 * @param v Value
	 */
	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const Address v)
	{
		s_appendKey(out, k);
		const uint64_t a = v.toInt();
		static_assert(ZT_ADDRESS_LENGTH_HEX == 10, "this must be rewritten for any change in address length");
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 36U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 32U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 28U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 24U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 20U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 16U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 12U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 8U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[(a >> 4U) & 0xfU]);
		out.push_back((uint8_t)Utils::HEXCHARS[a & 0xfU]);
		out.push_back((uint8_t)'\n');
	}

	/**
	 * Append a key=value pair to a buffer (vector or FCV)
	 * 
	 * @param out Buffer
	 * @param k Key (must be <= 8 characters)
	 * @param v Value
	 */
	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const uint64_t v)
	{
		s_appendKey(out, k);
		char buf[17];
		Utils::hex(v, buf);
		unsigned int i = 0;
		while (buf[i])
			out.push_back((uint8_t)buf[i++]);
		out.push_back((uint8_t)'\n');
	}

	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const int64_t v)
	{ append(out, k, (uint64_t)v); }

	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const uint32_t v)
	{ append(out, k, (uint64_t)v); }

	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const int32_t v)
	{ append(out, k, (uint64_t)v); }

	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const uint16_t v)
	{ append(out, k, (uint64_t)v); }

	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const int16_t v)
	{ append(out, k, (uint64_t)v); }

	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const uint8_t v)
	{ append(out, k, (uint64_t)v); }

	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const int8_t v)
	{ append(out, k, (uint64_t)v); }

	/**
	 * Append a key=value pair to a buffer (vector or FCV)
	 * 
	 * @param out Buffer
	 * @param k Key (must be <= 8 characters)
	 * @param v Value
	 */
	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const char *v)
	{
		if ((v) && (*v)) {
			s_appendKey(out, k);
			while (*v)
				s_appendValueByte(out, (uint8_t)*(v++));
			out.push_back((uint8_t)'\n');
		}
	}

	/**
	 * Append a key=value pair to a buffer (vector or FCV)
	 * 
	 * @param out Buffer
	 * @param k Key (must be <= 8 characters)
	 * @param v Value
	 * @param vlen Value length in bytes
	 */
	template< typename V >
	ZT_MAYBE_UNUSED ZT_INLINE static void append(V &out, const char *const k, const void *const v, const unsigned int vlen)
	{
		s_appendKey(out, k);
		for (unsigned int i = 0; i < vlen; ++i)
			s_appendValueByte(out, reinterpret_cast<const uint8_t *>(v)[i]);
		out.push_back((uint8_t)'\n');
	}

	/**
	 * Append a packet ID as raw bytes in the provided byte order
	 * 
	 * @param out Buffer
	 * @param k Key (must be <= 8 characters)
	 * @param pid Packet ID
	 */
	template< typename V >
	ZT_MAYBE_UNUSED static ZT_INLINE void appendPacketId(V &out, const char *const k, const uint64_t pid)
	{ append(out, k, &pid, 8); }

	/**
	 * Append key=value with any object implementing the correct marshal interface
	 * 
	 * @param out Buffer
	 * @param k Key (must be <= 8 characters)
	 * @param v Marshal-able object
	 * @return Bytes appended or negative on error (return value of marshal())
	 */
	template< typename V, typename T >
	ZT_MAYBE_UNUSED static ZT_INLINE int appendObject(V &out, const char *const k, const T &v)
	{
		uint8_t tmp[2048]; // large enough for any current object
		if (T::marshalSizeMax() > sizeof(tmp))
			return -1;
		const int mlen = v.marshal(tmp);
		if (mlen > 0)
			append(out, k, tmp, (unsigned int)mlen);
		return mlen;
	}

	/**
	 * Append #sub where sub is a hexadecimal string to 'name' and store in 'buf'
	 *
	 * @param buf Buffer to store subscript key
	 * @param name Root name
	 * @param sub Subscript index
	 * @return Pointer to 'buf'
	 */
	ZT_MAYBE_UNUSED static char *arraySubscript(char *buf, unsigned int bufSize, const char *name, const unsigned long sub) noexcept;

private:
	template< typename V >
	ZT_INLINE static void s_appendValueByte(V &out, const uint8_t c)
	{
		switch (c) {
			case 0:
				out.push_back(92);
				out.push_back(48);
				break;
			case 10:
				out.push_back(92);
				out.push_back(110);
				break;
			case 13:
				out.push_back(92);
				out.push_back(114);
				break;
			case 61:
				out.push_back(92);
				out.push_back(101);
				break;
			case 92:
				out.push_back(92);
				out.push_back(92);
				break;
			default:
				out.push_back(c);
				break;
		}
	}

	template< typename V >
	ZT_INLINE static void s_appendKey(V &out, const char *k)
	{
		for (;;) {
			const char c = *(k++);
			if (c == 0)
				break;
			out.push_back((uint8_t)c);
		}
		out.push_back((uint8_t)'=');
	}

	// Dictionary maps need to be sorted so that they always encode in the same order
	// to yield blobs that can be hashed and signed reproducibly. Other than for areas
	// where dictionaries are signed and verified the order doesn't matter.
	SortedMap< String, Vector< uint8_t > > m_entries;
};

} // namespace ZeroTier

#endif
