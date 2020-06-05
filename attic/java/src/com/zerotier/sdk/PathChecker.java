/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
 */

package com.zerotier.sdk;

import java.net.InetSocketAddress;

public interface PathChecker {
    /**
     * Callback to check whether a path should be used for ZeroTier traffic
     *
     * This function must return true if the path should be used.
     *
     * If no path check function is specified, ZeroTier will still exclude paths
     * that overlap with ZeroTier-assigned and managed IP address blocks. But the
     * use of a path check function is recommended to ensure that recursion does
     * not occur in cases where addresses are assigned by the OS or managed by
     * an out of band mechanism like DHCP. The path check function should examine
     * all configured ZeroTier interfaces and check to ensure that the supplied
     * addresses will not result in ZeroTier traffic being sent over a ZeroTier
     * interface (recursion).
     *
     * Obviously this is not required in configurations where this can't happen,
     * such as network containers or embedded.
     *
     * @param ztAddress ZeroTier address or 0 for none/any
     * @param localSocket Local interface socket.  -1 if unspecified
     * @param remoteAddress remote address
     */
    boolean onPathCheck(long ztAddress, long localSocket, InetSocketAddress remoteAddress);

    /**
     * Function to get physical addresses for ZeroTier peers
     *
     * If provided this function will be occasionally called to get physical
     * addresses that might be tried to reach a ZeroTier address.
     *
     * @param ztAddress ZeroTier address (least significant 40 bits)
     * @param ss_family desired address family or -1 for any
     * @return address and port of ztAddress or null
     */
    InetSocketAddress onPathLookup(long ztAddress, int ss_family);
}
