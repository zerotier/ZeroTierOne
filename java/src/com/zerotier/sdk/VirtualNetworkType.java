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
 * Virtual network type codes
 *
 * Defined in ZeroTierOne.h as ZT_VirtualNetworkType
 */
public enum VirtualNetworkType {

    /**
     * Private networks are authorized via certificates of membership
     */
    NETWORK_TYPE_PRIVATE(0),

    /**
     * Public networks have no access control -- they'll always be AUTHORIZED
     */
    NETWORK_TYPE_PUBLIC(1);

    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private final int id;

    VirtualNetworkType(int id) {
        this.id = id;
    }

    public static VirtualNetworkType fromInt(int id) {
        switch (id) {
            case 0:
                return NETWORK_TYPE_PRIVATE;
            case 1:
                return NETWORK_TYPE_PUBLIC;
            default:
                throw new RuntimeException("Unhandled value: " + id);
        }
    }
}
