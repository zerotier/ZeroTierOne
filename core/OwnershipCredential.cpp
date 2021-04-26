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

#include "OwnershipCredential.hpp"

namespace ZeroTier {

void OwnershipCredential::addThing(const InetAddress& ip)
{
    if (m_thingCount >= ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS)
        return;
    if (ip.as.sa.sa_family == AF_INET) {
        m_thingTypes[m_thingCount] = THING_IPV4_ADDRESS;
        Utils::copy<4>(
            m_thingValues[m_thingCount],
            &(reinterpret_cast<const struct sockaddr_in*>(&ip)->sin_addr.s_addr));
        ++m_thingCount;
    }
    else if (ip.as.sa.sa_family == AF_INET6) {
        m_thingTypes[m_thingCount] = THING_IPV6_ADDRESS;
        Utils::copy<16>(
            m_thingValues[m_thingCount],
            reinterpret_cast<const struct sockaddr_in6*>(&ip)->sin6_addr.s6_addr);
        ++m_thingCount;
    }
}

void OwnershipCredential::addThing(const MAC& mac)
{
    if (m_thingCount >= ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS)
        return;
    m_thingTypes[m_thingCount] = THING_MAC_ADDRESS;
    mac.copyTo(m_thingValues[m_thingCount]);
    ++m_thingCount;
}

bool OwnershipCredential::sign(const Identity& signer)
{
    uint8_t buf[ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX + 16];
    if (signer.hasPrivate()) {
        m_signedBy = signer.address();
        m_signatureLength = signer.sign(buf, (unsigned int)marshal(buf, true), m_signature, sizeof(m_signature));
        return true;
    }
    return false;
}

int OwnershipCredential::marshal(uint8_t data[ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX], bool forSign) const noexcept
{
    int p = 0;
    if (forSign) {
        for (int k = 0; k < 16; ++k)
            data[p++] = 0x7f;
    }
    Utils::storeBigEndian<uint64_t>(data + p, m_networkId);
    Utils::storeBigEndian<uint64_t>(data + p + 8, (uint64_t)m_ts);
    Utils::storeBigEndian<uint64_t>(data + p + 16, m_flags);
    Utils::storeBigEndian<uint32_t>(data + p + 24, m_id);
    Utils::storeBigEndian<uint16_t>(data + p + 28, (uint16_t)m_thingCount);
    p += 30;
    for (unsigned int i = 0, j = m_thingCount; i < j; ++i) {
        data[p++] = m_thingTypes[i];
        Utils::copy<ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE>(data + p, m_thingValues[i]);
        p += ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE;
    }
    m_issuedTo.copyTo(data + p);
    p += ZT_ADDRESS_LENGTH;
    m_signedBy.copyTo(data + p);
    p += ZT_ADDRESS_LENGTH;
    if (! forSign) {
        data[p++] = 1;
        Utils::storeBigEndian<uint16_t>(data + p, (uint16_t)m_signatureLength);
        p += 2;
        Utils::copy(data + p, m_signature, m_signatureLength);
        p += (int)m_signatureLength;
    }
    data[p++] = 0;
    data[p++] = 0;
    if (forSign) {
        for (int k = 0; k < 16; ++k)
            data[p++] = 0x7f;
    }
    return p;
}

int OwnershipCredential::unmarshal(const uint8_t* data, int len) noexcept
{
    if (len < 30)
        return -1;

    m_networkId = Utils::loadBigEndian<uint64_t>(data);
    m_ts = (int64_t)Utils::loadBigEndian<uint64_t>(data + 8);
    m_flags = Utils::loadBigEndian<uint64_t>(data + 16);
    m_id = Utils::loadBigEndian<uint32_t>(data + 24);
    m_thingCount = Utils::loadBigEndian<uint16_t>(data + 28);
    if (m_thingCount > ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS)
        return -1;
    int p = 30;

    for (unsigned int i = 0, j = m_thingCount; i < j; ++i) {
        if ((p + 1 + ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE) > len)
            return -1;
        m_thingTypes[i] = data[p++];
        Utils::copy<ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE>(m_thingValues[i], data + p);
        p += ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE;
    }

    if ((p + ZT_ADDRESS_LENGTH + ZT_ADDRESS_LENGTH + 1 + 2) > len)
        return -1;
    m_issuedTo.setTo(data + p);
    p += ZT_ADDRESS_LENGTH;
    m_signedBy.setTo(data + p);
    p += ZT_ADDRESS_LENGTH + 1;

    p += 2 + Utils::loadBigEndian<uint16_t>(data + p);
    if (p > len)
        return -1;
    return p;
}

}   // namespace ZeroTier
