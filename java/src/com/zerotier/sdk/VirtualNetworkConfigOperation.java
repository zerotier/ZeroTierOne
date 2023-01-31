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
 * Virtual network configuration update type
 *
 * Defined in ZeroTierOne.h as ZT_VirtualNetworkConfigOperation
 */
public enum VirtualNetworkConfigOperation {

    /**
     * Network is coming up (either for the first time or after service restart)
     */
    VIRTUAL_NETWORK_CONFIG_OPERATION_UP(1),

    /**
     * Network configuration has been updated
     */
    VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE(2),

    /**
     * Network is going down (not permanently)
     */
    VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN(3),

    /**
     * Network is going down permanently (leave/delete)
     */
    VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY(4);

    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private final int id;

    VirtualNetworkConfigOperation(int id) {
        this.id = id;
    }

    public static VirtualNetworkConfigOperation fromInt(int id) {
        switch (id) {
            case 1:
                return VIRTUAL_NETWORK_CONFIG_OPERATION_UP;
            case 2:
                return VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE;
            case 3:
                return VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN;
            case 4:
                return VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY;
            default:
                throw new RuntimeException("Unhandled value: " + id);
        }
    }
}
