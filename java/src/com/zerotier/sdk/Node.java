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
import java.util.ArrayList;
import java.io.IOException;

/**
 * A ZeroTier One node
 */
public class Node {
	static {
        try {
    		System.loadLibrary("ZeroTierOneJNI");
        } catch (UnsatisfiedLinkError e) {
            try { 
                if(System.getProperty("os.name").startsWith("Windows")) {
                    System.out.println("Arch: " + System.getProperty("sun.arch.data.model"));
                    if(System.getProperty("sun.arch.data.model").equals("64")) {
                        NativeUtils.loadLibraryFromJar("/lib/ZeroTierOneJNI_win64.dll");
                    } else {
                        NativeUtils.loadLibraryFromJar("/lib/ZeroTierOneJNI_win32.dll");
                    }
                } else if(System.getProperty("os.name").startsWith("Mac")) {
                    NativeUtils.loadLibraryFromJar("/lib/libZeroTierOneJNI.jnilib");
                } else {
                    // TODO: Linux
                }
            } catch (IOException ioe) {
                ioe.printStackTrace();
            }
        }
	}

    private static final String TAG = "NODE";

    /**
     * Node ID for JNI purposes.
     * Currently set to the now value passed in at the constructor
     */
    private final long nodeId;
    
    /**
     * Create a new ZeroTier One node
     *
     * @param now Current clock in milliseconds
     */
    public Node(long now) {
        this.nodeId = now;
    }

    /**
     * Init a new ZeroTier One node
     *
     * <p>Note that this can take a few seconds the first time it's called, as it
     * will generate an identity.</p>
     *
     * @param getListener User written instance of the {@link DataStoreGetListener} interface called to get objects from persistent storage.  This instance must be unique per Node object.
     * @param putListener User written instance of the {@link DataStorePutListener} interface called to put objects in persistent storage.  This instance must be unique per Node object.
     * @param sender User written instance of the {@link PacketSender} interface to send ZeroTier packets out over the wire.
     * @param eventListener User written instance of the {@link EventListener} interface to receive status updates and non-fatal error notices.  This instance must be unique per Node object.
     * @param frameListener User written instance of the {@link VirtualNetworkFrameListener} interface to send a frame out to a virtual network port.
     * @param configListener User written instance of the {@link VirtualNetworkConfigListener} interface to be called when virtual LANs are created, deleted, or their config parameters change.  This instance must be unique per Node object.
     * @param pathChecker User written instance of the {@link PathChecker} interface. Not required and can be null.
     */
    public ResultCode init(
            DataStoreGetListener getListener,
            DataStorePutListener putListener,
            PacketSender sender,
            EventListener eventListener,
            VirtualNetworkFrameListener frameListener,
            VirtualNetworkConfigListener configListener,
            PathChecker pathChecker) throws NodeException {
        ResultCode rc = node_init(
                nodeId,
                getListener,
                putListener,
                sender,
                eventListener,
                frameListener,
                configListener,
                pathChecker);
        if(rc != ResultCode.RESULT_OK) {
            throw new NodeException(rc.toString());
        }
        return rc;
    }

    public boolean isInited() {
        return node_isInited(nodeId);
    }

    /**
      * Close this Node.
      * 
      * <p>The Node object can no longer be used once this method is called.</p>
      */
    public void close() {
        node_delete(nodeId);
    }

    @Override
    public String toString() {
        return "Node(" + nodeId + ")";
    }

