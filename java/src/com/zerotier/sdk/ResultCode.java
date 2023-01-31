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
 * Function return code: OK (0) or error results
 *
 * <p>Use {@link ResultCode#isFatal) to check for a fatal error. If a fatal error
 * occurs, the node should be considered to not be working correctly. These
 * indicate serious problems like an inaccessible data store or a compile
 * problem.</p>
 *
 * Defined in ZeroTierOne.h as ZT_ResultCode
 */
public enum ResultCode {

    /**
     * Operation completed normally
     */
	RESULT_OK(0),

    // Fatal errors (>=100, <1000)
    /**
     * Ran out of memory
     */
	RESULT_FATAL_ERROR_OUT_OF_MEMORY(100),

    /**
     * Data store is not writable or has failed
     */
	RESULT_FATAL_ERROR_DATA_STORE_FAILED(101),

    /**
     * Internal error (e.g. unexpected exception indicating bug or build problem)
     */
	RESULT_FATAL_ERROR_INTERNAL(102),

    // non-fatal errors

    /**
     * Network ID not valid
     */
	RESULT_ERROR_NETWORK_NOT_FOUND(1000),

    RESULT_ERROR_UNSUPPORTED_OPERATION(1001),

    RESULT_ERROR_BAD_PARAMETER(1002);

    private final int id;

    ResultCode(int id) {
        this.id = id;
    }

    public static ResultCode fromInt(int id) {
        switch (id) {
            case 0:
                return RESULT_OK;
            case 100:
                return RESULT_FATAL_ERROR_OUT_OF_MEMORY;
            case 101:
                return RESULT_FATAL_ERROR_DATA_STORE_FAILED;
            case 102:
                return RESULT_FATAL_ERROR_INTERNAL;
            case 1000:
                return RESULT_ERROR_NETWORK_NOT_FOUND;
            case 1001:
                return RESULT_ERROR_UNSUPPORTED_OPERATION;
            case 1002:
                return RESULT_ERROR_BAD_PARAMETER;
            default:
                throw new RuntimeException("Unhandled value: " + id);
        }
    }

    public boolean isFatal(int id) {
    	return (id > 100 && id < 1000);
    }
}
