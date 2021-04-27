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

#ifndef ZT_LOCATOR_HPP
#define ZT_LOCATOR_HPP

#include "Constants.hpp"
#include "Containers.hpp"
#include "Dictionary.hpp"
#include "Endpoint.hpp"
#include "FCV.hpp"
#include "Identity.hpp"
#include "SharedPtr.hpp"
#include "TriviallyCopyable.hpp"

/**
 * Maximum size of endpoint attributes dictionary plus one byte for size.
 *
 * This cannot be (easily) changed.
 */
#define ZT_LOCATOR_MAX_ENDPOINT_ATTRIBUTES_SIZE 256

/**
 * Maximum number of endpoints, which can be increased.
 */
#define ZT_LOCATOR_MAX_ENDPOINTS 16

#define ZT_LOCATOR_MARSHAL_SIZE_MAX                                                                                                                            \
    (8 + ZT_ADDRESS_LENGTH + 2 + (ZT_LOCATOR_MAX_ENDPOINTS * (ZT_ENDPOINT_MARSHAL_SIZE_MAX + ZT_LOCATOR_MAX_ENDPOINT_ATTRIBUTES_SIZE)) + 2 + 2                 \
     + ZT_SIGNATURE_BUFFER_SIZE)

/**
 * Maximum size of a string format Locator (this is way larger than needed)
 */
#define ZT_LOCATOR_STRING_SIZE_MAX 16384

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 *
 * A locator contains long-lived endpoints for a node such as IP/port pairs,
 * URLs, or other nodes, and is signed by the node it describes.
 */
class Locator {
    friend class SharedPtr<Locator>;
    friend class SharedPtr<const Locator>;

  public:
    /**
     * Attributes of an endpoint in this locator
     *
     * This is specified for future use, but there are currently no attributes
     * defined. A Dictionary is used for serialization for extensibility.
     */
    struct EndpointAttributes {
        friend class SharedPtr<Locator::EndpointAttributes>;
        friend class SharedPtr<const Locator::EndpointAttributes>;

        /**
         * Default endpoint attributes
         */
        static const SharedPtr<const Locator::EndpointAttributes> DEFAULT;

        /**
         * Raw attributes data in the form of a dictionary prefixed by its size.
         *
         * The maximum size of attributes is 255, which is more than enough for
         * tiny things like bandwidth and priority.
         */
        uint8_t data[ZT_LOCATOR_MAX_ENDPOINT_ATTRIBUTES_SIZE];

        ZT_INLINE EndpointAttributes() noexcept
        {
            Utils::zero<ZT_LOCATOR_MAX_ENDPOINT_ATTRIBUTES_SIZE>(data);
        }

        ZT_INLINE bool operator==(const EndpointAttributes& a) const noexcept
        {
            return ((data[0] == a.data[0]) && (memcmp(data, a.data, data[0]) == 0));
        }

        ZT_INLINE bool operator<(const EndpointAttributes& a) const noexcept
        {
            return ((data[0] < a.data[0]) || ((data[0] == a.data[0]) && (memcmp(data, a.data, data[0]) < 0)));
        }

        ZT_INLINE bool operator!=(const EndpointAttributes& a) const noexcept
        {
            return ! (*this == a);
        }

        ZT_INLINE bool operator>(const EndpointAttributes& a) const noexcept
        {
            return (a < *this);
        }

        ZT_INLINE bool operator<=(const EndpointAttributes& a) const noexcept
        {
            return ! (a < *this);
        }

        ZT_INLINE bool operator>=(const EndpointAttributes& a) const noexcept
        {
            return ! (*this < a);
        }

      private:
        std::atomic<int> __refCount;
    };

    ZT_INLINE Locator() noexcept : m_revision(0)
    {
    }

    ZT_INLINE Locator(const Locator& l) noexcept
        : m_revision(l.m_revision)
        , m_signer(l.m_signer)
        , m_endpoints(l.m_endpoints)
        , m_signature(l.m_signature)
        , __refCount(0)
    {
    }

