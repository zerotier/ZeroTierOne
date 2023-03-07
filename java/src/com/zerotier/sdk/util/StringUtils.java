/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2023  ZeroTier, Inc.  https://www.zerotier.com/
 */

package com.zerotier.sdk.util;

public class StringUtils {

    /**
     * Convert mac address to string.
     *
     * @param mac MAC address
     * @return string in XX:XX:XX:XX:XX:XX format
     */
    public static String macAddressToString(long mac) {

        int[] macChars = new int[6];
        for (int i = 0; i < 6; i++) {
            macChars[i] = (int) (mac % 256);
            mac >>= 8;
        }

        return String.format("%02x:%02x:%02x:%02x:%02x:%02x", macChars[5], macChars[4], macChars[3], macChars[2], macChars[1], macChars[0]);
    }

    /**
     * Convert long to hex string.
     *
     * @param networkId long
     * @return string with 0 padding
     */
    public static String networkIdToString(long networkId) {
        return String.format("%016x", networkId);
    }

    /**
     * Convert node address to string.
     *
     * Node addresses are 40 bits, so print 10 hex characters.
     *
     * @param address Node address
     * @return formatted string
     */
    public static String addressToString(long address) {
        return String.format("%010x", address);
    }

    public static String etherTypeToString(long etherType) {
        return String.format("%04x", etherType);
    }
}
