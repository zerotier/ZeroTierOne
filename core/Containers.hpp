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

#ifndef ZT_CONTAINERS_HPP
#define ZT_CONTAINERS_HPP

/* This defines a Map, SortedMap, Vector, etc. based on STL templates. */

#include "Constants.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef __CPP11__

#include <atomic>
#include <forward_list>
#include <unordered_map>

#endif

namespace ZeroTier {

template <typename V> class Vector : public std::vector<V> {
  public:
    ZT_INLINE Vector() : std::vector<V>()
    {
    }

    template <typename I> ZT_INLINE Vector(I begin, I end) : std::vector<V>(begin, end)
    {
    }
};

template <typename V> class List : public std::list<V> {
};

#ifdef __CPP11__

struct intl_MapHasher {
    template <typename O> std::size_t operator()(const O& obj) const noexcept
    {
        return (std::size_t)obj.hashCode();
    }

    std::size_t operator()(const Vector<uint8_t>& bytes) const noexcept
    {
        return (std::size_t)Utils::fnv1a32(bytes.data(), (unsigned int)bytes.size());
    }

    std::size_t operator()(const uint64_t i) const noexcept
    {
        return (std::size_t)Utils::hash64(i ^ Utils::s_mapNonce);
    }

    std::size_t operator()(const int64_t i) const noexcept
    {
        return (std::size_t)Utils::hash64((uint64_t)i ^ Utils::s_mapNonce);
    }

    std::size_t operator()(const uint32_t i) const noexcept
    {
        return (std::size_t)Utils::hash32(i ^ (uint32_t)Utils::s_mapNonce);
    }

    std::size_t operator()(const int32_t i) const noexcept
    {
        return (std::size_t)Utils::hash32((uint32_t)i ^ (uint32_t)Utils::s_mapNonce);
    }
};

template <typename K, typename V> class Map : public std::unordered_map<K, V, intl_MapHasher> {
};

template <typename K, typename V> class MultiMap : public std::unordered_multimap<K, V, intl_MapHasher, std::equal_to<K> > {
};

#else

template <typename K, typename V> class Map : public std::map<K, V> {
};

template <typename K, typename V> class MultiMap : public std::multimap<K, V> {
};

#endif

template <typename K, typename V> class SortedMap : public std::map<K, V> {
};

#ifdef __CPP11__

template <typename V> class ForwardList : public std::forward_list<V> {
};

#else

template <typename V> class ForwardList : public std::list<V> {
};

#endif

template <typename V> class Set : public std::set<V, std::less<V> > {
};

typedef std::string String;

/**
 * A 384-bit hash
 */
struct H384 {
    uint64_t data[6];

    ZT_INLINE H384() noexcept
    {
        Utils::zero<sizeof(data)>(data);
    }

    ZT_INLINE H384(const H384& b) noexcept
    {
        Utils::copy<48>(data, b.data);
    }

    explicit ZT_INLINE H384(const void* const d) noexcept
    {
        Utils::copy<48>(data, d);
    }

    ZT_INLINE H384& operator=(const H384& b) noexcept
    {
        Utils::copy<48>(data, b.data);
        return *this;
    }

    ZT_INLINE unsigned long hashCode() const noexcept
    {
        return (unsigned long)data[0];
    }

    ZT_INLINE operator bool() const noexcept
    {
        return ((data[0] != 0) && (data[1] != 0) && (data[2] != 0) && (data[3] != 0) && (data[4] != 0) && (data[5] != 0));
    }

    ZT_INLINE bool operator==(const H384& b) const noexcept
    {
        return ((data[0] == b.data[0]) && (data[1] == b.data[1]) && (data[2] == b.data[2]) && (data[3] == b.data[3]) && (data[4] == b.data[4]) && (data[5] == b.data[5]));
    }

    ZT_INLINE bool operator!=(const H384& b) const noexcept
    {
        return ! (*this == b);
    }

    ZT_INLINE bool operator<(const H384& b) const noexcept
    {
        return std::lexicographical_compare(data, data + 6, b.data, b.data + 6);
    }

    ZT_INLINE bool operator<=(const H384& b) const noexcept
    {
        return ! (b < *this);
    }

    ZT_INLINE bool operator>(const H384& b) const noexcept
    {
        return (b < *this);
    }

    ZT_INLINE bool operator>=(const H384& b) const noexcept
    {
        return ! (*this < b);
    }
};

static_assert(sizeof(H384) == 48, "H384 contains unnecessary padding");

/**
 * A fixed size byte array
 *
 * @tparam S Size in bytes
 */
template <unsigned long S> struct Blob {
    uint8_t data[S];

    ZT_INLINE Blob() noexcept
    {
        Utils::zero<S>(data);
    }

    ZT_INLINE Blob(const Blob& b) noexcept
    {
        Utils::copy<S>(data, b.data);
    }

    explicit ZT_INLINE Blob(const void* const d) noexcept
    {
        Utils::copy<S>(data, d);
    }

    explicit ZT_INLINE Blob(const void* const d, const unsigned int l) noexcept
    {
        Utils::copy(data, d, (l > (unsigned int)S) ? (unsigned int)S : l);
        if (l < S) {
            Utils::zero(data + l, S - l);
        }
    }

    ZT_INLINE Blob& operator=(const Blob& b) noexcept
    {
        Utils::copy<S>(data, b.data);
        return *this;
    }

    ZT_INLINE unsigned long hashCode() const noexcept
    {
        return Utils::fnv1a32(data, (unsigned int)S);
    }

    ZT_INLINE operator bool() const noexcept
    {
        return Utils::allZero(data, (unsigned int)S);
    }

    ZT_INLINE bool operator==(const Blob& b) const noexcept
    {
        return (memcmp(data, b.data, S) == 0);
    }

    ZT_INLINE bool operator!=(const Blob& b) const noexcept
    {
        return (memcmp(data, b.data, S) != 0);
    }

    ZT_INLINE bool operator<(const Blob& b) const noexcept
    {
        return (memcmp(data, b.data, S) < 0);
    }

    ZT_INLINE bool operator<=(const Blob& b) const noexcept
    {
        return (memcmp(data, b.data, S) <= 0);
    }

    ZT_INLINE bool operator>(const Blob& b) const noexcept
    {
        return (memcmp(data, b.data, S) > 0);
    }

    ZT_INLINE bool operator>=(const Blob& b) const noexcept
    {
        return (memcmp(data, b.data, S) >= 0);
    }
};

}   // namespace ZeroTier

#endif
