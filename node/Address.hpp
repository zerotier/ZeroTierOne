/*
 * Copyright (c)2019 ZeroTier, Inc.
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

#ifndef ZT_ADDRESS_HPP
#define ZT_ADDRESS_HPP

#include "Buffer.hpp"
#include "Constants.hpp"
#include "Utils.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace ZeroTier {

/**
 * A ZeroTier address
 */
class Address {
  public:
    Address() : _a(0)
    {
    }
    Address(const Address& a) : _a(a._a)
    {
    }
    Address(uint64_t a) : _a(a & 0xffffffffffULL)
    {
    }

    /**
     * @param bits Raw address -- 5 bytes, big-endian byte order
     * @param len Length of array
     */
    Address(const void* bits, unsigned int len)
    {
        setTo(bits, len);
    }

    inline Address& operator=(const Address& a)
    {
        _a = a._a;
        return *this;
    }
    inline Address& operator=(const uint64_t a)
    {
        _a = (a & 0xffffffffffULL);
        return *this;
    }

    /**
     * @param bits Raw address -- 5 bytes, big-endian byte order
     * @param len Length of array
     */
    inline void setTo(const void* bits, const unsigned int len)
    {
        if (len < ZT_ADDRESS_LENGTH) {
            _a = 0;
            return;
        }
        const unsigned char* b = (const unsigned char*)bits;
        uint64_t a = ((uint64_t)*b++) << 32;
        a |= ((uint64_t)*b++) << 24;
        a |= ((uint64_t)*b++) << 16;
        a |= ((uint64_t)*b++) << 8;
        a |= ((uint64_t)*b);
        _a = a;
    }

    /**
     * @param bits Buffer to hold 5-byte address in big-endian byte order
     * @param len Length of array
     */
    inline void copyTo(void* const bits, const unsigned int len) const
    {
        if (len < ZT_ADDRESS_LENGTH) {
            return;
        }
        unsigned char* b = (unsigned char*)bits;
        *(b++) = (unsigned char)((_a >> 32) & 0xff);
        *(b++) = (unsigned char)((_a >> 24) & 0xff);
        *(b++) = (unsigned char)((_a >> 16) & 0xff);
        *(b++) = (unsigned char)((_a >> 8) & 0xff);
        *b = (unsigned char)(_a & 0xff);
    }

    /**
     * Append to a buffer in big-endian byte order
     *
     * @param b Buffer to append to
     */
    template <unsigned int C> inline void appendTo(Buffer<C>& b) const
    {
        unsigned char* p = (unsigned char*)b.appendField(ZT_ADDRESS_LENGTH);
        *(p++) = (unsigned char)((_a >> 32) & 0xff);
        *(p++) = (unsigned char)((_a >> 24) & 0xff);
        *(p++) = (unsigned char)((_a >> 16) & 0xff);
        *(p++) = (unsigned char)((_a >> 8) & 0xff);
        *p = (unsigned char)(_a & 0xff);
    }

    /**
     * @return Integer containing address (0 to 2^40)
     */
    inline uint64_t toInt() const
    {
        return _a;
    }

    /**
     * @return Hash code for use with Hashtable
     */
    inline unsigned long hashCode() const
    {
        return (unsigned long)_a;
    }

    /**
     * @return Hexadecimal string
     */
    inline char* toString(char buf[11]) const
    {
        return Utils::hex10(_a, buf);
    }

    /**
     * @return True if this address is not zero
     */
    inline operator bool() const
    {
        return (_a != 0);
    }

    /**
     * Check if this address is reserved
     *
     * The all-zero null address and any address beginning with 0xff are
     * reserved. (0xff is reserved for future use to designate possibly
     * longer addresses, addresses based on IPv6 innards, etc.)
     *
     * @return True if address is reserved and may not be used
     */
    inline bool isReserved() const
    {
        return ((! _a) || ((_a >> 32) == ZT_ADDRESS_RESERVED_PREFIX));
    }

    /**
     * @param i Value from 0 to 4 (inclusive)
     * @return Byte at said position (address interpreted in big-endian order)
     */
    inline uint8_t operator[](unsigned int i) const
    {
        return (uint8_t)(_a >> (32 - (i * 8)));
    }

    inline void zero()
    {
        _a = 0;
    }

    inline bool operator==(const uint64_t& a) const
    {
        return (_a == (a & 0xffffffffffULL));
    }
    inline bool operator!=(const uint64_t& a) const
    {
        return (_a != (a & 0xffffffffffULL));
    }
    inline bool operator>(const uint64_t& a) const
    {
        return (_a > (a & 0xffffffffffULL));
    }
    inline bool operator<(const uint64_t& a) const
    {
        return (_a < (a & 0xffffffffffULL));
    }
    inline bool operator>=(const uint64_t& a) const
    {
        return (_a >= (a & 0xffffffffffULL));
    }
    inline bool operator<=(const uint64_t& a) const
    {
        return (_a <= (a & 0xffffffffffULL));
    }

    inline bool operator==(const Address& a) const
    {
        return (_a == a._a);
    }
    inline bool operator!=(const Address& a) const
    {
        return (_a != a._a);
    }
    inline bool operator>(const Address& a) const
    {
        return (_a > a._a);
    }
    inline bool operator<(const Address& a) const
    {
        return (_a < a._a);
    }
    inline bool operator>=(const Address& a) const
    {
        return (_a >= a._a);
    }
    inline bool operator<=(const Address& a) const
    {
        return (_a <= a._a);
    }

  private:
    uint64_t _a;
};

}   // namespace ZeroTier

#endif
