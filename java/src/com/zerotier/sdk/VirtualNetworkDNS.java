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

    @Override
    public boolean equals(Object o) {

        if (o == null) {
            return false;
        }

        if (!(o instanceof VirtualNetworkDNS)) {
            return false;
        }

        VirtualNetworkDNS d = (VirtualNetworkDNS) o;

        if (!domain.equals(d.domain)) {
            return false;
        }

        if (!servers.equals(d.servers)) {
            return false;
        }

        return true;
    }

    @Override
    public int compareTo(VirtualNetworkDNS o) {
        return domain.compareTo(o.domain);
    }

    @Override
    public int hashCode() {

        int result = 17;
        result = 37 * result + domain.hashCode();
        result = 37 * result + servers.hashCode();

        return result;
    }

    public String getSearchDomain() { return domain; }

    public ArrayList<InetSocketAddress> getServers() { return servers; }
}
