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

public interface DataStorePutListener {

    /**
     * Function to store an object in the data store
     *
     * <p>If secure is true, the file should be set readable and writable only
     * to the user running ZeroTier One. What this means is platform-specific.</p>
     *
     * <p>Name semantics are the same as {@link DataStoreGetListener}. This must return 
     * zero on success. You can return any OS-specific error code on failure, as these
     * may be visible in logs or error messages and might aid in debugging.</p>
     *
     * @param name Object name
     * @param buffer data to store
     * @param secure set to user read/write only.
     * @return 0 on success.
     */
    int onDataStorePut(
        String name,
        byte[] buffer,
        boolean secure);

    /**
     * Function to delete an object from the data store
     * 
     * @param name Object name
     * @return 0 on success.
     */
    int onDelete(
        String name);
}
