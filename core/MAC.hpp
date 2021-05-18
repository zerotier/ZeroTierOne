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

#ifndef ZT_MAC_HPP
#define ZT_MAC_HPP

#include "Address.hpp"
#include "Constants.hpp"
#include "Containers.hpp"
#include "TriviallyCopyable.hpp"
#include "Utils.hpp"

namespace ZeroTier {

/**
 * 48-byte Ethernet MAC address
 */
class MAC : public TriviallyCopyable {
  public:
    ZT_INLINE MAC() noexcept : m_mac(0ULL) {}

    ZT_INLINE
    MAC(const uint8_t a, const uint8_t b, const uint8_t c, const uint8_t d, const uint8_t e, const uint8_t f) noexcept
        : m_mac(
            (((uint64_t)a) << 40U) | (((uint64_t)b) << 32U) | (((uint64_t)c) << 24U) | (((uint64_t)d) << 16U)
            | (((uint64_t)e) << 8U) | ((uint64_t)f))
    {
    }

    explicit ZT_INLINE MAC(const uint64_t m) noexcept : m_mac(m) {}

    explicit ZT_INLINE MAC(const uint8_t b[6]) noexcept { setTo(b); }

    ZT_INLINE MAC(const Address &ztaddr, const uint64_t nwid) noexcept { fromAddress(ztaddr, nwid); }

    /**
     * @return MAC in 64-bit integer
     */
    ZT_INLINE uint64_t toInt() const noexcept { return m_mac; }

    /**
     * Set MAC to zero
     */
    ZT_INLINE void zero() noexcept { m_mac = 0ULL; }

    /**
     * @param bits Raw MAC in big-endian byte order
     * @param len Length, must be >= 6 or result is zero
     */
    ZT_INLINE void setTo(const uint8_t b[6]) noexcept
    {
        m_mac = ((uint64_t)b[0] << 40U) | ((uint64_t)b[1] << 32U) | ((uint64_t)b[2] << 24U) | ((uint64_t)b[3] << 16U)
                | ((uint64_t)b[4] << 8U) | (uint64_t)b[5];
    }

    /**
     * @param buf Destination buffer for MAC in big-endian byte order
     * @param len Length of buffer, must be >= 6 or nothing is copied
     */
    ZT_INLINE void copyTo(uint8_t b[6]) const noexcept
    {
        b[0] = (uint8_t)(m_mac >> 40U);
        b[1] = (uint8_t)(m_mac >> 32U);
        b[2] = (uint8_t)(m_mac >> 24U);
        b[3] = (uint8_t)(m_mac >> 16U);
        b[4] = (uint8_t)(m_mac >> 8U);
        b[5] = (uint8_t)m_mac;
    }

    /**
     * @return True if this is broadcast (all 0xff)
     */
    ZT_INLINE bool isBroadcast() const noexcept { return m_mac; }

    /**
     * @return True if this is a multicast MAC
     */
    ZT_INLINE bool isMulticast() const noexcept { return ((m_mac & 0x010000000000ULL) != 0ULL); }

    /**
     * Set this MAC to a MAC derived from an address and a network ID
     *
     * @param ztaddr ZeroTier address
     * @param nwid 64-bit network ID
     */
    ZT_INLINE void fromAddress(const Address &ztaddr, uint64_t nwid) noexcept
    {
        uint64_t m = ((uint64_t)firstOctetForNetwork(nwid)) << 40U;
        m |= ztaddr.toInt();   // a is 40 bits
        m ^= ((nwid >> 8U) & 0xffU) << 32U;
        m ^= ((nwid >> 16U) & 0xffU) << 24U;
        m ^= ((nwid >> 24U) & 0xffU) << 16U;
        m ^= ((nwid >> 32U) & 0xffU) << 8U;
        m ^= (nwid >> 40U) & 0xffU;
        m_mac = m;
    }

    /**
     * Get the ZeroTier address for this MAC on this network (assuming no bridging of course, basic unicast)
     *
     * This just XORs the next-lest-significant 5 bytes of the network ID again to unmask.
     *
     * @param nwid Network ID
     */
    ZT_INLINE Address toAddress(uint64_t nwid) const noexcept
    {
        uint64_t a = m_mac & 0xffffffffffULL;   // least significant 40 bits of MAC are formed from address
        a ^= ((nwid >> 8U) & 0xffU)
             << 32U;   // ... XORed with bits 8-48 of the nwid in little-endian byte order, so unmask it
        a ^= ((nwid >> 16U) & 0xffU) << 24U;
        a ^= ((nwid >> 24U) & 0xffU) << 16U;
        a ^= ((nwid >> 32U) & 0xffU) << 8U;
        a ^= (nwid >> 40U) & 0xffU;
        return Address(a);
    }

    /**
     * @param nwid Network ID
     * @return First octet of MAC for this network
     */
    static ZT_INLINE unsigned char firstOctetForNetwork(uint64_t nwid) noexcept
    {
        const uint8_t a =
            ((uint8_t)(nwid & 0xfeU) | 0x02U);   // locally administered, not multicast, from LSB of network ID
        return ((a == 0x52) ? 0x32 : a);         // blacklist 0x52 since it's used by KVM, libvirt, and other popular
                                                 // virtualization engines... seems de-facto standard on Linux
    }

