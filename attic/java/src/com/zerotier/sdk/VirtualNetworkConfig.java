/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

package com.zerotier.sdk;

import java.lang.Comparable;
import java.lang.Override;
import java.lang.String;
import java.util.ArrayList;
import java.net.InetSocketAddress;

public final class VirtualNetworkConfig implements Comparable<VirtualNetworkConfig> {
    public static final int MAX_MULTICAST_SUBSCRIPTIONS = 4096;
    public static final int ZT_MAX_ZT_ASSIGNED_ADDRESSES = 16;

    private long nwid;
    private long mac;
    private String name;
    private VirtualNetworkStatus status;
    private VirtualNetworkType type;
    private int mtu;
    private boolean dhcp;
    private boolean bridge;
    private boolean broadcastEnabled;
    private int portError;
    private boolean enabled;
    private long netconfRevision;
    private InetSocketAddress[] assignedAddresses;
    private VirtualNetworkRoute[] routes;

    private VirtualNetworkConfig() {

    }

    public boolean equals(VirtualNetworkConfig cfg) {
        boolean aaEqual = true;
        if(assignedAddresses.length == cfg.assignedAddresses.length) {
            for(int i = 0; i < assignedAddresses.length; ++i) {
                if(!assignedAddresses[i].equals(cfg.assignedAddresses[i])) {
                    aaEqual = false;
                }
            }
        } else {
            aaEqual = false;
        }

        boolean routesEqual = true;
        if(routes.length == cfg.routes.length) {
            for (int i = 0; i < routes.length; ++i) {
                if (!routes[i].equals(cfg.routes[i])) {
                    routesEqual = false;
                }
            }
        } else {
            routesEqual = false;
        }

        return nwid == cfg.nwid &&
               mac == cfg.mac &&
               name.equals(cfg.name) &&
               status.equals(cfg.status) &&
               type.equals(cfg.type) &&
               mtu == cfg.mtu &&
               dhcp == cfg.dhcp &&
               bridge == cfg.bridge &&
               broadcastEnabled == cfg.broadcastEnabled &&
               portError == cfg.portError &&
               enabled == cfg.enabled &&
               aaEqual && routesEqual;
    }

    public int compareTo(VirtualNetworkConfig cfg) {
        if(cfg.nwid == this.nwid) {
            return 0;
        } else {
            return this.nwid > cfg.nwid ? 1 : -1;
        }
    }

    /**
     * 64-bit ZeroTier network ID
     */
    public final long networkId() {
        return nwid;
    }

    /**
     * Ethernet MAC (40 bits) that should be assigned to port
     */
    public final long macAddress() {
        return mac;
    }

    /**
     * Network name (from network configuration master)
     */
    public final String name() {
        return name;
    }

    /**
     * Network configuration request status
     */
    public final VirtualNetworkStatus networkStatus() {
        return status;
    }

    /**
     * Network type
     */
    public final VirtualNetworkType networkType() {
        return type;
    }

    /**
     * Maximum interface MTU
     */
    public final int mtu() {
        return mtu;
    }

    /**
     * If the network this port belongs to indicates DHCP availability
     *
     * <p>This is a suggestion. The underlying implementation is free to ignore it
     * for security or other reasons. This is simply a netconf parameter that
     * means 'DHCP is available on this network.'</p>
     */
    public final boolean isDhcpAvailable() {
        return dhcp;
    }

    /**
     * If this port is allowed to bridge to other networks
     *
     * <p>This is informational. If this is false, bridged packets will simply
     * be dropped and bridging won't work.</p>
     */
    public final boolean isBridgeEnabled() {
        return bridge;
    }

    /**
     * If true, this network supports and allows broadcast (ff:ff:ff:ff:ff:ff) traffic
     */
    public final boolean broadcastEnabled() {
        return broadcastEnabled;
    }

    /**
     * If the network is in PORT_ERROR state, this is the error most recently returned by the port config callback
     */
    public final int portError() {
        return portError;
    }

    /**
     * Network config revision as reported by netconf master
     *
     * <p>If this is zero, it means we're still waiting for our netconf.</p>
     */
    public final long netconfRevision() {
        return netconfRevision;
    }

    /**
     * ZeroTier-assigned addresses (in {@link java.net.InetSocketAddress} objects)
     *
     * For IP, the port number of the sockaddr_XX structure contains the number
     * of bits in the address netmask. Only the IP address and port are used.
     * Other fields like interface number can be ignored.
     *
     * This is only used for ZeroTier-managed address assignments sent by the
     * virtual network's configuration master.
     */
    public final InetSocketAddress[] assignedAddresses() {
        return assignedAddresses;
    }

    /**
     * ZeroTier-assigned routes (in {@link com.zerotier.sdk.VirtualNetworkRoute} objects)
     *
     * @return
     */
    public final VirtualNetworkRoute[] routes() { return routes; }
}
