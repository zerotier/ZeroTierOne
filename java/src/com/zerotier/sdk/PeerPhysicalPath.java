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

import java.net.InetSocketAddress;

/**
 * Physical network path to a peer
 */
public final class PeerPhysicalPath {
    private InetSocketAddress address;
    private long lastSend;
    private long lastReceive;
    private boolean fixed;
    private boolean preferred;

    private PeerPhysicalPath() {}

    /**
     * Address of endpoint
     */
    public final InetSocketAddress address() {
        return address;
    }

    /**
     * Time of last send in milliseconds or 0 for never
     */
    public final long lastSend() {
        return lastSend;
    }

    /**
     * Time of last receive in milliseconds or 0 for never
     */
    public final long lastReceive() {
        return lastReceive;
    }

    /**
     * Is path fixed? (i.e. not learned, static)
     */
    public final boolean isFixed() {
        return fixed;
    }

    /**
     * Is path preferred?
     */
    public final boolean isPreferred() {
        return preferred;
    }
}