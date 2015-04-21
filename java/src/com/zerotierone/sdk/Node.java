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

import java.nio.ByteBuffer;
import java.lang.Long;

public class Node {
	static {
		System.loadLibrary("ZeroTierOneJNI");
	}

        private final DataStoreGetListener getListener;
        private final DataStorePutListener putListener;
        private final PacketSender sender;
        private final VirtualNetworkFrameListener frameListener;
        private final VirtualNetworkConfigListener configListener;
        
	public Node(long now,
                DataStoreGetListener getListener,
                DataStorePutListener putListener,
                PacketSender sender,
                VirtualNetworkFrameListener frameListener,
                VirtualNetworkConfigListener configListener)
	{
            this.getListener = getListener;
            this.putListener = putListener;
            this.sender = sender;
            this.frameListener = frameListener;
            this.configListener = configListener;
	}

	public native ResultCode processVirtualNetworkFrame(
		long now,
		long nwid,
		long sourceMac,
		long destMac,
		int etherTYpe,
		int vlanId,
		ByteBuffer frameData,
		int frameLength,
		Long nextBackgroundTaskDeadline);

	public native ResultCode processBackgroundTasks(
		long now,
		Long nextBackgroundTaskDeadline);

	public native ResultCode join(long nwid);

	public native ResultCode leave(long nwid);

	public native ResultCode multicastSubscribe(
		long nwid,
		long multicastGroup,
		long multicastAdi);

	public ResultCode multicastSubscribe(
		long nwid,
		long multicastGroup) {
		return multicastSubscribe(nwid, multicastGroup, 0);
	}

	public native ResultCode multicastUnsubscribe(
		long nwid,
		long multicastGroup,
		long multicastAdi);

	public ResultCode multicastUnsubscribe(
		long nwid,
		long multicastGroup) {
		return multicastUnsubscribe(nwid, multicastGroup, 0);
	}

	public native long address();



}