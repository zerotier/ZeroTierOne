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

import java.util.ArrayList;

public class Peer {
    private long address;
    private long lastUnicastFrame;
    private long lastMulticastFrame;
    private int versionMajor;
    private int versionMinor;
    private int versionRev;
    private int latency;
    private PeerRole role;
    private ArrayList<PhysicalPeerPath> paths;

    private Peer() {}

    public final long address() {
        return address;
    }

    public final long lastUnicastFrame() {
        return lastUnicastFrame;
    }

    public final long lastMulticastFrame() {
        return lastMulticastFrame;
    }

    public final int versionMajor() {
        return versionMajor;
    }

    public final int versionMinor() {
        return versionMinor;
    }

    public final int versionRev() {
        return versionRev;
    }

    public final int latency() {
        return latency;
    }

    public final PeerRole role() {
        return role;
    }

    public final ArrayList<PhysicalPeerPath> paths() {
        return paths;
    }
}