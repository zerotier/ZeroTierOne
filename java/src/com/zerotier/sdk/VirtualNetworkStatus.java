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

/**
 * Virtual network status codes
 *
 * Defined in ZeroTierOne.h as ZT_VirtualNetworkStatus
 */
public enum VirtualNetworkStatus {

    /**
     * Waiting for network configuration (also means revision == 0)
     */
    NETWORK_STATUS_REQUESTING_CONFIGURATION(0),

    /**
     * Configuration received and we are authorized
     */
    NETWORK_STATUS_OK(1),

    /**
     * Netconf master told us 'nope'
     */
    NETWORK_STATUS_ACCESS_DENIED(2),

    /**
     * Netconf master exists, but this virtual network does not
     */
    NETWORK_STATUS_NOT_FOUND(3),

    /**
     * Initialization of network failed or other internal error
     */
    NETWORK_STATUS_PORT_ERROR(4),

    /**
     * ZeroTier One version too old
     */
    NETWORK_STATUS_CLIENT_TOO_OLD(5),

    /**
     * External authentication is required (e.g. SSO)
     */
    NETWORK_STATUS_AUTHENTICATION_REQUIRED(6);

    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private final int id;

    VirtualNetworkStatus(int id) {
        this.id = id;
    }

    public static VirtualNetworkStatus fromInt(int id) {
        switch (id) {
            case 0:
                return NETWORK_STATUS_REQUESTING_CONFIGURATION;
            case 1:
                return NETWORK_STATUS_OK;
            case 2:
                return NETWORK_STATUS_ACCESS_DENIED;
            case 3:
                return NETWORK_STATUS_NOT_FOUND;
            case 4:
                return NETWORK_STATUS_PORT_ERROR;
            case 5:
                return NETWORK_STATUS_CLIENT_TOO_OLD;
            case 6:
                return NETWORK_STATUS_AUTHENTICATION_REQUIRED;
            default:
                throw new RuntimeException("Unhandled value: " + id);
        }
    }
}
