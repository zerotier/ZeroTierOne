/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifndef _ZT_VERSION_H
#define _ZT_VERSION_H

/**
 * Major version
 */
#define ZEROTIER_ONE_VERSION_MAJOR 1

/**
 * Minor version
 */
#define ZEROTIER_ONE_VERSION_MINOR 16

/**
 * Revision
 */
#define ZEROTIER_ONE_VERSION_REVISION 2

/**
 * Build version
 *
 * This starts at 0 for each major.minor.rev tuple and can be incremented
 * to force a minor update without an actual version number change. It's
 * not part of the actual release version number.
 */
#define ZEROTIER_ONE_VERSION_BUILD 0

#ifndef ZT_BUILD_ARCHITECTURE
#define ZT_BUILD_ARCHITECTURE 0
#endif
#ifndef ZT_BUILD_PLATFORM
#define ZT_BUILD_PLATFORM 0
#endif

#define _ZT_STR_HELPER(x)		 #x
#define _ZT_STR(x)				 _ZT_STR_HELPER(x)
#define ZEROTIER_ONE_VERSION_STR _ZT_STR(ZEROTIER_ONE_VERSION_MAJOR) "." _ZT_STR(ZEROTIER_ONE_VERSION_MINOR) "." _ZT_STR(ZEROTIER_ONE_VERSION_REVISION)
#define ZEROTIER_ONE_NAME		 "zerotier-one"

#endif
