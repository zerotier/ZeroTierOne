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

public interface DataStoreGetListener {

    /**
     * Function to get an object from the data store
     *
     * <p>Object names can contain forward slash (/) path separators. They will
     * never contain .. or backslash (\), so this is safe to map as a Unix-style
     * path if the underlying storage permits. For security reasons we recommend
     * returning errors if .. or \ are used.</p>
     *
     * <p>The function must return the actual number of bytes read. If the object
     * doesn't exist, it should return -1. -2 should be returned on other errors
     * such as errors accessing underlying storage.</p>
     *
     * <p>If the read doesn't fit in the buffer, the max number of bytes should be
     * read. The caller may call the function multiple times to read the whole
     * object.</p>
     *
     * @param name Name of the object in the data store
     * @param out_buffer buffer to put the object in
     * @return size of the object
     */
    public long onDataStoreGet(
            String name,
            byte[] out_buffer);
}
