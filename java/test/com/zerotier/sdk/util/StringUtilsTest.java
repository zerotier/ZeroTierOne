/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2023  ZeroTier, Inc.  https://www.zerotier.com/
 */

package com.zerotier.sdk.util;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class StringUtilsTest {

    public StringUtilsTest() {
    }

    public String oldMacDisplay(long mac) {

        String macStr = Long.toHexString(mac);

        if (macStr.length() > 12) {
            throw new RuntimeException();
        }

        while (macStr.length() < 12) {
            //noinspection StringConcatenationInLoop
            macStr = "0" + macStr;
        }

        //noinspection StringBufferReplaceableByString
        StringBuilder displayMac = new StringBuilder();
        displayMac.append(macStr.charAt(0));
        displayMac.append(macStr.charAt(1));
        displayMac.append(':');
        displayMac.append(macStr.charAt(2));
        displayMac.append(macStr.charAt(3));
        displayMac.append(':');
        displayMac.append(macStr.charAt(4));
        displayMac.append(macStr.charAt(5));
        displayMac.append(':');
        displayMac.append(macStr.charAt(6));
        displayMac.append(macStr.charAt(7));
        displayMac.append(':');
        displayMac.append(macStr.charAt(8));
        displayMac.append(macStr.charAt(9));
        displayMac.append(':');
        displayMac.append(macStr.charAt(10));
        displayMac.append(macStr.charAt(11));

        return displayMac.toString();
    }

    @Test
    public void testMacDisplay() {

        long mac1 = 1234567891;
        assertThat(StringUtils.macAddressToString(mac1)).isEqualTo(oldMacDisplay(mac1));

        long mac2 = 999999999;
        assertThat(StringUtils.macAddressToString(mac2)).isEqualTo(oldMacDisplay(mac2));

        long mac3 = 0x7fffffffffffL;
        assertThat(StringUtils.macAddressToString(mac3)).isEqualTo(oldMacDisplay(mac3));
        assertThat(StringUtils.macAddressToString(mac3)).isEqualTo("7f:ff:ff:ff:ff:ff");

        long mac4 = 0x7fafcf3f8fffL;
        assertThat(StringUtils.macAddressToString(mac4)).isEqualTo(oldMacDisplay(mac4));
        assertThat(StringUtils.macAddressToString(mac4)).isEqualTo("7f:af:cf:3f:8f:ff");
    }
}