    /**
     * Process a frame from a virtual network port
     *
     * @param now Current clock in milliseconds
     * @param nwid ZeroTier 64-bit virtual network ID
     * @param sourceMac Source MAC address (least significant 48 bits)
     * @param destMac Destination MAC address (least significant 48 bits)
     * @param etherType 16-bit Ethernet frame type
     * @param vlanId 10-bit VLAN ID or 0 if none
     * @param frameData Frame payload data
     * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode processVirtualNetworkFrame(
        long now,
        long nwid,
        long sourceMac,
        long destMac,
        int etherType,
        int vlanId,
        byte[] frameData,
        long[] nextBackgroundTaskDeadline) {
        return processVirtualNetworkFrame(
            nodeId, now, nwid, sourceMac, destMac, etherType, vlanId, 
            frameData, nextBackgroundTaskDeadline);
    }

    /**
     * Process a packet received from the physical wire
     *
     * @param now Current clock in milliseconds
     * @param localSocket Local socket or -1
     * @param remoteAddress Origin of packet
     * @param packetData Packet data
     * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode processWirePacket(
        long now,
        long localSocket,
        InetSocketAddress remoteAddress,
        byte[] packetData,
        long[] nextBackgroundTaskDeadline) {
        return processWirePacket(
            nodeId, now, localSocket, remoteAddress, packetData,
            nextBackgroundTaskDeadline);
    }

    /**
     * Perform periodic background operations
     *
     * @param now Current clock in milliseconds
     * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode processBackgroundTasks(long now, long[] nextBackgroundTaskDeadline) {
        return processBackgroundTasks(nodeId, now, nextBackgroundTaskDeadline);
    }

    /**
     * Join a network
     *
     * <p>This may generate calls to the port config callback before it returns,
     * or these may be deferred if a netconf is not available yet.</p>
     *
     * <p>If we are already a member of the network, nothing is done and OK is
     * returned.</p>
     *
     * @param nwid 64-bit ZeroTier network ID
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode join(long nwid) {
        return join(nodeId, nwid);
    }

    /**
     * Leave a network
     *
     * <p>If a port has been configured for this network this will generate a call
     * to the port config callback with a NULL second parameter to indicate that
     * the port is now deleted.</p>
     *
     * @param nwid 64-bit network ID
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode leave(long nwid) {
        return leave(nodeId, nwid);
    }

    /**
     * Subscribe to an Ethernet multicast group
     *
     * <p>For IPv4 ARP, the implementation must subscribe to 0xffffffffffff (the
     * broadcast address) but with an ADI equal to each IPv4 address in host
     * byte order. This converts ARP from a non-scalable broadcast protocol to
     * a scalable multicast protocol with perfect address specificity.</p>
     *
     * <p>If this is not done, ARP will not work reliably.</p>
     *
     * <p>Multiple calls to subscribe to the same multicast address will have no
     * effect. It is perfectly safe to do this.</p>
     *
     * <p>This does not generate an update call to the {@link VirtualNetworkConfigListener#onNetworkConfigurationUpdated} method.</p>
     *
     * @param nwid 64-bit network ID
     * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode multicastSubscribe(
		long nwid,
		long multicastGroup) {
		return multicastSubscribe(nodeId, nwid, multicastGroup, 0);
	}

    /**
     * Subscribe to an Ethernet multicast group
     *
     * <p>ADI stands for additional distinguishing information. This defaults to zero
     * and is rarely used. Right now its only use is to enable IPv4 ARP to scale,
     * and this must be done.</p>
     *
     * <p>For IPv4 ARP, the implementation must subscribe to 0xffffffffffff (the
     * broadcast address) but with an ADI equal to each IPv4 address in host
     * byte order. This converts ARP from a non-scalable broadcast protocol to
     * a scalable multicast protocol with perfect address specificity.</p>
     *
     * <p>If this is not done, ARP will not work reliably.</p>
     *
     * <p>Multiple calls to subscribe to the same multicast address will have no
     * effect. It is perfectly safe to do this.</p>
     *
     * <p>This does not generate an update call to the {@link VirtualNetworkConfigListener#onNetworkConfigurationUpdated} method.</p>
     *
     * @param nwid 64-bit network ID
     * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
     * @param multicastAdi Multicast ADI (least significant 32 bits only, default: 0)
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode multicastSubscribe(
        long nwid,
        long multicastGroup,
        long multicastAdi) {
        return multicastSubscribe(nodeId, nwid, multicastGroup, multicastAdi);
    }


    /**
     * Unsubscribe from an Ethernet multicast group (or all groups)
     *
     * <p>If multicastGroup is zero (0), this will unsubscribe from all groups. If
     * you are not subscribed to a group this has no effect.</p>
     *
     * <p>This does not generate an update call to the {@link VirtualNetworkConfigListener#onNetworkConfigurationUpdated} method.</p>
     *
     * @param nwid 64-bit network ID
     * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
     * @return OK (0) or error code if a fatal error condition has occurred
     */
	public ResultCode multicastUnsubscribe(
		long nwid,
		long multicastGroup) {
		return multicastUnsubscribe(nodeId, nwid, multicastGroup, 0);
	}

