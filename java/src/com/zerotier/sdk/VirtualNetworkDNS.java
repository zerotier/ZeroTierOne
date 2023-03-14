/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2020  ZeroTier, Inc.  https://www.zerotier.com/
 */

package com.zerotier.sdk;

import java.net.InetSocketAddress;
import java.util.ArrayList;

/**
 * DNS configuration to be pushed on a virtual network
 * <p>
 * Defined in ZeroTierOne.h as ZT_VirtualNetworkDNS
 */
public class VirtualNetworkDNS implements Comparable<VirtualNetworkDNS> {

    private final String domain;
    private final ArrayList<InetSocketAddress> servers;

    public VirtualNetworkDNS(String domain, ArrayList<InetSocketAddress> servers) {
        this.domain = domain;
        this.servers = servers;
    }

    @Override
    public String toString() {
        return "VirtualNetworkDNS(" + domain + ", " + servers + ")";
    }

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

        //noinspection RedundantIfStatement
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

    public String getDomain() {
        return domain;
    }

    public ArrayList<InetSocketAddress> getServers() {
        return servers;
    }
}
