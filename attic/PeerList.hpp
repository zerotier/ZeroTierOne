/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_PEERLIST_HPP
#define ZT_PEERLIST_HPP

#include "Constants.hpp"
#include "SharedPtr.hpp"
#include "Peer.hpp"

namespace ZeroTier {

/**
 * A list of peers
 *
 * This is a simple vector optimized for the case where there will almost always
 * be zero or one element. In that case it doesn't allocate. If there's more than
 * one element, it will grow to include all elements.
 *
 * It's used to return lookups in Topology where there will almost always be zero
 * or one peers returned but where there technically (but very rarely) can be more.
 */
class PeerList
{
public:
	ZT_INLINE PeerList() noexcept:
		m_onePeer(),
		m_peers(&m_onePeer),
		m_peerCount(0)
	{}

	ZT_INLINE PeerList(const PeerList &pl)
	{
		const unsigned int pc = pl.m_peerCount;
		if (likely(pc <= 1)) {
			m_onePeer = pl.m_onePeer;
			m_peers = &m_onePeer;
		} else {
			m_peers = new SharedPtr<Peer>[pc];
			for (unsigned int i = 0;i < pc;++i)
				m_peers[i] = pl.m_peers[i];
		}
		m_peerCount = pc;
	}

	ZT_INLINE ~PeerList()
	{
		if (unlikely(m_peers != &m_onePeer))
			delete[] m_peers;
	}

	ZT_INLINE PeerList &operator=(const PeerList &pl)
	{
		if (&pl != this) {
			if (unlikely(m_peers != &m_onePeer))
				delete[] m_peers;
			if (likely(pl.m_peerCount <= 1)) {
				m_onePeer = pl.m_onePeer;
				m_peers = &m_onePeer;
			} else {
				m_onePeer.zero();
				m_peers = new SharedPtr<Peer>[pl.m_peerCount];
				for (unsigned int i = 0;i < pl.m_peerCount;++i)
					m_peers[i] = pl.m_peers[i];
			}
			m_peerCount = pl.m_peerCount;
		}
		return *this;
	}

	/**
	 * Resize the peer list to store a given number of members
	 *
	 * To populate the list, this must be called first followed by each member
	 * being set with the [] operator. List content after this call is undefined
	 * and may contain old data if the object is being re-used.
	 *
	 * @param s New size of list
	 */
	ZT_INLINE void resize(const unsigned int s)
	{
		if (unlikely(m_peers != &m_onePeer))
			delete[] m_peers;
		m_peerCount = s;
		if (likely(s <= 1)) {
			m_peers = &m_onePeer;
		} else {
			m_peers = new SharedPtr<Peer>[s];
		}
	}

	ZT_INLINE SharedPtr <Peer> &operator[](const unsigned int i) noexcept
	{ return m_peers[i]; }

	ZT_INLINE const SharedPtr <Peer> &operator[](const unsigned int i) const noexcept
	{ return m_peers[i]; }

	ZT_INLINE unsigned int size() const noexcept
	{ return m_peerCount; }

private:
	SharedPtr <Peer> m_onePeer;
	SharedPtr <Peer> *m_peers;
	unsigned int m_peerCount;
};

} // namespace ZeroTier

#endif
