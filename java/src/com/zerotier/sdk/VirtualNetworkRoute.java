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
 * A route to be pushed on a virtual network
 * <p>
 * Defined in ZeroTierOne.h as ZT_VirtualNetworkRoute
 */
public class VirtualNetworkRoute implements Comparable<VirtualNetworkRoute>
{
	/**
	 * Target network / netmask bits (in port field) or NULL or 0.0.0.0/0 for default
	 */
    private final InetSocketAddress target;
    
	/**
	 * Gateway IP address (port ignored) or NULL (family == 0) for LAN-local (no gateway)
	 */
    private final InetSocketAddress via;

	/**
	 * Route flags
	 */
    private final int flags;

	/**
	 * Route metric (not currently used)
	 */
    private final int metric;

    public VirtualNetworkRoute(InetSocketAddress target, InetSocketAddress via, int flags, int metric) {
        this.target = target;
        this.via = via;
        this.flags = flags;
        this.metric = metric;
    }

    @Override
    public String toString() {
        return "VirtualNetworkRoute(" + target + ", " + via + ", " + flags + ", " + metric + ")";
    }

    @Override
    public int compareTo(VirtualNetworkRoute other) {
        throw new RuntimeException("Unimplemented");
    }

    @Override
    public boolean equals(Object o) {

        if (!(o instanceof VirtualNetworkRoute)) {
            return false;
        }

        VirtualNetworkRoute other = (VirtualNetworkRoute) o;

        boolean targetEquals;
        if (target == null) {
            //noinspection RedundantIfStatement
            if (other.target == null) {
                targetEquals = true;
            } else {
                targetEquals = false;
            }
        } else {
            if (other.target == null) {
                targetEquals = false;
            } else {
                targetEquals = target.equals(other.target);
            }
        }

        if (!targetEquals) {
            return false;
        }

        boolean viaEquals;
        if (via == null) {
            //noinspection RedundantIfStatement
            if (other.via == null) {
                viaEquals = true;
            } else {
                viaEquals = false;
            }
        } else {
            if (other.via == null) {
                viaEquals = false;
            } else {
                viaEquals = via.equals(other.via);
            }
        }

        if (!viaEquals) {
            return false;
        }

        if (flags != other.flags) {
            return false;
        }

        //noinspection RedundantIfStatement
        if (metric != other.metric) {
            return false;
        }

        return true;
    }

    @Override
    public int hashCode() {

        int result = 17;
        result = 37 * result + (target == null ? 0 : target.hashCode());
        result = 37 * result + (via == null ? 0 : via.hashCode());
        result = 37 * result + flags;
        result = 37 * result + metric;

        return result;
    }

    public InetSocketAddress getTarget() {
        return target;
    }

    public InetSocketAddress getVia() {
        return via;
    }

    public int getFlags() {
        return flags;
    }

    public int getMetric() {
        return metric;
    }
}
