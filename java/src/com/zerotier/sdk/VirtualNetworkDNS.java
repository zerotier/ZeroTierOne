/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2020  ZeroTier, Inc.  https://www.zerotier.com/
 */

package com.zerotier.sdk;

import java.net.InetSocketAddress;
import java.util.ArrayList;

public class VirtualNetworkDNS implements Comparable<VirtualNetworkDNS> {
    private String domain;
    private ArrayList<InetSocketAddress> servers;

    public VirtualNetworkDNS() {}

    public boolean equals(VirtualNetworkDNS o) {
        return domain.equals(o.domain) && servers.equals(o.servers);
    }

    @Override
    public int compareTo(VirtualNetworkDNS o) {
        return domain.compareTo(o.domain);
    }

    public String getSearchDomain() { return domain; }

    public ArrayList<InetSocketAddress> getServers() { return servers; }
}
