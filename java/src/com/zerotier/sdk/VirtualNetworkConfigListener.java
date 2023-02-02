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


public interface VirtualNetworkConfigListener {

    /**
     * Callback called to update virtual network port configuration
     *
     * <p>This can be called at any time to update the configuration of a virtual
     * network port. The parameter after the network ID specifies whether this
     * port is being brought up, updated, brought down, or permanently deleted.
     *
     * This in turn should be used by the underlying implementation to create
     * and configure tap devices at the OS (or virtual network stack) layer.</P>
     *
     * This should not call {@link Node#multicastSubscribe} or other network-modifying
     * methods, as this could cause a deadlock in multithreaded or interrupt
     * driven environments.
     *
     * This must return 0 on success. It can return any OS-dependent error code
     * on failure, and this results in the network being placed into the
     * PORT_ERROR state.
     *
     * @param nwid network id
     * @param op {@link VirtualNetworkConfigOperation} enum describing the configuration operation
     * @param config {@link VirtualNetworkConfig} object with the new configuration
     * @return 0 on success
     */
    public int onNetworkConfigurationUpdated(
            long nwid,
            VirtualNetworkConfigOperation op,
            VirtualNetworkConfig config);
}