    explicit Locator(const char* const str) noexcept;

    /**
     * @return Timestamp (a.k.a. revision number) set by Location signer
     */
    ZT_INLINE int64_t revision() const noexcept
    {
        return m_revision;
    }

    /**
     * @return ZeroTier address of signer
     */
    ZT_INLINE Address signer() const noexcept
    {
        return m_signer;
    }

    /**
     * @return Endpoints specified in locator
     */
    ZT_INLINE const Vector<std::pair<Endpoint, SharedPtr<const EndpointAttributes> > >& endpoints() const noexcept
    {
        return m_endpoints;
    }

    /**
     * @return Signature data
     */
    ZT_INLINE const FCV<uint8_t, ZT_SIGNATURE_BUFFER_SIZE>& signature() const noexcept
    {
        return m_signature;
    }

    /**
     * Add an endpoint to this locator
     *
     * This doesn't check for the presence of the endpoint, so take
     * care not to add duplicates.
     *
     * @param ep Endpoint to add
     * @param a Endpoint attributes or NULL to use default
     * @return True if endpoint was added (or already present), false if locator is full
     */
    bool add(const Endpoint& ep, const SharedPtr<const EndpointAttributes>& a);

    /**
     * Sign this locator
     *
     * This sets timestamp, sorts endpoints so that the same set of endpoints
     * will always produce the same locator, and signs.
     *
     * @param id Identity that includes private key
     * @return True if signature successful
     */
    bool sign(int64_t rev, const Identity& id) noexcept;

    /**
     * Verify this Locator's validity and signature
     *
     * @param id Identity corresponding to hash
     * @return True if valid and signature checks out
     */
    bool verify(const Identity& id) const noexcept;

    /**
     * Convert this locator to a string
     *
     * @param s String buffer
     * @return Pointer to buffer
     */
    char* toString(char s[ZT_LOCATOR_STRING_SIZE_MAX]) const noexcept;

    ZT_INLINE String toString() const
    {
        char tmp[ZT_LOCATOR_STRING_SIZE_MAX];
        return String(toString(tmp));
    }

    /**
     * Decode a string format locator
     *
     * @param s Locator from toString()
     * @return True if format was valid
     */
    bool fromString(const char* s) noexcept;

    explicit ZT_INLINE operator bool() const noexcept
    {
        return m_revision > 0;
    }

    static constexpr int marshalSizeMax() noexcept
    {
        return ZT_LOCATOR_MARSHAL_SIZE_MAX;
    }

    int marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX], bool excludeSignature = false) const noexcept;
    int unmarshal(const uint8_t* data, int len) noexcept;

    ZT_INLINE bool operator==(const Locator& l) const noexcept
    {
        const unsigned long es = (unsigned long)m_endpoints.size();
        if ((m_revision == l.m_revision) && (m_signer == l.m_signer) && (es == (unsigned long)l.m_endpoints.size()) && (m_signature == l.m_signature)) {
            for (unsigned long i = 0; i < es; ++i) {
                if (m_endpoints[i].first != l.m_endpoints[i].first)
                    return false;
                if (! m_endpoints[i].second) {
                    if (l.m_endpoints[i].second)
                        return false;
                }
                else {
                    if ((! l.m_endpoints[i].second) || (*(m_endpoints[i].second) != *(l.m_endpoints[i].second)))
                        return false;
                }
            }
            return true;
        }
        return false;
    }

    ZT_INLINE bool operator!=(const Locator& l) const noexcept
    {
        return ! (*this == l);
    }

  private:
    void m_sortEndpoints() noexcept;

    int64_t m_revision;
    Address m_signer;
    Vector<std::pair<Endpoint, SharedPtr<const EndpointAttributes> > > m_endpoints;
    FCV<uint8_t, ZT_SIGNATURE_BUFFER_SIZE> m_signature;
    std::atomic<int> __refCount;
};

}   // namespace ZeroTier

#endif