    /**
     * @param i Value from 0 to 5 (inclusive)
     * @return Byte at said position (address interpreted in big-endian order)
     */
    ZT_INLINE uint8_t operator[](unsigned int i) const noexcept
    {
        return (uint8_t)(m_mac >> (unsigned int)(40 - (i * 8)));
    }

    /**
     * @return 6, which is the number of bytes in a MAC, for container compliance
     */
    ZT_INLINE unsigned int size() const noexcept { return 6; }

    ZT_INLINE unsigned long hashCode() const noexcept { return (unsigned long)Utils::hash64(m_mac); }

    ZT_INLINE operator bool() const noexcept { return (m_mac != 0ULL); }

    ZT_INLINE operator uint64_t() const noexcept { return m_mac; }

    /**
     * Convert this MAC to a standard format colon-separated hex string
     *
     * @param buf Buffer to store string
     * @return Pointer to buf
     */
    ZT_INLINE char *toString(char buf[18]) const noexcept
    {
        buf[0]  = Utils::HEXCHARS[(m_mac >> 44U) & 0xfU];
        buf[1]  = Utils::HEXCHARS[(m_mac >> 40U) & 0xfU];
        buf[2]  = ':';
        buf[3]  = Utils::HEXCHARS[(m_mac >> 36U) & 0xfU];
        buf[4]  = Utils::HEXCHARS[(m_mac >> 32U) & 0xfU];
        buf[5]  = ':';
        buf[6]  = Utils::HEXCHARS[(m_mac >> 28U) & 0xfU];
        buf[7]  = Utils::HEXCHARS[(m_mac >> 24U) & 0xfU];
        buf[8]  = ':';
        buf[9]  = Utils::HEXCHARS[(m_mac >> 20U) & 0xfU];
        buf[10] = Utils::HEXCHARS[(m_mac >> 16U) & 0xfU];
        buf[11] = ':';
        buf[12] = Utils::HEXCHARS[(m_mac >> 12U) & 0xfU];
        buf[13] = Utils::HEXCHARS[(m_mac >> 8U) & 0xfU];
        buf[14] = ':';
        buf[15] = Utils::HEXCHARS[(m_mac >> 4U) & 0xfU];
        buf[16] = Utils::HEXCHARS[m_mac & 0xfU];
        buf[17] = (char)0;
        return buf;
    }

    ZT_INLINE String toString() const
    {
        char tmp[18];
        return String(toString(tmp));
    }

    /**
     * Parse a MAC address in hex format with or without : separators and ignoring non-hex characters.
     *
     * @param s String to parse
     */
    ZT_INLINE void fromString(const char *s) noexcept
    {
        m_mac = 0;
        if (s) {
            while (*s) {
                uint64_t c;
                const char hc = *s++;
                if ((hc >= 48) && (hc <= 57))
                    c = (uint64_t)hc - 48;
                else if ((hc >= 97) && (hc <= 102))
                    c = (uint64_t)hc - 87;
                else if ((hc >= 65) && (hc <= 70))
                    c = (uint64_t)hc - 55;
                else
                    continue;
                m_mac = (m_mac << 4U) | c;
            }
            m_mac &= 0xffffffffffffULL;
        }
    }

    ZT_INLINE MAC &operator=(const uint64_t m) noexcept
    {
        m_mac = m;
        return *this;
    }

    ZT_INLINE bool operator==(const MAC &m) const noexcept { return (m_mac == m.m_mac); }

    ZT_INLINE bool operator!=(const MAC &m) const noexcept { return (m_mac != m.m_mac); }

    ZT_INLINE bool operator<(const MAC &m) const noexcept { return (m_mac < m.m_mac); }

    ZT_INLINE bool operator<=(const MAC &m) const noexcept { return (m_mac <= m.m_mac); }

    ZT_INLINE bool operator>(const MAC &m) const noexcept { return (m_mac > m.m_mac); }

    ZT_INLINE bool operator>=(const MAC &m) const noexcept { return (m_mac >= m.m_mac); }

    ZT_INLINE bool operator==(const uint64_t m) const noexcept { return (m_mac == m); }

    ZT_INLINE bool operator!=(const uint64_t m) const noexcept { return (m_mac != m); }

    ZT_INLINE bool operator<(const uint64_t m) const noexcept { return (m_mac < m); }

    ZT_INLINE bool operator<=(const uint64_t m) const noexcept { return (m_mac <= m); }

    ZT_INLINE bool operator>(const uint64_t m) const noexcept { return (m_mac > m); }

    ZT_INLINE bool operator>=(const uint64_t m) const noexcept { return (m_mac >= m); }

  private:
    uint64_t m_mac;
};

static_assert(sizeof(MAC) == sizeof(uint64_t), "MAC contains unnecessary padding");

}   // namespace ZeroTier

#endif
