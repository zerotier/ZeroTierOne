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

public interface VirtualNetworkFrameListener {

    /**
     * Function to send a frame out to a virtual network port
     * 
     * @param nwid ZeroTier One network ID
     * @param srcMac source MAC address
     * @param destMac destination MAC address
     * @param ethertype
     * @param vlanId
     * @param frameData data to send
     */
    public void onVirtualNetworkFrame(
                long nwid,
                long srcMac,
                long destMac,
                long etherType,
                long vlanId,
                byte[] frameData);
}
