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

package com.zerotierone.sdk;

public enum ResultCode {

	RESULT_OK(0),
	RESULT_FATAL_ERROR_OUT_OF_MEMORY(1),
	RESULT_FATAL_ERROR_DATA_STORE_FAILED(2),
	RESULT_FATAL_ERROR_INTERNAL(3),
	RESULT_ERROR_NETWORK_NOT_FOUND(1000);
	
	private final int id;
    ResultCode(int id) { this.id = id; }
    public int getValue() { return id; }

    public boolean isFatal(int id) {
    	return (id > 0 && id < 1000);
    }
}