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

public class VirtualNetworkConfig {
    public static final int MAX_MULTICAST_SUBSCRIPTIONS = 4096;
    private long nwid;
    private long mac;
    private String name;
    private VirtualNetworkStatus status;
    private VirtualNetworkType type;
    private int mtu;
    private int dhcp;
    private int bridge;
    private int broadcastEnabled;
    private int portError;
    private boolean enabled;
    private long netconfRevision;
    private int multicastSubscriptionCount;
    private ArrayList<MulticastGroup> multicastSubscriptions;
    
    // TODO: sockaddr_storage
}