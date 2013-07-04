/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_LAUNCHER_H
#define _ZT_LAUNCHER_H

#define ZT_LAUNCHER_VERSION_MAJOR 0
#define ZT_LAUNCHER_VERSION_MINOR 0
#define ZT_LAUNCHER_VERSION_REVISION 1

/* Default locations of data directories on operating systems */
#ifdef __APPLE__ /* MacOS likes purdy paths, none of this 1970s /var stuff */
#define ZT_DEFAULT_HOME "/Library/Application Support/ZeroTier/One"
#else
#ifdef _WIN32    /* Winbloze */
need windoze;
#else            /* Unix/Linux */
#define ZT_DEFAULT_HOME "/var/lib/zerotier-one"
#endif
#endif

/**
 * Normal termination
 * 
 * This causes the launcher too to exit normally.
 */
#define ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION 0

/**
 * Terminated for upgrade
 * 
 * This tells the launcher that an upgrade may be available, so a scan for
 * newer executables should be performed followed by a restart.
 */
#define ZT_EXEC_RETURN_VALUE_TERMINATED_FOR_UPGRADE 1

/**
 * Terminated but should be restarted
 * 
 * This simply tells the launcher to restart the executable. Possible
 * reasons include the need to change a config parameter that requires restart.
 */
#define ZT_EXEC_RETURN_VALUE_PLEASE_RESTART 2

/**
 * Unrecoverable error
 * 
 * This tells the launcher to exit after possibly sending an error report to
 * ZeroTier if the user has this option enabled.
 */
#define ZT_EXEC_RETURN_VALUE_UNRECOVERABLE_ERROR 3

/**
 * Used on Unix systems to return from forked sub-process if exec fails
 */
#define ZT_EXEC_RETURN_VALUE_NO_BINARY 4

#endif
