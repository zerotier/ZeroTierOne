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

package com.zerotierone.sdk;

import java.lang.String;
import java.util.ArrayList;
import java.net.InetAddress;

public class VirtualNetworkConfig {
    public static final int MAX_MULTICAST_SUBSCRIPTIONS = 4096;
    public static final int ZT1_MAX_ZT_ASSIGNED_ADDRESSES = 16;

    private long nwid;
    private long mac;
    private String name;
    private VirtualNetworkStatus status;
    private VirtualNetworkType type;
    private int mtu;
    private boolean dhcp;
    private boolean bridge;
    private boolean broadcastEnabled;
    private boolean portError;
    private boolean enabled;
    private long netconfRevision;
    private int multicastSubscriptionCount;
    private ArrayList<MulticastGroup> multicastSubscriptions;
    private ArrayList<InetAddress> assignedAddresses;

    private VirtualNetworkConfig() {

    }

    public final long networkId() {
        return nwid;
    }
    public final long macAddress() {
        return mac;
    }

    public final String name() {
        return name;
    }

    public final VirtualNetworkStatus networkStatus() {
        return status;
    }

    public final VirtualNetworkType networkType() {
        return type;
    }

    public final int mtu() {
        return mtu;
    }

    public final boolean isDhcpAvailable() {
        return dhcp;
    }

    public final boolean isBridgeEnabled() {
        return bridge;
    }

    public final boolean broadcastEnabled() {
        return broadcastEnabled;
    }

    public final boolean portError() {
        return portError;
    }

    public final boolean isEnabled() {
        return enabled;
    }

    public final long netconfRevision() {
        return netconfRevision;
    }

    public final ArrayList<MulticastGroup> multicastSubscriptions() {
        return multicastSubscriptions;
    }

    public final ArrayList<InetAddress> assignedAddresses() {
        return assignedAddresses;
    }
}
