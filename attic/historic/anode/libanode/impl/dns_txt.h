/* libanode: the Anode C reference implementation
 * Copyright (C) 2009-2010 Adam Ierymenko <adam.ierymenko@gmail.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ANODE_DNS_TXT_H
#define _ANODE_DNS_TXT_H

/**
 * Synchronous TXT resolver routine
 *
 * Error codes:
 *  1 - I/O error
 *  2 - Invalid response
 *  3 - TXT record not found
 *  4 - Destination buffer too small for result
 *
 * @param host Host name
 * @param txt Buffer to store TXT result
 * @param txt_len Size of buffer
 * @return Zero on success, special error code on failure
 */
int Anode_sync_resolve_txt(const char *host,char *txt,unsigned int txt_len);

#endif

