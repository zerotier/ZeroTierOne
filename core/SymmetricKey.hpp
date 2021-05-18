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

#ifndef ZT_SYMMETRICKEY_HPP
#define ZT_SYMMETRICKEY_HPP

#include "AES.hpp"
#include "Address.hpp"
#include "Constants.hpp"
#include "Utils.hpp"

namespace ZeroTier {

/**
 * Container for symmetric keys and ciphers initialized with them.
 */
class SymmetricKey {
  public:
    /**
     * Construct an uninitialized key (init() must be called)
     */
    ZT_INLINE SymmetricKey() : m_secret(), m_ts(-1), m_initialNonce(0), m_cipher(), m_nonce(0) {}

    /**
     * Construct a new symmetric key
     *
     * SECURITY: the MSB of the nonce is always 0 because this bit is set to 0
     * or 1 depending on which "direction" data is moving. See nextMessage().
     *
     * @param ts Key timestamp
     * @param key Key (must be 48 bytes / 384 bits)
     */
    ZT_INLINE SymmetricKey(const int64_t ts, const void *const key) noexcept
        : m_secret(key)
        , m_ts(ts)
        , m_initialNonce(Utils::getSecureRandomU64() >> 1U)
        , m_cipher(key)
        , m_nonce(m_initialNonce)
    {
    }

    ZT_INLINE SymmetricKey(const SymmetricKey &k) noexcept
        : m_secret(k.m_secret)
        , m_ts(k.m_ts)
        , m_initialNonce(k.m_initialNonce)
        , m_cipher(k.m_secret.data)
        , m_nonce(k.m_nonce.load(std::memory_order_relaxed))
    {
    }

    ZT_INLINE ~SymmetricKey() noexcept { Utils::burn(m_secret.data, ZT_SYMMETRIC_KEY_SIZE); }

    ZT_INLINE SymmetricKey &operator=(const SymmetricKey &k) noexcept
    {
        m_secret       = k.m_secret;
        m_ts           = k.m_ts;
        m_initialNonce = k.m_initialNonce;
        m_cipher.init(k.m_secret.data);
        m_nonce.store(k.m_nonce.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }

    /**
     * Initialize or re-initialize a symmetric key
     *
     * @param ts Key timestamp
     * @param key Key (must be 48 bytes / 384 bits)
     */
    ZT_INLINE void init(const int64_t ts, const void *const key) noexcept
    {
        Utils::copy<ZT_SYMMETRIC_KEY_SIZE>(m_secret.data, key);
        m_ts           = ts;
        m_initialNonce = Utils::getSecureRandomU64() >> 1U;
        m_cipher.init(key);
        m_nonce.store(m_initialNonce, std::memory_order_relaxed);
    }

    /**
     * Advance usage counter by one and return the next IV / packet ID.
     *
     * @param sender Sending ZeroTier address
     * @param receiver Receiving ZeroTier address
     * @return Next unique IV for next message
     */
    ZT_INLINE uint64_t nextMessage(const Address sender, const Address receiver) noexcept
    {
        return m_nonce.fetch_add(1, std::memory_order_relaxed) ^ (((uint64_t)(sender > receiver)) << 63U);
    }

    /**
     * Get the number of times this key has been used.
     *
     * This is used along with the key's initial timestamp to determine key age
     * for ephemeral key rotation.
     *
     * @return Number of times nextMessage() has been called since object creation
     */
    ZT_INLINE uint64_t odometer() const noexcept { return m_nonce.load(std::memory_order_relaxed) - m_initialNonce; }

    /**
     * @return Key creation timestamp or -1 if this is a long-lived key
     */
    ZT_INLINE int64_t timestamp() const noexcept { return m_ts; }

    /**
     * @return 48-byte / 384-bit secret key
     */
    ZT_INLINE const uint8_t *key() const noexcept { return m_secret.data; }

    /**
     * @return AES cipher (already initialized with secret key)
     */
    ZT_INLINE const AES &aes() const noexcept { return m_cipher; }

  private:
    Blob<ZT_SYMMETRIC_KEY_SIZE> m_secret;
    int64_t m_ts;
    uint64_t m_initialNonce;
    AES m_cipher;
    std::atomic<uint64_t> m_nonce;
};

}   // namespace ZeroTier

#endif
