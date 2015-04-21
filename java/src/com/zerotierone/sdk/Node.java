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

    private static final String TAG = "NODE";

    /**
     * Node ID for JNI purposes.
     * Currently set to the now value passed in at the constructor
     * 
     * -1 if the node has already been closed
     */
    private final long nodeId;

    private final DataStoreGetListener getListener;
    private final DataStorePutListener putListener;
    private final PacketSender sender;
    private final VirtualNetworkFrameListener frameListener;
    private final VirtualNetworkConfigListener configListener;
    

    private native ResultCode node_init(long now);
    private native void node_delete(long nodeId);

	public Node(long now,
                DataStoreGetListener getListener,
                DataStorePutListener putListener,
                PacketSender sender,
                VirtualNetworkFrameListener frameListener,
                VirtualNetworkConfigListener configListener)
	{
        this.nodeId = now;

        this.getListener = getListener;
        this.putListener = putListener;
        this.sender = sender;
        this.frameListener = frameListener;
        this.configListener = configListener;

        ResultCode rc = node_init(now);
        if(rc.getValue() != ResultCode.RESULT_OK)
        {
            // TODO: Throw Exception
        }
	}

    public void close() {
        if(nodeId != -1) {
            node_delete(nodeId);
            nodeId = -1;
        }
    }

    @Override
    protected void finalize() {
        close();
    }

	private native ResultCode processVirtualNetworkFrame(
        long nodeId,
		long now,
		long nwid,
		long sourceMac,
		long destMac,
		int etherType,
		int vlanId,
		ByteBuffer frameData,
		int frameLength,
		Long nextBackgroundTaskDeadline);

    public ResultCode processVirtualNetworkFrame(
        long now,
        long nwid,
        long sourceMac,
        long destMac,
        int etherType,
        int vlanId,
        ByteBuffer frameData,
        int frameLength,
        Long nextBackgroundTaskDeadline) {
        return processVirtualNetworkFrame(
            nodeId, now, nwid, sourceMac, destMac, etherType, vlanId, 
            frameData, frameLength, nextBackgroundTaskDeadline);
    }

	private native ResultCode processBackgroundTasks(
        long nodeId,
		long now,
		Long nextBackgroundTaskDeadline);

    public ResultCode processBackgroundTasks(long now, long nextBackgroundTaskDeadline) {
        return processBackgroundTasks(nodeId, now, nextBackgroundTaskDeadline);
    }

	private native ResultCode join(long nodeId, long nwid);

    public ResultCode join(long nwid) {
        return join(nodeId, nwid);
    }

	private native ResultCode leave(long nodeId, long nwid);

    public ResultCode leave(long nwid) {
        return leave(nodeId, nwid);
    }

	private native ResultCode multicastSubscribe(
        long nodeId,
		long nwid,
		long multicastGroup,
		long multicastAdi);

	public ResultCode multicastSubscribe(
		long nwid,
		long multicastGroup) {
		return multicastSubscribe(nodeId, nwid, multicastGroup, 0);
	}

    public ResultCode multicastSubscribe(
        long nwid,
        long multicastGroup,
        long multicastAdi) {
        return multicastSubscribe(nodeId, nwid, multicastGroup, multicastAdi);
    }

	private native ResultCode multicastUnsubscribe(
        long nodeId,
		long nwid,
		long multicastGroup,
		long multicastAdi);

	public ResultCode multicastUnsubscribe(
		long nwid,
		long multicastGroup) {
		return multicastUnsubscribe(nodeId, nwid, multicastGroup, 0);
	}

    public ResultCode multicastUnsubscribe(
        long nwid,
        long multicastGroup,
        long multicastAdi) {
        return multicastUnsubscribe(nodeId, nwid, multicastGroup, multicastAdi);
    }

	private native long address(long nodeId);

    public long address() {
        return address(nodeId);
    }
}