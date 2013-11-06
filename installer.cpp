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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "node/Constants.hpp"

#include "version.h"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>
#include <wchar.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#endif

#include "ext/lz4/lz4.h"
#include "ext/lz4/lz4hc.h"

// Include generated binaries -------------------------------------------------

// zerotier-one binary (or zerotier-one.exe for Windows)
#include "installer-build/zerotier-one.build.c"

// Unix uninstall script
#ifdef __UNIX_LIKE__
#include "installer-build/uninstall.sh.build.c"
#endif

// Linux init.d script
#ifdef __LINUX__
#include "installer-build/redhat__init.d__zerotier-one.build.c"
#include "installer-build/debian__init.d__zerotier-one.build.c"
#endif

// Apple Tap device driver
#ifdef __APPLE__
#include "installer-build/tap-mac__tap.build.c"
#include "installer-build/tap-mac__Info.plist.build.c"
#endif

// Windows Tap device drivers
#ifdef __WINDOWS__
#include "installer-build/tap-windows__x64__ztTap100.sys.build.c"
#include "installer-build/tap-windows__x64__ztTap100.inf.build.c"
#include "installer-build/tap-windows__x86__ztTap100.sys.build.c"
#include "installer-build/tap-windows__x86__ztTap100.inf.build.c"
#include "installer-build/tap-windows__devcon32.exe.build.c"
#include "installer-build/tap-windows__devcon64.exe.build.c"
#endif

// ----------------------------------------------------------------------------

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
}
