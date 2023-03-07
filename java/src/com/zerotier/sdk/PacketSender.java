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


public interface PacketSender {

    /**
     * Function to send a ZeroTier packet out over the wire
     *
     * <p>The function must return zero on success and may return any error code
     * on failure. Note that success does not (of course) guarantee packet
     * delivery. It only means that the packet appears to have been sent.</p>
     *
     * @param localSocket socket file descriptor to send from.  Set to -1 if not specified.
     * @param remoteAddr {@link InetSocketAddress} to send to
     * @param packetData data to send
     * @param ttl TTL is ignored
     * @return 0 on success, any error code on failure.
     */
    int onSendPacketRequested(
            long localSocket,
            InetSocketAddress remoteAddr,
            byte[] packetData,
            int ttl);
}
