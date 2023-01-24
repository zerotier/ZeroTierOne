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

import android.util.Log;

import com.zerotier.sdk.util.StringUtils;

import java.lang.Comparable;
import java.lang.Override;
import java.lang.String;
import java.util.ArrayList;
import java.net.InetSocketAddress;
import java.util.Collections;

public final class VirtualNetworkConfig implements Comparable<VirtualNetworkConfig> {
    private final static String TAG = "VirtualNetworkConfig";

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
    private VirtualNetworkDNS dns;

    private VirtualNetworkConfig() {

    }

    public boolean equals(VirtualNetworkConfig cfg) {
        ArrayList<String> aaCurrent = new ArrayList<>();
        ArrayList<String> aaNew = new ArrayList<>();
        for (InetSocketAddress s : assignedAddresses) {
            aaCurrent.add(s.toString());
        }
        for (InetSocketAddress s : cfg.assignedAddresses) {
            aaNew.add(s.toString());
        }
        Collections.sort(aaCurrent);
        Collections.sort(aaNew);
        boolean aaEqual = aaCurrent.equals(aaNew);

        ArrayList<String> rCurrent = new ArrayList<>();
        ArrayList<String> rNew = new ArrayList<>();
        for (VirtualNetworkRoute r : routes) {
            rCurrent.add(r.toString());
        }
        for (VirtualNetworkRoute r : cfg.routes) {
            rNew.add(r.toString());
        }
        Collections.sort(rCurrent);
        Collections.sort(rNew);
        boolean routesEqual = rCurrent.equals(rNew);

        if (this.nwid != cfg.nwid) {
            Log.i(TAG, "nwid Changed. Old: " + StringUtils.networkIdToString(this.nwid) + " (" + this.nwid + "), " +
                    "New: " + StringUtils.networkIdToString(cfg.nwid) + " (" + cfg.nwid + ")");
        }
        if (this.mac != cfg.mac) {
            Log.i(TAG, "MAC Changed. Old: " + StringUtils.macAddressToString(this.mac) + ", New: " + StringUtils.macAddressToString(cfg.mac));
        }

        if (!this.name.equals(cfg.name)) {
            Log.i(TAG, "Name Changed.  Old: " + this.name + " New: "+ cfg.name);
        }

        if (!this.type.equals(cfg.type)) {
            Log.i(TAG, "TYPE changed.  Old " + this.type + ", New: " + cfg.type);
        }

        if (this.mtu != cfg.mtu) {
            Log.i(TAG, "MTU Changed.  Old: " + this.mtu + ", New: " + cfg.mtu);
        }

        if (this.dhcp != cfg.dhcp) {
            Log.i(TAG, "DHCP Flag Changed. Old: " + this.dhcp + ", New: " + cfg.dhcp);
        }

        if (this.bridge != cfg.bridge) {
            Log.i(TAG, "Bridge Flag Changed. Old: " + this.bridge + ", New: " + cfg.bridge);
        }

        if (this.broadcastEnabled != cfg.broadcastEnabled) {
            Log.i(TAG, "Broadcast Flag Changed. Old: "+ this.broadcastEnabled +", New: " + this.broadcastEnabled);
        }

        if (this.portError != cfg.portError) {
            Log.i(TAG, "Port Error Changed. Old: " + this.portError + ", New: " + this.portError);
        }

        if (this.enabled != cfg.enabled) {
            Log.i(TAG, "Enabled Changed. Old: " + this.enabled + ", New: " + this.enabled);
        }

        if (!aaEqual) {
            Log.i(TAG, "Assigned Addresses Changed");
            Log.i(TAG, "Old:");
            for (String s : aaCurrent) {
                Log.i(TAG, "    " + s);
            }
            Log.i(TAG, "New:");
            for (String s : aaNew) {
                Log.i(TAG, "    " +s);
            }
        }

        if (!routesEqual) {
            Log.i(TAG, "Managed Routes Changed");
            Log.i(TAG, "Old:");
            for (String s : rCurrent) {
                Log.i(TAG, "    " + s);
            }
            Log.i(TAG, "New:");
            for (String s : rNew) {
                Log.i(TAG, "    " + s);
            }
        }

        boolean dnsEquals = false;
        if (this.dns == null || cfg.dns == null) {
            dnsEquals = true;
        } else if (this.dns != null) {
            dnsEquals = this.dns.equals(cfg.dns);
        }

        return this.nwid == cfg.nwid &&
               this.mac == cfg.mac &&
               this.name.equals(cfg.name) &&
               this.status.equals(cfg.status) &&
               this.type.equals(cfg.type) &&
               this.mtu == cfg.mtu &&
               this.dhcp == cfg.dhcp &&
               this.bridge == cfg.bridge &&
               this.broadcastEnabled == cfg.broadcastEnabled &&
               this.portError == cfg.portError &&
               this.enabled == cfg.enabled &&
               dnsEquals &&
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

    public final VirtualNetworkDNS dns() { return dns; }
}
