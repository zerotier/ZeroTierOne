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

#ifndef ZT_TOPOLOGY_HPP
#define ZT_TOPOLOGY_HPP

#include "Address.hpp"
#include "CallContext.hpp"
#include "Certificate.hpp"
#include "Constants.hpp"
#include "Containers.hpp"
#include "FCV.hpp"
#include "Fingerprint.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"
#include "Path.hpp"
#include "Peer.hpp"
#include "ScopedPtr.hpp"
#include "SharedPtr.hpp"
#include "Spinlock.hpp"

namespace ZeroTier {

class Context;

/**
 * Database of network topology
 */
class Topology {
  public:
    Topology(const Context& ctx, const CallContext& cc);

    /**
     * Add peer to database
     *
     * If there's already a peer with this address, the existing peer is
     * returned. Otherwise the new peer is added and returned.
     *
     * @param peer Peer to add
     * @return New or existing peer
     */
    SharedPtr<Peer> add(const CallContext& cc, const SharedPtr<Peer>& peer);

    /**
     * Get a peer from its address
     *
     * @param zta ZeroTier address of peer
     * @param loadFromCached If false do not load from cache if not in memory (default: true)
     * @return Peer or NULL if not found
     */
    ZT_INLINE SharedPtr<Peer> peer(const CallContext& cc, const Address& zta, const bool loadFromCached = true)
    {
        {
            RWMutex::RLock l(m_peers_l);
            Map<Address, SharedPtr<Peer> >::const_iterator ap(m_peers.find(zta));
            if (likely(ap != m_peers.end()))
                return ap->second;
        }
        if (loadFromCached)
            return m_peerFromCached(cc, zta);
        return SharedPtr<Peer>();
    }

    /**
     * Get a Path object for a given local and remote physical address, creating if needed
     *
     * @param l Local socket
     * @param r Remote address
     * @return Pointer to canonicalized Path object or NULL on error
     */
    ZT_INLINE SharedPtr<Path> path(const int64_t l, const InetAddress& r)
    {
        const Path::Key k(r);
        {
            RWMutex::RLock lck(m_paths_l);
            Map<Path::Key, SharedPtr<Path> >::const_iterator p(m_paths.find(k));
            if (likely(p != m_paths.end()))
                return p->second;
        }
        return m_newPath(l, r, k);
    }

    /**
     * Get current best root
     *
     * @return Root peer or nullptr if none
     */
    ZT_INLINE SharedPtr<Peer> root()
    {
        l_bestRoot.lock();   // spinlock
        SharedPtr<Peer> r(m_bestRoot);
        l_bestRoot.unlock();
        return r;
    }

    /**
     * Get current best root by setting a result parameter
     *
     * @param root Set to best root or nullptr if none
     */
    ZT_INLINE void root(SharedPtr<Peer>& root)
    {
        l_bestRoot.lock();   // spinlock
        root = m_bestRoot;
        l_bestRoot.unlock();
    }

    /**
     * @param allPeers Vector to fill with all current peers
     * @param rootPeers Vector to fill with peers that are roots
     */
    void allPeers(Vector<SharedPtr<Peer> >& allPeers, Vector<SharedPtr<Peer> >& rootPeers) const;

    /**
     * Do periodic tasks such as database cleanup, cert cleanup, root ranking, etc.
     */
    void doPeriodicTasks(const CallContext& cc);

    /**
     * Rank root servers in descending order of quality
     */
    ZT_INLINE void rankRoots(const CallContext& cc)
    {
        Mutex::Lock l(m_roots_l);
        m_rankRoots();
    }

    /**
     * Perform internal updates based on changes in the trust store
     */
    void trustStoreChanged(const CallContext& cc);

    /**
     * Save all currently known peers to data store
     */
    void saveAll(const CallContext& cc);

  private:
    void m_rankRoots();
    void m_loadCached(const CallContext& cc, const Address& zta, SharedPtr<Peer>& peer);
    SharedPtr<Peer> m_peerFromCached(const CallContext& cc, const Address& zta);
    SharedPtr<Path> m_newPath(int64_t l, const InetAddress& r, const Path::Key& k);

    const Context& m_ctx;

    Vector<SharedPtr<Peer> > m_roots;
    Map<Address, SharedPtr<Peer> > m_peers;
    Map<Path::Key, SharedPtr<Path> > m_paths;

    RWMutex m_peers_l;   // m_peers
    RWMutex m_paths_l;   // m_paths
    Mutex m_roots_l;     // m_roots

    SharedPtr<Peer> m_bestRoot;
    Spinlock l_bestRoot;
};

}   // namespace ZeroTier

#endif
