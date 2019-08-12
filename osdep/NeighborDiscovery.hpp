/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_NEIGHBORDISCOVERY_HPP
#define ZT_NEIGHBORDISCOVERY_HPP

#include "../node/Hashtable.hpp"
#include "../node/MAC.hpp"
#include "../node/InetAddress.hpp"


#define ZT_ND_QUERY_INTERVAL 2000

#define ZT_ND_QUERY_MAX_TTL 5000

#define ZT_ND_EXPIRE 600000


namespace ZeroTier {

class NeighborDiscovery
{
public:
    NeighborDiscovery();

    /**
     * Set a local IP entry that we should respond to Neighbor Requests withPrefix64k
     *
     * @param mac Our local MAC address
     * @param ip Our IPv6 address
     */
    void addLocal(const sockaddr_storage &address, const MAC &mac);

    /**
     * Delete a local IP entry or cached Neighbor entry
     *
     * @param address IPv6 address to remove
     */
    void remove(const sockaddr_storage &address);

    sockaddr_storage processIncomingND(const uint8_t *nd, unsigned int len, const sockaddr_storage &localIp, uint8_t *response, unsigned int &responseLen, MAC &responseDest);

    MAC query(const MAC &localMac, const sockaddr_storage &localIp, const sockaddr_storage &targetIp, uint8_t *query, unsigned int &queryLen, MAC &queryDest);

private:
    struct _NDEntry
    {
        _NDEntry() : lastQuerySent(0), lastResponseReceived(0), mac(), local(false) {}
        uint64_t lastQuerySent;
        uint64_t lastResponseReceived;
        MAC mac;
        bool local;
    };

    Hashtable<InetAddress, _NDEntry> _cache;
    uint64_t _lastCleaned;
};

}  // namespace ZeroTier

#endif