    /**
     * Unsubscribe from an Ethernet multicast group (or all groups)
     *
     * <p>If multicastGroup is zero (0), this will unsubscribe from all groups. If
     * you are not subscribed to a group this has no effect.</p>
     *
     * <p>This does not generate an update call to the {@link VirtualNetworkConfigListener#onNetworkConfigurationUpdated} method.</p>
     *
     * <p>ADI stands for additional distinguishing information. This defaults to zero
     * and is rarely used. Right now its only use is to enable IPv4 ARP to scale,
     * and this must be done.</p>
     *
     * @param nwid 64-bit network ID
     * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
     * @param multicastAdi Multicast ADI (least significant 32 bits only, default: 0)
     * @return OK (0) or error code if a fatal error condition has occurred
     */
    public ResultCode multicastUnsubscribe(
        long nwid,
        long multicastGroup,
        long multicastAdi) {
        return multicastUnsubscribe(nodeId, nwid, multicastGroup, multicastAdi);
    }

    /**
     * Add or update a moon
     *
     * Moons are persisted in the data store in moons.d/, so this can persist
     * across invocations if the contents of moon.d are scanned and orbit is
     * called for each on startup.
     *
     * @param moonWorldId Moon's world ID
     * @param moonSeed If non-zero, the ZeroTier address of any member of the moon to query for moon definition
     * @return Error if moon was invalid or failed to be added
     */
    public ResultCode orbit(
            long moonWorldId,
            long moonSeed) {
        return orbit(nodeId, moonWorldId, moonSeed);
    }

    /**
     * Remove a moon (does nothing if not present)
     *
     * @param moonWorldId World ID of moon to remove
     * @return Error if anything bad happened
     */
    public ResultCode deorbit(
            long moonWorldId) {
        return deorbit(nodeId, moonWorldId);
    }

    /**
     * Get this node's 40-bit ZeroTier address
     *
     * @return ZeroTier address (least significant 40 bits of 64-bit int)
     */
    public long address() {
        return address(nodeId);
    }

    /**
     * Get the status of this node
     *
     * @return @{link NodeStatus} struct with the current node status.
     */
    public NodeStatus status() {
        return status(nodeId);
    }

    /**
     * Get a list of known peer nodes
     *
     * @return List of known peers or NULL on failure
     */
    public Peer[] peers() {
        return peers(nodeId);
    }

    /**
     * Get the status of a virtual network
     *
     * @param nwid 64-bit network ID
     * @return {@link VirtualNetworkConfig} or NULL if we are not a member of this network
     */
    public VirtualNetworkConfig networkConfig(long nwid) {
        return networkConfig(nodeId, nwid);
    }

    /**
     * Enumerate and get status of all networks
     *
     * @return List of networks or NULL on failure
     */
    public VirtualNetworkConfig[] networks() {
        return networks(nodeId);
    }

    /**
     * Get ZeroTier One version
     *
     * @return {@link Version} object with ZeroTierOne version information.
     */
    public Version getVersion() {
        return version();
    }

    //
    // function declarations for JNI
    //
    private native ResultCode node_init(
            long nodeId,
            DataStoreGetListener dataStoreGetListener,
            DataStorePutListener dataStorePutListener,
            PacketSender packetSender,
            EventListener eventListener,
            VirtualNetworkFrameListener virtualNetworkFrameListener,
            VirtualNetworkConfigListener virtualNetworkConfigListener,
            PathChecker pathChecker);

    private native boolean node_isInited(long nodeId);

    private native void node_delete(long nodeId);

    private native ResultCode processVirtualNetworkFrame(
        long nodeId,
        long now,
        long nwid,
        long sourceMac,
        long destMac,
        int etherType,
        int vlanId,
        byte[] frameData,
        long[] nextBackgroundTaskDeadline);

    private native ResultCode processWirePacket(
        long nodeId,
        long now,
        long localSocket,
        InetSocketAddress remoteAddress,
        byte[] packetData,
        long[] nextBackgroundTaskDeadline);

    private native ResultCode processBackgroundTasks(
        long nodeId,
        long now,
        long[] nextBackgroundTaskDeadline);

    private native ResultCode join(long nodeId, long nwid);

    private native ResultCode leave(long nodeId, long nwid);

    private native ResultCode multicastSubscribe(
        long nodeId,
        long nwid,
        long multicastGroup,
        long multicastAdi);

    private native ResultCode multicastUnsubscribe(
        long nodeId,
        long nwid,
        long multicastGroup,
        long multicastAdi);

    private native ResultCode orbit(
            long nodeId,
            long moonWorldId,
            long moonSeed);

    private native ResultCode deorbit(
            long nodeId,
            long moonWorldId);

    private native long address(long nodeId);

    private native NodeStatus status(long nodeId);

    private native VirtualNetworkConfig networkConfig(long nodeId, long nwid);

    private native Version version();

    private native Peer[] peers(long nodeId);

    private native VirtualNetworkConfig[] networks(long nodeId);
}
